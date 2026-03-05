use crate::project::ProjectFileManager;
use chrono::{NaiveDate, NaiveDateTime, NaiveTime};
use serde::{Deserialize, Serialize};
use std::fs;
use std::future::Future;
use std::path::{Path, PathBuf};
use std::pin::Pin;
use std::process::Command;
use tracing::{debug, info};
use yup_oauth2::{
    InstalledFlowAuthenticator, InstalledFlowReturnMethod,
    authenticator_delegate::InstalledFlowDelegate, read_application_secret,
};

pub const GOOGLE_REQUIRED_SCOPES: [&str; 4] = [
    "https://www.googleapis.com/auth/photoslibrary",
    "https://www.googleapis.com/auth/photoslibrary.readonly.appcreateddata",
    "https://www.googleapis.com/auth/photoslibrary.appendonly",
    "https://www.googleapis.com/auth/drive",
];

/// Represents the access token response from Google OAuth
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GoogleTokenData {
    pub scopes: Vec<String>,
    pub token: GoogleToken,
}

/// The actual token information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GoogleToken {
    pub access_token: String,
    pub refresh_token: Option<String>,
    pub expires_at: Option<Vec<i64>>,
    pub id_token: Option<String>,
}

/// Custom delegate that opens browser automatically on macOS/Linux/Windows
struct BrowserOpeningDelegate;

impl InstalledFlowDelegate for BrowserOpeningDelegate {
    fn present_user_url<'a>(
        &'a self,
        url: &'a str,
        _need_code: bool,
    ) -> Pin<Box<dyn Future<Output = Result<String, String>> + Send + 'a>> {
        Box::pin(async move {
            info!("Opening browser for OAuth URL: {}", url);

            #[cfg(target_os = "macos")]
            let open_cmd = "open";
            #[cfg(target_os = "linux")]
            let open_cmd = "xdg-open";
            #[cfg(target_os = "windows")]
            let open_cmd = "start";

            match Command::new(open_cmd).arg(url).status() {
                Ok(status) if status.success() => {
                    info!("Browser opened successfully");
                }
                Ok(_) | Err(_) => {
                    eprintln!(
                        "Unable to open browser automatically. Please navigate to: {}",
                        url
                    );
                }
            }

            Ok(String::new())
        })
    }
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub enum GoogleAuthStatus {
    MissingCredentials,
    NeedsLogin { credentials_path: PathBuf },
    LoggedIn { credentials_path: PathBuf },
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct GoogleCredentialsInfo {
    pub project_id: String,
    pub client_id: String,
    pub client_secret: String,
    pub auth_uri: String,
    pub token_uri: String,
    pub auth_provider_x509_cert_url: String,
    pub redirect_uris: Vec<String>,
}

#[allow(dead_code)]
#[derive(Debug, Deserialize)]
struct GoogleCredentialsFile {
    installed: Option<GoogleInstalledCredentials>,
    web: Option<GoogleInstalledCredentials>,
}

#[allow(dead_code)]
#[derive(Debug, Deserialize)]
struct GoogleInstalledCredentials {
    project_id: String,
    client_id: String,
    client_secret: String,
    auth_uri: String,
    token_uri: String,
    auth_provider_x509_cert_url: String,
    redirect_uris: Vec<String>,
}

pub struct GoogleAuthFileManager<'a> {
    // we need a reference to the project manager to get the credentials path and token path
    project_manager: &'a ProjectFileManager,
}

impl GoogleAuthFileManager<'_> {
    pub fn new(project_manager: &ProjectFileManager) -> GoogleAuthFileManager<'_> {
        GoogleAuthFileManager { project_manager }
    }

    /// Validates the credentials.json file and returns the parsed information
    pub fn validate_google_credentials_file(
        file_path: &Path,
    ) -> Result<GoogleCredentialsInfo, Box<dyn std::error::Error>> {
        debug!("Validating Google credentials file: {:?}", file_path);

        if !file_path.exists() {
            return Err("credentials.json file does not exist".into());
        }
        if !file_path.is_file() {
            return Err("credentials path is not a file".into());
        }

        let raw = fs::read_to_string(file_path)?;
        let parsed: GoogleCredentialsFile = serde_json::from_str(&raw)
            .map_err(|e| format!("Invalid credentials.json format: {e}"))?;

        let credentials = parsed
            .installed
            .or(parsed.web)
            .ok_or("Missing 'installed' or 'web' section in credentials.json")?;

        if credentials.project_id.trim().is_empty() {
            return Err("credentials.json: 'project_id' is missing or empty".into());
        }
        if credentials.client_id.trim().is_empty() {
            return Err("credentials.json: 'client_id' is missing or empty".into());
        }
        if credentials.client_secret.trim().is_empty() {
            return Err("credentials.json: 'client_secret' is missing or empty".into());
        }
        if credentials.auth_uri.trim().is_empty() {
            return Err("credentials.json: 'auth_uri' is missing or empty".into());
        }
        if credentials.token_uri.trim().is_empty() {
            return Err("credentials.json: 'token_uri' is missing or empty".into());
        }
        if credentials.auth_provider_x509_cert_url.trim().is_empty() {
            return Err(
                "credentials.json: 'auth_provider_x509_cert_url' is missing or empty".into(),
            );
        }
        if credentials.redirect_uris.is_empty() {
            return Err("credentials.json: 'redirect_uris' is missing or empty".into());
        }

        if credentials
            .redirect_uris
            .iter()
            .any(|uri| uri.trim().is_empty())
        {
            return Err("credentials.json: contains an empty redirect uri".into());
        }

        info!(
            "Google credentials file validated successfully for project: {}",
            credentials.project_id
        );

        Ok(GoogleCredentialsInfo {
            project_id: credentials.project_id,
            client_id: credentials.client_id,
            client_secret: credentials.client_secret,
            auth_uri: credentials.auth_uri,
            token_uri: credentials.token_uri,
            auth_provider_x509_cert_url: credentials.auth_provider_x509_cert_url,
            redirect_uris: credentials.redirect_uris,
        })
    }

    /// Gets a valid access token, refreshing if necessary
    pub fn perform_google_oauth_login(
        token_path: &Path,
        credentials_path: &Path,
    ) -> Result<PathBuf, Box<dyn std::error::Error>> {
        let credentials_path = credentials_path.to_path_buf();
        let runtime = tokio::runtime::Runtime::new()?;
        runtime.block_on(async move {
            let secret = read_application_secret(&credentials_path).await?;

            let authenticator = InstalledFlowAuthenticator::builder(
                secret,
                InstalledFlowReturnMethod::HTTPRedirect,
            )
            .flow_delegate(Box::new(BrowserOpeningDelegate))
            .persist_tokens_to_disk(token_path)
            .build()
            .await?;

            authenticator.token(&GOOGLE_REQUIRED_SCOPES).await?;
            Ok::<(), Box<dyn std::error::Error>>(())
        })?;

        info!(
            "Google OAuth login completed and token saved to: {}",
            token_path.to_string_lossy()
        );
        Ok(token_path.to_path_buf())
    }

    /// Gets the current Google authentication status based on the presence of credentials and token files
    pub(crate) fn get_google_auth_status(
        &self,
    ) -> Result<GoogleAuthStatus, Box<dyn std::error::Error>> {
        let credentials_path = self.project_manager.get_credentials_path();
        let token_path = self.project_manager.get_token_path();
        if !credentials_path.exists() {
            return Ok(GoogleAuthStatus::MissingCredentials);
        }

        if !token_path.exists() {
            Ok(GoogleAuthStatus::NeedsLogin { credentials_path })
        } else {
            Ok(GoogleAuthStatus::LoggedIn { credentials_path })
        }
    }

    /// Gets a valid access token, refreshing if necessary
    pub(crate) fn get_valid_access_token(
        &self,
        token_path: &Path,
    ) -> Result<String, Box<dyn std::error::Error>> {
        // Check if token is expired
        if self.is_token_expired(token_path).unwrap_or(true) {
            info!("Token is expired or missing, attempting to refresh...");
            // Try to refresh - if it fails, we'll still return what we have
            match Self::perform_google_oauth_login(
                token_path,
                &self.project_manager.get_credentials_path(),
            ) {
                Ok(new_token) => {
                    info!("Token refreshed successfully");
                    return self.get_access_token(&new_token);
                }
                Err(e) => {
                    debug!("Token refresh failed: {}, using existing token", e);
                    // Fall back to existing token
                }
            }
        }

        self.get_access_token(token_path)
    }

    /// Checks if the token has expired
    pub(crate) fn is_token_expired(
        &self,
        token_path: &Path,
    ) -> Result<bool, Box<dyn std::error::Error>> {
        match self.get_token_expiration(token_path) {
            Ok(expiration_time) => {
                let now = chrono::Utc::now().naive_utc();
                Ok(now >= expiration_time)
            }
            Err(e) => {
                debug!(
                    "Could not determine token expiration: {}, assuming expired",
                    e
                );
                Ok(true)
            }
        }
    }

    /// Checks if the token is valid (has an access_token)
    #[allow(dead_code)]
    pub(crate) fn is_token_valid(&self, token_path: &Path) -> bool {
        match self.read_token(token_path) {
            Ok(data) => !data.token.access_token.is_empty(),
            Err(_) => false,
        }
    }

    /// Gets the access token from a token file
    pub(crate) fn get_access_token(
        &self,
        token_path: &Path,
    ) -> Result<String, Box<dyn std::error::Error>> {
        let token_data = self.read_token(token_path)?;
        Ok(token_data.token.access_token)
    }

    /// Reads a token file and returns the GoogleTokenData
    /// The token file is expected to be a JSON array with one element
    fn read_token(&self, token_path: &Path) -> Result<GoogleTokenData, Box<dyn std::error::Error>> {
        if !token_path.exists() {
            return Err(format!("Token file not found: {}", token_path.display()).into());
        }

        let token_json = fs::read_to_string(token_path)?;

        // The token file is a JSON array with one element
        let tokens: Vec<GoogleTokenData> = serde_json::from_str(&token_json)?;

        if tokens.is_empty() {
            return Err("Token file is empty".into());
        }

        Ok(tokens[0].clone())
    }

    /// Gets the token expiration time from the token file
    fn get_token_expiration(
        &self,
        token_path: &Path,
    ) -> Result<NaiveDateTime, Box<dyn std::error::Error>> {
        let token_data = self.read_token(token_path)?;
        let expires_at = token_data
            .token
            .expires_at
            .ok_or("Token does not have expires_at field")?;
        if expires_at.len() < 6 {
            return Err("expires_at field does not have enough components".into());
        }

        let year = expires_at[0] as i32;
        let day_of_year = expires_at[1] as u32; // day number in the year (1-366)
        let hour = expires_at[2] as u32;
        let min = expires_at[3] as u32;
        let sec = expires_at[4] as u32;
        let nano = expires_at[5] as u32;

        let date = NaiveDate::from_yo_opt(year, day_of_year).expect("Giorno dell'anno non valido");
        let time = NaiveTime::from_hms_nano_opt(hour, min, sec, nano).expect("Orario non valido");
        let dt = NaiveDateTime::new(date, time);
        Ok(dt)
    }
}
