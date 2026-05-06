use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use tracing::{error, info};

/// Status del download/processamento di una foto
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PhotoSyncStatus {
    /// In attesa di essere scaricata
    #[serde(rename = "pending")]
    Pending,
    /// In corso di scaricamento
    #[serde(rename = "downloading")]
    Downloading,
    /// In attesa di processamento
    #[serde(rename = "downloaded")]
    Downloaded,
    /// In corso di processamento
    #[serde(rename = "processing")]
    Processing,
    /// Processato con successo
    #[serde(rename = "processed")]
    Processed,
    /// In attesa di upload su Drive
    #[serde(rename = "ready_for_upload")]
    ReadyForUpload,
    /// Uploadato su Drive
    #[serde(rename = "uploaded")]
    Uploaded,
    /// Errore durante il processo
    #[serde(rename = "failed")]
    Failed(String),
}

impl Default for PhotoSyncStatus {
    fn default() -> Self {
        PhotoSyncStatus::Pending
    }
}

/// Traccia lo stato di una foto singola nell'album
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhotoSyncState {
    /// ID media item dall'album di Google Photos
    pub media_item_id: String,
    /// Percorso locale del file scaricato (se disponibile)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub local_path: Option<PathBuf>,
    /// Hash del file (SHA256) per rilevare duplicati
    #[serde(skip_serializing_if = "Option::is_none")]
    pub file_hash: Option<String>,
    /// Nome del file processato caricato su Drive (se disponibile)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub uploaded_file_name: Option<String>,
    /// ID del file su Google Drive (se uploadato)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub drive_file_id: Option<String>,
    /// Status attuale
    pub status: PhotoSyncStatus,
    /// Timestamp dell'ultimo update (ISO 8601)
    #[serde(skip_serializing_if = "Option::is_none")]
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
    /// Album ID di Google Photos
    pub album_id: String,
    /// ID della cartella Drive di output
    pub drive_folder_id: String,
    /// Mappa: media_item_id → stato foto
    pub photos: HashMap<String, PhotoSyncState>,
    /// Timestamp dell'ultimo sync (ISO 8601)
    pub last_sync: Option<String>,
}

impl AlbumSyncState {
    pub fn new(album_id: String, drive_folder_id: String) -> Self {
        Self {
            album_id,
            drive_folder_id,
            photos: HashMap::new(),
            last_sync: None,
        }
    }

    /// Aggiorna last_sync al momento attuale
    pub fn mark_sync_complete(&mut self) {
        self.last_sync = Some(chrono::Utc::now().to_rfc3339());
    }

    /// Ritorna le foto non ancora scaricate
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

    /// Ritorna le foto scaricate ma non ancora processate
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

    /// Ritorna le foto processate ma non ancora uploadate
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

    /// Ritorna le foto fallite
    pub fn get_failed(&self) -> Vec<&PhotoSyncState> {
        self.photos
            .values()
            .filter(|p| matches!(p.status, PhotoSyncStatus::Failed(_)))
            .collect()
    }
}

/// Gestisce la persistenza dello stato di sincronizzazione
pub struct SyncStateManager {
    db_path: PathBuf,
}

impl SyncStateManager {
    pub fn new(project_dir: &Path) -> Self {
        let db_path = project_dir.join(".photoframe_sync.json");
        Self { db_path }
    }

    /// Carica lo stato di sincronizzazione per un album
    pub fn load_album_state(
        &self,
        album_id: &str,
        drive_folder_id: &str,
    ) -> Result<AlbumSyncState, String> {
        if !self.db_path.exists() {
            info!(
                "Sync DB not found, creating new album state for {}",
                album_id
            );
            return Ok(AlbumSyncState::new(
                album_id.to_string(),
                drive_folder_id.to_string(),
            ));
        }

        let content = fs::read_to_string(&self.db_path)
            .map_err(|e| format!("Failed to read sync DB: {}", e))?;
        let all_albums: Vec<AlbumSyncState> = serde_json::from_str(&content)
            .map_err(|e| format!("Failed to parse sync DB: {}", e))?;

        Ok(all_albums
            .into_iter()
            .find(|s| s.album_id == album_id && s.drive_folder_id == drive_folder_id)
            .unwrap_or_else(|| {
                AlbumSyncState::new(album_id.to_string(), drive_folder_id.to_string())
            }))
    }

    /// Salva lo stato di sincronizzazione per un album
    pub fn save_album_state(&self, state: &AlbumSyncState) -> Result<(), String> {
        // Carica lo stato globale
        let mut all_albums = if self.db_path.exists() {
            let content = fs::read_to_string(&self.db_path)
                .map_err(|e| format!("Failed to read sync DB: {}", e))?;
            serde_json::from_str(&content).unwrap_or_default()
        } else {
            Vec::new()
        };

        // Aggiorna o aggiungi lo stato dell'album
        if let Some(existing) = all_albums.iter_mut().find(|s| s.album_id == state.album_id) {
            *existing = state.clone();
        } else {
            all_albums.push(state.clone());
        }

        // Salva il tutto
        let json = serde_json::to_string_pretty(&all_albums)
            .map_err(|e| format!("Failed to serialize sync DB: {}", e))?;
        fs::write(&self.db_path, json).map_err(|e| format!("Failed to write sync DB: {}", e))?;

        info!(
            "Saved sync state for album {} to {:?}",
            state.album_id, self.db_path
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

        let content = fs::read_to_string(&self.db_path)
            .map_err(|e| format!("Failed to read sync DB: {}", e))?;
        let all_albums: Vec<AlbumSyncState> = serde_json::from_str(&content)
            .map_err(|e| format!("Failed to parse sync DB: {}", e))?;

        for album in all_albums {
            if album.album_id == album_id {
                if let Some(photo) = album.photos.get(media_item_id) {
                    return Ok(matches!(
                        photo.status,
                        PhotoSyncStatus::Uploaded | PhotoSyncStatus::Processed
                    ));
                }
            }
        }

        Ok(false)
    }

    /// Ottiene il DB path per debug
    pub fn db_path(&self) -> &Path {
        &self.db_path
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_photo_sync_state_creation() {
        let state = PhotoSyncState::new("media_123".to_string());
        assert_eq!(state.media_item_id, "media_123");
        assert_eq!(state.status, PhotoSyncStatus::Pending);
        assert!(state.local_path.is_none());
    }

    #[test]
    fn test_photo_sync_state_builder() {
        let state = PhotoSyncState::new("media_123".to_string())
            .with_local_path(PathBuf::from("/tmp/photo.jpg"))
            .with_file_hash("abc123".to_string());

        assert_eq!(state.local_path, Some(PathBuf::from("/tmp/photo.jpg")));
        assert_eq!(state.file_hash, Some("abc123".to_string()));
    }

    #[test]
    fn test_album_sync_state_creation() {
        let state = AlbumSyncState::new("album_456".to_string(), "folder_789".to_string());
        assert_eq!(state.album_id, "album_456");
        assert_eq!(state.drive_folder_id, "folder_789");
        assert!(state.photos.is_empty());
    }
}
