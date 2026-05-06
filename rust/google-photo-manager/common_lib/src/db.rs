use rusqlite::{Connection, OptionalExtension, params};
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};
use tracing::info;

/// Status del download/processamento di una foto
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PhotoSyncStatus {
    #[serde(rename = "pending")]
    Pending,
    #[serde(rename = "downloading")]
    Downloading,
    #[serde(rename = "downloaded")]
    Downloaded,
    #[serde(rename = "processing")]
    Processing,
    #[serde(rename = "processed")]
    Processed,
    #[serde(rename = "ready_for_upload")]
    ReadyForUpload,
    #[serde(rename = "uploaded")]
    Uploaded,
    #[serde(rename = "failed")]
    Failed(String),
}

impl PhotoSyncStatus {
    fn to_string(&self) -> String {
        match self {
            Self::Pending => "pending".to_string(),
            Self::Downloading => "downloading".to_string(),
            Self::Downloaded => "downloaded".to_string(),
            Self::Processing => "processing".to_string(),
            Self::Processed => "processed".to_string(),
            Self::ReadyForUpload => "ready_for_upload".to_string(),
            Self::Uploaded => "uploaded".to_string(),
            Self::Failed(msg) => format!("failed:{}", msg),
        }
    }

    fn from_string(s: &str) -> Self {
        match s {
            "pending" => Self::Pending,
            "downloading" => Self::Downloading,
            "downloaded" => Self::Downloaded,
            "processing" => Self::Processing,
            "processed" => Self::Processed,
            "ready_for_upload" => Self::ReadyForUpload,
            "uploaded" => Self::Uploaded,
            s if s.starts_with("failed:") => Self::Failed(s[7..].to_string()),
            _ => Self::Pending,
        }
    }
}

impl Default for PhotoSyncStatus {
    fn default() -> Self {
        Self::Pending
    }
}

/// Traccia lo stato di una foto singola nell'album
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhotoSyncState {
    pub media_item_id: String,
    pub local_path: Option<PathBuf>,
    pub file_hash: Option<String>,
    pub uploaded_file_name: Option<String>,
    pub drive_file_id: Option<String>,
    pub status: PhotoSyncStatus,
    pub last_updated: Option<String>,
}

impl PhotoSyncState {
    pub fn new(media_item_id: String) -> Self {
        Self {
            media_item_id,
            local_path: None,
            file_hash: None,
            uploaded_file_name: None,
            drive_file_id: None,
            status: PhotoSyncStatus::Pending,
            last_updated: Some(chrono::Utc::now().to_rfc3339()),
        }
    }

    pub fn with_local_path(mut self, path: PathBuf) -> Self {
        self.local_path = Some(path);
        self.last_updated = Some(chrono::Utc::now().to_rfc3339());
        self
    }

    pub fn with_file_hash(mut self, hash: String) -> Self {
        self.file_hash = Some(hash);
        self.last_updated = Some(chrono::Utc::now().to_rfc3339());
        self
    }

    pub fn set_status(&mut self, status: PhotoSyncStatus) {
        self.status = status;
        self.last_updated = Some(chrono::Utc::now().to_rfc3339());
    }

    pub fn set_uploaded(&mut self, file_name: String, drive_file_id: String) {
        self.uploaded_file_name = Some(file_name);
        self.drive_file_id = Some(drive_file_id);
        self.status = PhotoSyncStatus::Uploaded;
        self.last_updated = Some(chrono::Utc::now().to_rfc3339());
    }

    pub fn mark_failed(&mut self, reason: String) {
        self.status = PhotoSyncStatus::Failed(reason);
        self.last_updated = Some(chrono::Utc::now().to_rfc3339());
    }
}

/// Traccia lo stato di sincronizzazione di un album intero
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlbumSyncState {
    pub album_id: String,
    pub drive_folder_id: String,
    pub photos: std::collections::HashMap<String, PhotoSyncState>,
    pub last_sync: Option<String>,
}

impl AlbumSyncState {
    pub fn new(album_id: String, drive_folder_id: String) -> Self {
        Self {
            album_id,
            drive_folder_id,
            photos: std::collections::HashMap::new(),
            last_sync: None,
        }
    }

    pub fn mark_sync_complete(&mut self) {
        self.last_sync = Some(chrono::Utc::now().to_rfc3339());
    }

    pub fn get_pending_downloads(&self) -> Vec<&PhotoSyncState> {
        self.photos
            .values()
            .filter(|p| {
                matches!(
                    p.status,
                    PhotoSyncStatus::Pending | PhotoSyncStatus::Downloading
                )
            })
            .collect()
    }

    pub fn get_pending_processing(&self) -> Vec<&PhotoSyncState> {
        self.photos
            .values()
            .filter(|p| {
                matches!(
                    p.status,
                    PhotoSyncStatus::Downloaded | PhotoSyncStatus::Processing
                )
            })
            .collect()
    }

    pub fn get_pending_upload(&self) -> Vec<&PhotoSyncState> {
        self.photos
            .values()
            .filter(|p| {
                matches!(
                    p.status,
                    PhotoSyncStatus::Processed | PhotoSyncStatus::ReadyForUpload
                )
            })
            .collect()
    }

    pub fn get_failed(&self) -> Vec<&PhotoSyncState> {
        self.photos
            .values()
            .filter(|p| matches!(p.status, PhotoSyncStatus::Failed(_)))
            .collect()
    }
}

/// Gestisce la persistenza dello stato di sincronizzazione su SQLite
pub struct SyncStateManager {
    db_path: PathBuf,
}

impl SyncStateManager {
    pub fn new(project_dir: &Path) -> Self {
        let db_path = project_dir.join(".photoframe_sync.db");
        Self { db_path }
    }

    /// Inizializza il database e crea gli schema se necessario
    pub fn init_db(&self) -> Result<(), String> {
        let conn =
            Connection::open(&self.db_path).map_err(|e| format!("Failed to open DB: {}", e))?;

        // Crea tabella album_sync
        conn.execute(
            "CREATE TABLE IF NOT EXISTS album_sync (
                album_id TEXT NOT NULL,
                drive_folder_id TEXT NOT NULL,
                last_sync TEXT,
                PRIMARY KEY (album_id, drive_folder_id)
            )",
            [],
        )
        .map_err(|e| format!("Failed to create album_sync table: {}", e))?;

        // Crea tabella photo_sync
        conn.execute(
            "CREATE TABLE IF NOT EXISTS photo_sync (
                id INTEGER PRIMARY KEY,
                album_id TEXT NOT NULL,
                media_item_id TEXT NOT NULL,
                local_path TEXT,
                file_hash TEXT,
                uploaded_file_name TEXT,
                drive_file_id TEXT,
                status TEXT NOT NULL,
                last_updated TEXT,
                UNIQUE(album_id, media_item_id),
                FOREIGN KEY(album_id) REFERENCES album_sync(album_id)
            )",
            [],
        )
        .map_err(|e| format!("Failed to create photo_sync table: {}", e))?;

        info!("Database initialized at: {:?}", self.db_path);
        Ok(())
    }

    /// Carica lo stato di sincronizzazione per un album
    pub fn load_album_state(
        &self,
        album_id: &str,
        drive_folder_id: &str,
    ) -> Result<AlbumSyncState, String> {
        // Crea DB se non esiste
        if !self.db_path.exists() {
            self.init_db()?;
            return Ok(AlbumSyncState::new(
                album_id.to_string(),
                drive_folder_id.to_string(),
            ));
        }

        let conn =
            Connection::open(&self.db_path).map_err(|e| format!("Failed to open DB: {}", e))?;

        // Assicura che la tabella album_sync esista (nel caso di DB vecchio)
        let _ = conn.execute(
            "CREATE TABLE IF NOT EXISTS album_sync (
                album_id TEXT NOT NULL,
                drive_folder_id TEXT NOT NULL,
                last_sync TEXT,
                PRIMARY KEY (album_id, drive_folder_id)
            )",
            [],
        );

        // Carica Album
        let last_sync: Option<String> = conn
            .query_row(
                "SELECT last_sync FROM album_sync WHERE album_id = ? AND drive_folder_id = ?",
                params![album_id, drive_folder_id],
                |row| row.get(0),
            )
            .optional()
            .map_err(|e| format!("Failed to query album_sync: {}", e))?
            .flatten();

        let mut album_state =
            AlbumSyncState::new(album_id.to_string(), drive_folder_id.to_string());
        album_state.last_sync = last_sync;

        // Carica foto associate
        let mut stmt = conn
            .prepare(
                "SELECT media_item_id, local_path, file_hash, uploaded_file_name, drive_file_id, status, last_updated
                 FROM photo_sync WHERE album_id = ?",
            )
            .map_err(|e| format!("Failed to prepare statement: {}", e))?;

        let photos = stmt
            .query_map(params![album_id], |row| {
                Ok(PhotoSyncState {
                    media_item_id: row.get(0)?,
                    local_path: row.get::<_, Option<String>>(1)?.map(PathBuf::from),
                    file_hash: row.get(2)?,
                    uploaded_file_name: row.get(3)?,
                    drive_file_id: row.get(4)?,
                    status: PhotoSyncStatus::from_string(&row.get::<_, String>(5)?),
                    last_updated: row.get(6)?,
                })
            })
            .map_err(|e| format!("Failed to query photo_sync: {}", e))?;

        for photo_result in photos {
            let photo = photo_result.map_err(|e| format!("Failed to read photo row: {}", e))?;
            album_state
                .photos
                .insert(photo.media_item_id.clone(), photo);
        }

        info!(
            "Loaded album state: {} photos for album {}",
            album_state.photos.len(),
            album_id
        );
        Ok(album_state)
    }

    /// Salva lo stato di sincronizzazione per un album (transazione atomica)
    pub fn save_album_state(&self, state: &AlbumSyncState) -> Result<(), String> {
        if !self.db_path.exists() {
            self.init_db()?;
        }

        let mut conn =
            Connection::open(&self.db_path).map_err(|e| format!("Failed to open DB: {}", e))?;

        let tx = conn
            .transaction()
            .map_err(|e| format!("Failed to start transaction: {}", e))?;

        // Inserisci o aggiorna album
        tx.execute(
            "INSERT OR REPLACE INTO album_sync (album_id, drive_folder_id, last_sync)
             VALUES (?, ?, ?)",
            params![&state.album_id, &state.drive_folder_id, &state.last_sync],
        )
        .map_err(|e| format!("Failed to upsert album: {}", e))?;

        // Inserisci o aggiorna foto
        for photo in state.photos.values() {
            tx.execute(
                "INSERT OR REPLACE INTO photo_sync
                 (album_id, media_item_id, local_path, file_hash, uploaded_file_name, drive_file_id, status, last_updated)
                 VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                params![
                    &state.album_id,
                    &photo.media_item_id,
                    photo.local_path.as_ref().map(|p| p.to_string_lossy().to_string()),
                    &photo.file_hash,
                    &photo.uploaded_file_name,
                    &photo.drive_file_id,
                    photo.status.to_string(),
                    &photo.last_updated,
                ],
            ).map_err(|e| format!("Failed to upsert photo: {}", e))?;
        }

        tx.commit()
            .map_err(|e| format!("Failed to commit transaction: {}", e))?;

        info!(
            "Saved sync state for album {} ({} photos)",
            state.album_id,
            state.photos.len()
        );
        Ok(())
    }

    /// Controlla se una foto è già stata scaricata/processata
    pub fn is_media_item_processed(
        &self,
        album_id: &str,
        media_item_id: &str,
    ) -> Result<bool, String> {
        if !self.db_path.exists() {
            return Ok(false);
        }

        let conn =
            Connection::open(&self.db_path).map_err(|e| format!("Failed to open DB: {}", e))?;

        let status: Option<String> = conn
            .query_row(
                "SELECT status FROM photo_sync WHERE album_id = ? AND media_item_id = ?",
                params![album_id, media_item_id],
                |row| row.get(0),
            )
            .optional()
            .map_err(|e| format!("Failed to query photo status: {}", e))?
            .flatten();

        Ok(if let Some(status_str) = status {
            let status = PhotoSyncStatus::from_string(&status_str);
            matches!(
                status,
                PhotoSyncStatus::Uploaded | PhotoSyncStatus::Processed
            )
        } else {
            false
        })
    }

    pub fn db_path(&self) -> &Path {
        &self.db_path
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn test_photo_sync_state_creation() {
        let state = PhotoSyncState::new("media_123".to_string());
        assert_eq!(state.media_item_id, "media_123");
        assert_eq!(state.status, PhotoSyncStatus::Pending);
        assert!(state.local_path.is_none());
    }

    #[test]
    fn test_album_sync_state_creation() {
        let state = AlbumSyncState::new("album_456".to_string(), "folder_789".to_string());
        assert_eq!(state.album_id, "album_456");
        assert_eq!(state.drive_folder_id, "folder_789");
        assert!(state.photos.is_empty());
    }

    #[test]
    fn test_sqlite_persistence() -> Result<(), Box<dyn std::error::Error>> {
        let temp_dir = TempDir::new()?;
        let manager = SyncStateManager::new(temp_dir.path());

        // Crea stato con una foto
        let mut album_state =
            AlbumSyncState::new("album_123".to_string(), "folder_456".to_string());
        let mut photo = PhotoSyncState::new("media_001".to_string());
        photo.set_status(PhotoSyncStatus::Downloaded);
        photo.local_path = Some(PathBuf::from("/tmp/photo.jpg"));
        album_state.photos.insert("media_001".to_string(), photo);
        album_state.mark_sync_complete();

        // Salva
        manager.init_db()?;
        manager.save_album_state(&album_state)?;

        // Carica e verifica
        let loaded = manager.load_album_state("album_123", "folder_456")?;
        assert_eq!(loaded.photos.len(), 1);
        assert_eq!(
            loaded.photos["media_001"].status,
            PhotoSyncStatus::Downloaded
        );
        assert!(loaded.last_sync.is_some());

        Ok(())
    }
}
