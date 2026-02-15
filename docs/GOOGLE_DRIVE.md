# Google Drive API Documentation

## Overview

The ESP32 Photo Frame project includes a comprehensive Google Drive integration system that provides reliable cloud storage access with advanced caching, streaming architecture, and robust error handling. 

This is how your confi.json should look like, in order to enable the Google Drive dataprovider. 
> For a complete reference about the config.json file [see more here](CONFIG_REFERENCE.md)


```json
{
  ...
  "google_drive_config": {
    "authentication": {
      "service_account_email": "your-service-account@project.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
      "client_id": "your-client-id"
    },
    "drive": {
      "folder_ids": [
        "your-google-drive-folder-id-1",
        "your-google-drive-folder-id-2"
      ],
      "root_ca_path": "/certs/google_root_ca.pem",
      "list_page_size": 100,
      "use_insecure_tls": false
    },
    "caching": {
      "local_path": "/gdrive",
      "toc_max_age_seconds": 604800
    },
    "rate_limiting": {
      "max_requests_per_window": 100,
      "rate_limit_window_seconds": 100,
      "min_request_delay_ms": 500,
      "max_retry_attempts": 3,
      "backoff_base_delay_ms": 5000,
      "max_wait_time_ms": 30000
    }
  },
  ...
}
```


## Google Drive Integration Setup

To use Google Drive with this application, you need to set up a Google Cloud project, create a service account, enable the Drive API, and share the required folders. Follow these steps:

### 1. Create a Google Cloud Project

1. Go to the [Google Cloud Console](https://console.cloud.google.com/).
2. Click the project dropdown at the top and select “New Project.”
3. Enter a name and create the project.

### 2. Enable Google Drive API

1. In your new project, go to “APIs & Services” > “Library.”
2. Search for “Google Drive API.”
3. Click on it and press “Enable.”

### 3. Create a Service Account

1. Go to “APIs & Services” > “Credentials.”
2. Click “Create Credentials” > “Service account.”
3. Enter a name and description, then click “Create and Continue.”
4. (Optional) Assign roles if needed, then click “Done.”
5. In the service account list, click your new account, then go to the “Keys” tab.
6. Click “Add Key” > “Create new key” > “JSON.”
7. Download the JSON file and keep it safe. You’ll need its contents for your configuration.

### 4. Share Google Drive Folders

1. In Google Drive, create or locate the folders you want the application to access.
2. Right-click each folder and select “Share.”
3. Enter the service account email (found in the JSON file under `client_email`).
4. Give at least “Viewer” access (or “Editor” if needed).
5. Click “Send.”

### 5. Get Folder IDs

1. Open the folder in Google Drive.
2. The folder ID is the long string in the URL after `/folders/`.
   - Example: `https://drive.google.com/drive/folders/1A2B3C4D5E6F7G8H9I`
   - The ID is `1A2B3C4D5E6F7G8H9I`
3. Add these IDs to the `folder_ids` property in your configuration.

### 6. Fill in the Configuration

- Use the downloaded JSON file to fill in the required properties (such as `client_email`, `private_key`, etc.).
- Set the `folder_ids` to the IDs of the folders you shared with the service account.

---

**Note:** The service account will only have access to folders that are explicitly shared with it. Make sure to repeat the sharing step for every folder you want the application to access.
