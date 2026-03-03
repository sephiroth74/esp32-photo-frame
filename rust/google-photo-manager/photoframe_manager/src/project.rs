use crate::google_auth::{GoogleAuthFileManager, GoogleAuthStatus};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;
use strum::{AsRefStr, Display, EnumString, IntoStaticStr};
use tracing::{debug, info};

const PROJECT_FILE_NAME: &str = "project.properties";
const TOKEN_FILE_NAME: &str = "token.json";
const CREDENTIALS_FILE_NAME: &str = "credentials.json";

#[derive(Debug, Eq, PartialEq, EnumString, AsRefStr, IntoStaticStr, Hash, Display, Copy, Clone)]
pub(crate) enum ProjectKeys {
    #[strum(to_string = "google.photos.album_id")]
    GooglePhotosAlbumId,
    #[strum(to_string = "google.photos.album_name")]
    GooglePhotosAlbumName,
}

pub struct ProjectFileManager {
    path: PathBuf,
}

impl ProjectFileManager {
    pub fn new(path: &str) -> Result<ProjectFileManager, Box<dyn std::error::Error>> {
        debug!("Creating project at path: {}", path);

        Self::validate_project_path(path)?;

        let project_path = PathBuf::from(path);

        if !project_path.exists() {
            info!("Creating project directory: {}", path);
            fs::create_dir_all(&project_path)?;
        } else if !project_path.is_dir() {
            return Err("Path exists but is not a directory".into());
        }

        Self::initialize_project_files(&project_path)?;

        info!("Project initialized successfully at: {}", path);
        Ok(ProjectFileManager { path: project_path })
    }

    pub fn open(path: &str) -> Result<ProjectFileManager, Box<dyn std::error::Error>> {
        debug!("Opening project at path: {}", path);

        if path.trim().is_empty() {
            return Err("Project path cannot be empty".into());
        }

        let project_path = PathBuf::from(path);
        if !project_path.exists() {
            return Err("Project directory does not exist".into());
        }
        if !project_path.is_dir() {
            return Err("Path exists but is not a directory".into());
        }

        let project_file = project_path.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            return Err("project.properties not found in project directory".into());
        }
        if !project_file.is_file() {
            return Err("project.properties is not a file".into());
        }

        let project_manager = ProjectFileManager {
            path: project_path.clone(),
        };
        project_manager.validate_project_file(&project_file)?;

        info!("Project validated successfully at: {}", path);
        Ok(ProjectFileManager { path: project_path })
    }

    #[allow(dead_code)]
    pub fn get_project_path(&self) -> &Path {
        &self.path
    }

    pub fn get_google_auth(&'_ self) -> GoogleAuthFileManager<'_> {
        GoogleAuthFileManager::new(&self)
    }

    fn validate_project_file(&self, _file_path: &Path) -> Result<(), Box<dyn std::error::Error>> {
        // TODO: Add concrete validation rules when project.properties fields are defined
        Ok(())
    }

    fn validate_project_path(path: &str) -> Result<(), Box<dyn std::error::Error>> {
        debug!("Validating project path: {}", path);

        if path.trim().is_empty() {
            return Err("Project path cannot be empty".into());
        }

        let project_path = PathBuf::from(path);

        if project_path.exists() {
            if !project_path.is_dir() {
                return Err("Path exists but is not a directory".into());
            }

            let entries: Vec<_> = fs::read_dir(&project_path)?
                .filter_map(|e| e.ok())
                .collect();

            if !entries.is_empty() {
                return Err("Directory is not empty".into());
            }
        }

        debug!("Project path validation successful");
        Ok(())
    }

    fn initialize_project_files(project_dir: &Path) -> Result<(), Box<dyn std::error::Error>> {
        debug!("Initializing project files at: {:?}", project_dir);

        let project_file = project_dir.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            fs::File::create(&project_file)?;
            info!("Created project.properties file");
        }

        Ok(())
    }

    #[allow(dead_code)]
    pub fn get_album_id(&self) -> Option<String> {
        self.get_property(ProjectKeys::GooglePhotosAlbumId)
            .ok()
            .flatten()
    }

    #[allow(dead_code)]
    pub fn get_album_name(&self) -> Option<String> {
        self.get_property(ProjectKeys::GooglePhotosAlbumName)
            .ok()
            .flatten()
    }

    #[allow(dead_code)]
    pub fn has_album_info(&self) -> bool {
        self.get_album_id().is_some() && self.get_album_name().is_some()
    }

    pub fn get_valid_access_token(&self) -> Result<String, Box<dyn std::error::Error>> {
        self.get_google_auth()
            .get_valid_access_token(self.get_token_path().as_path())
    }

    #[allow(dead_code)]
    pub fn perform_google_oauth_login(&self) -> Result<PathBuf, Box<dyn std::error::Error>> {
        GoogleAuthFileManager::perform_google_oauth_login(
            self.get_token_path().as_path(),
            self.get_credentials_path().as_path(),
        )
    }

    pub fn get_google_auth_status(&self) -> Result<GoogleAuthStatus, Box<dyn std::error::Error>> {
        self.get_google_auth().get_google_auth_status()
    }

    pub fn logout_from_google(&self) -> Result<(), Box<dyn std::error::Error>> {
        let token_path = self.get_token_path();
        if token_path.exists() {
            let _ = fs::remove_file(&token_path);
        }
        let credentials_path = self.get_credentials_path();
        if credentials_path.exists() {
            let _ = fs::remove_file(&credentials_path);
        }

        info!("Google auth data removed");
        Ok(())
    }

    pub fn copy_credentials_to_project(
        &self,
        source_path: &Path,
    ) -> Result<PathBuf, Box<dyn std::error::Error>> {
        if !source_path.exists() || !source_path.is_file() {
            return Err("Source credentials file does not exist".into());
        }

        if !self.path.exists() || !self.path.is_dir() {
            return Err("Project directory does not exist".into());
        }

        let dest_path = self.path.join("credentials.json");

        fs::copy(source_path, &dest_path).map_err(|e| {
            Box::<dyn std::error::Error>::from(format!(
                "Failed to copy credentials file to project: {}",
                e
            ))
        })?;

        info!(
            "Credentials file copied to project at: {}",
            dest_path.display()
        );
        Ok(dest_path)
    }

    /// Return the absolute path to the token file, as specified in TOKEN_FILE_NAME
    pub fn get_token_path(&self) -> PathBuf {
        let token_path = PathBuf::from(TOKEN_FILE_NAME);

        if token_path.is_absolute() {
            token_path
        } else {
            self.path.join(token_path)
        }
    }

    pub fn get_credentials_path(&self) -> PathBuf {
        let credentials_path = PathBuf::from(CREDENTIALS_FILE_NAME);

        if credentials_path.is_absolute() {
            credentials_path
        } else {
            self.path.join(credentials_path)
        }
    }

    pub fn get_property(
        &self,
        key: ProjectKeys,
    ) -> Result<Option<String>, Box<dyn std::error::Error>> {
        let project_file = self.path.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            return Ok(None);
        }
        let props = read_properties_map(&project_file)?;
        Ok(props.get(&key).cloned())
    }

    pub fn set_property(
        &self,
        key: ProjectKeys,
        value: &str,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let project_file = self.path.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            return Err("project.properties not found in project directory".into());
        }

        let existing = fs::read_to_string(&project_file).unwrap_or_default();
        let mut lines: Vec<String> = existing.lines().map(|s| s.to_string()).collect();

        let entry = format!("{}={}", key.as_ref(), value);
        if let Some(idx) = lines
            .iter()
            .position(|line| line.trim_start().starts_with(&format!("{}=", key.as_ref())))
        {
            lines[idx] = entry;
        } else {
            lines.push(entry);
        }

        let mut output = lines.join("\n");
        output.push('\n');
        fs::write(&project_file, output)?;
        Ok(())
    }

    pub fn remove_properties(
        &self,
        keys: &[ProjectKeys],
    ) -> Result<(), Box<dyn std::error::Error>> {
        let project_file = self.path.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            return Ok(());
        }

        let mut lines: Vec<String> = fs::read_to_string(&project_file)
            .unwrap_or_default()
            .lines()
            .map(|s| s.to_string())
            .collect();

        lines.retain(|line| {
            let trimmed = line.trim_start();
            !keys
                .iter()
                .any(|key| trimmed.starts_with(&format!("{}=", key.as_ref())))
        });

        let mut output = lines.join("\n");
        output.push('\n');
        fs::write(&project_file, output)?;
        Ok(())
    }

    #[allow(dead_code)]
    fn get_all_properties(
        &self,
    ) -> Result<HashMap<ProjectKeys, String>, Box<dyn std::error::Error>> {
        let project_file = self.path.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            return Ok(HashMap::new());
        }
        read_properties_map(&project_file)
    }
}

pub(crate) fn read_properties_map(
    file_path: &Path,
) -> Result<HashMap<ProjectKeys, String>, Box<dyn std::error::Error>> {
    let content = fs::read_to_string(file_path).unwrap_or_default();
    let mut map = HashMap::new();

    for line in content.lines() {
        let trimmed = line.trim();
        if trimmed.is_empty() || trimmed.starts_with('#') {
            continue;
        }
        if let Some((key, value)) = trimmed.split_once('=') {
            if let Ok(project_key) = ProjectKeys::from_str(key.trim()) {
                map.insert(project_key, value.trim().to_string());
            }
        }
    }

    Ok(map)
}
