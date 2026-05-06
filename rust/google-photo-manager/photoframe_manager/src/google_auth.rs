use crate::project::ProjectFileManager;
use photoframe_common::google_auth as common_auth;
use std::path::{Path, PathBuf};

pub use common_auth::{GoogleAuthStatus, GoogleCredentialsInfo};

pub struct GoogleAuthFileManager<'a> {
    project_manager: &'a ProjectFileManager,
}

impl GoogleAuthFileManager<'_> {
    pub fn new(project_manager: &ProjectFileManager) -> GoogleAuthFileManager<'_> {
        GoogleAuthFileManager { project_manager }
    }

    pub fn validate_google_credentials_file(
        file_path: &Path,
    ) -> Result<GoogleCredentialsInfo, Box<dyn std::error::Error>> {
        common_auth::validate_google_credentials_file(file_path)
    }

    pub fn perform_google_oauth_login(
        token_path: &Path,
        credentials_path: &Path,
    ) -> Result<PathBuf, Box<dyn std::error::Error>> {
        common_auth::perform_google_oauth_login(token_path, credentials_path)
    }

    pub(crate) fn get_google_auth_status(
        &self,
    ) -> Result<GoogleAuthStatus, Box<dyn std::error::Error>> {
        common_auth::get_google_auth_status(
            self.project_manager.get_credentials_path().as_path(),
            self.project_manager.get_token_path().as_path(),
        )
    }

    pub(crate) fn get_valid_access_token(
        &self,
        token_path: &Path,
    ) -> Result<String, Box<dyn std::error::Error>> {
        common_auth::get_valid_access_token(
            token_path,
            self.project_manager.get_credentials_path().as_path(),
        )
    }
}
