use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

const PROJECT_FILE_NAME: &str = "project.properties";
const TOKEN_FILE_NAME: &str = "token.json";
const CREDENTIALS_FILE_NAME: &str = "credentials.json";

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ProjectKeys {
    GooglePhotosAlbumId,
    GooglePhotosAlbumName,
    GoogleDriveFolderId,
    GoogleDriveFolderName,
    BinaryDataPath,
    BinaryDataArguments,
    BinaryDataCron,
}

impl ProjectKeys {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::GooglePhotosAlbumId => "google.photos.album_id",
            Self::GooglePhotosAlbumName => "google.photos.album_name",
            Self::GoogleDriveFolderId => "google.drive.folder_id",
            Self::GoogleDriveFolderName => "google.drive.folder_name",
            Self::BinaryDataPath => "binary.data.path",
            Self::BinaryDataArguments => "binary.data.arguments",
            Self::BinaryDataCron => "binary.data.cron",
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct ProjectConfig {
    pub album_id: Option<String>,
    pub album_name: Option<String>,
    pub drive_folder_id: Option<String>,
    pub drive_folder_name: Option<String>,
    pub binary_path: Option<PathBuf>,
    pub binary_arguments: Option<String>,
    pub cron_frequency: Option<String>,
}

impl ProjectConfig {
    pub fn is_minimally_ready_for_worker(&self) -> bool {
        self.album_id.is_some()
            && self.drive_folder_id.is_some()
            && self.binary_path.is_some()
            && self.binary_arguments.is_some()
    }
}

#[derive(Debug, Clone)]
pub struct ProjectFileManager {
    path: PathBuf,
}

impl ProjectFileManager {
    pub fn open(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        if !path.exists() {
            return Err("Project directory does not exist".into());
        }
        if !path.is_dir() {
            return Err("Project path is not a directory".into());
        }

        let project_file = path.join(PROJECT_FILE_NAME);
        if !project_file.exists() {
            return Err("project.properties not found in project directory".into());
        }
        if !project_file.is_file() {
            return Err("project.properties is not a file".into());
        }

        Ok(Self {
            path: path.to_path_buf(),
        })
    }

    pub fn project_path(&self) -> &Path {
        &self.path
    }

    pub fn token_path(&self) -> PathBuf {
        self.path.join(TOKEN_FILE_NAME)
    }

    pub fn credentials_path(&self) -> PathBuf {
        self.path.join(CREDENTIALS_FILE_NAME)
    }

    pub fn get_property(
        &self,
        key: ProjectKeys,
    ) -> Result<Option<String>, Box<dyn std::error::Error>> {
        let project_file = self.path.join(PROJECT_FILE_NAME);
        let map = read_properties_map(&project_file)?;
        Ok(map.get(key.as_str()).cloned())
    }

    pub fn read_config(&self) -> Result<ProjectConfig, Box<dyn std::error::Error>> {
        let album_id = self.get_property(ProjectKeys::GooglePhotosAlbumId)?;
        let album_name = self.get_property(ProjectKeys::GooglePhotosAlbumName)?;
        let drive_folder_id = self.get_property(ProjectKeys::GoogleDriveFolderId)?;
        let drive_folder_name = self.get_property(ProjectKeys::GoogleDriveFolderName)?;
        let binary_path = self
            .get_property(ProjectKeys::BinaryDataPath)?
            .map(PathBuf::from);
        let binary_arguments = self.get_property(ProjectKeys::BinaryDataArguments)?;
        let cron_frequency = self.get_property(ProjectKeys::BinaryDataCron)?;

        Ok(ProjectConfig {
            album_id,
            album_name,
            drive_folder_id,
            drive_folder_name,
            binary_path,
            binary_arguments,
            cron_frequency,
        })
    }
}

fn read_properties_map(
    project_file: &Path,
) -> Result<HashMap<String, String>, Box<dyn std::error::Error>> {
    let content = fs::read_to_string(project_file)?;
    let mut map = HashMap::new();

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        if let Some((key, value)) = line.split_once('=') {
            map.insert(key.trim().to_string(), value.trim().to_string());
        }
    }

    Ok(map)
}
