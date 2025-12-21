#!/usr/bin/env python3
"""
Test Google Drive Service Account Authentication
This script verifies that the Google Drive credentials are correct without using the Arduino board.
"""

import json
import time
import base64
import hashlib
from pathlib import Path

try:
    import requests
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding
    from cryptography.hazmat.backends import default_backend
except ImportError:
    print("ERROR: Missing required packages. Install with:")
    print("  pip install requests cryptography")
    exit(1)


def load_config():
    """Load configuration from data/config.json"""
    config_path = Path(__file__).parent / "data" / "config.json"
    if not config_path.exists():
        print(f"ERROR: Configuration file not found at {config_path}")
        exit(1)

    with open(config_path, 'r') as f:
        return json.load(f)


def create_jwt(service_account_email, private_key_pem, client_id):
    """Create a JWT token for Google OAuth2"""

    # JWT Header
    header = {
        "alg": "RS256",
        "typ": "JWT"
    }

    # JWT Claim Set
    now = int(time.time())
    claim = {
        "iss": service_account_email,
        "scope": "https://www.googleapis.com/auth/drive.readonly",
        "aud": "https://oauth2.googleapis.com/token",
        "exp": now + 3600,
        "iat": now
    }

    # Encode header and claim
    header_b64 = base64.urlsafe_b64encode(
        json.dumps(header, separators=(',', ':')).encode('utf-8')
    ).rstrip(b'=').decode('utf-8')

    claim_b64 = base64.urlsafe_b64encode(
        json.dumps(claim, separators=(',', ':')).encode('utf-8')
    ).rstrip(b'=').decode('utf-8')

    # Create signature
    message = f"{header_b64}.{claim_b64}".encode('utf-8')

    try:
        # Load private key
        private_key = serialization.load_pem_private_key(
            private_key_pem.encode('utf-8'),
            password=None,
            backend=default_backend()
        )

        # Sign with RS256
        signature = private_key.sign(
            message,
            padding.PKCS1v15(),
            hashes.SHA256()
        )

        signature_b64 = base64.urlsafe_b64encode(signature).rstrip(b'=').decode('utf-8')

        return f"{header_b64}.{claim_b64}.{signature_b64}"

    except Exception as e:
        print(f"ERROR creating JWT: {e}")
        return None


def get_access_token(jwt_token):
    """Exchange JWT for an access token"""

    url = "https://oauth2.googleapis.com/token"
    data = {
        "grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer",
        "assertion": jwt_token
    }

    try:
        response = requests.post(url, data=data, timeout=30)

        if response.status_code == 200:
            return response.json().get('access_token')
        else:
            print(f"ERROR: Failed to get access token")
            print(f"Status: {response.status_code}")
            print(f"Response: {response.text}")
            return None

    except Exception as e:
        print(f"ERROR requesting access token: {e}")
        return None


def test_folder_access(access_token, folder_id):
    """Test access to the Google Drive folder"""

    url = f"https://www.googleapis.com/drive/v3/files"
    params = {
        "q": f"'{folder_id}' in parents and trashed=false",
        "fields": "files(id,name,mimeType,size)",
        "pageSize": 10
    }
    headers = {
        "Authorization": f"Bearer {access_token}"
    }

    try:
        response = requests.get(url, params=params, headers=headers, timeout=30)

        if response.status_code == 200:
            data = response.json()
            files = data.get('files', [])

            print(f"\n✅ SUCCESS: Access to folder granted!")
            print(f"Found {len(files)} files in the folder")

            if files:
                print("\nFirst 10 files:")
                for i, file in enumerate(files[:10], 1):
                    size = int(file.get('size', 0)) if file.get('size') else 0
                    size_kb = size / 1024
                    print(f"  {i}. {file['name']} ({size_kb:.1f} KB)")
            else:
                print("  (folder is empty)")

            return True
        else:
            print(f"\n❌ ERROR: Failed to access folder")
            print(f"Status: {response.status_code}")
            print(f"Response: {response.text}")
            return False

    except Exception as e:
        print(f"\n❌ ERROR accessing folder: {e}")
        return False


def main():
    print("=" * 60)
    print("Google Drive Service Account Authentication Test")
    print("=" * 60)

    # Load configuration
    print("\n[1/4] Loading configuration...")
    config = load_config()

    google_drive = config.get('google_drive', {})
    auth = google_drive.get('authentication', {})
    drive = google_drive.get('drive', {})

    service_account_email = auth.get('service_account_email')
    private_key_pem = auth.get('private_key_pem')
    client_id = auth.get('client_id')
    folder_id = drive.get('folder_id')

    if not all([service_account_email, private_key_pem, client_id, folder_id]):
        print("❌ ERROR: Missing required configuration")
        print(f"  service_account_email: {'✓' if service_account_email else '✗'}")
        print(f"  private_key_pem: {'✓' if private_key_pem else '✗'}")
        print(f"  client_id: {'✓' if client_id else '✗'}")
        print(f"  folder_id: {'✓' if folder_id else '✗'}")
        exit(1)

    print(f"  Service Account: {service_account_email}")
    print(f"  Client ID: {client_id}")
    print(f"  Folder ID: {folder_id}")

    # Create JWT
    print("\n[2/4] Creating JWT token...")
    jwt_token = create_jwt(service_account_email, private_key_pem, client_id)

    if not jwt_token:
        print("❌ Failed to create JWT token")
        exit(1)

    print(f"  JWT token created ({len(jwt_token)} bytes)")

    # Get access token
    print("\n[3/4] Requesting access token...")
    access_token = get_access_token(jwt_token)

    if not access_token:
        print("❌ Failed to get access token")
        exit(1)

    print(f"  ✅ Access token received ({len(access_token)} bytes)")

    # Test folder access
    print("\n[4/4] Testing folder access...")
    success = test_folder_access(access_token, folder_id)

    if success:
        print("\n" + "=" * 60)
        print("✅ ALL TESTS PASSED - Credentials are valid!")
        print("=" * 60)
        exit(0)
    else:
        print("\n" + "=" * 60)
        print("❌ TEST FAILED - Check folder permissions")
        print("=" * 60)
        exit(1)


if __name__ == "__main__":
    main()
