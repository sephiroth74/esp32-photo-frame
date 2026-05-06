use reqwest::blocking::Client;
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;
use tracing::info;

#[derive(Debug, Clone)]
pub struct AlbumMediaItem {
    pub media_item_id: String,
    pub filename: Option<String>,
    pub mime_type: Option<String>,
    pub base_url: Option<String>,
}

#[derive(Debug, Clone)]
pub struct DriveUploadResult {
    pub file_id: String,
    pub file_name: String,
}

#[derive(Debug, Clone)]
pub struct ProcessedPhotoRecord {
    pub media_item_id: String,
    pub output_file_name: String,
}

#[derive(Debug, Clone)]
pub struct SyncBatch {
    pub album_id: String,
    pub drive_folder_id: String,
    pub new_items: Vec<AlbumMediaItem>,
}

pub trait GoogleServices {
    fn ensure_authenticated(&self) -> Result<(), Box<dyn std::error::Error>>;

    fn list_album_media_items(
        &self,
        album_id: &str,
    ) -> Result<Vec<AlbumMediaItem>, Box<dyn std::error::Error>>;

    fn download_media_item(
        &self,
        media_item: &AlbumMediaItem,
        destination: &Path,
    ) -> Result<(), Box<dyn std::error::Error>>;

    fn ensure_drive_folder_exists(
        &self,
        folder_id: &str,
    ) -> Result<bool, Box<dyn std::error::Error>>;

    fn upload_file_to_drive_folder(
        &self,
        folder_id: &str,
        local_file: &Path,
    ) -> Result<DriveUploadResult, Box<dyn std::error::Error>>;
}

/// Concrete implementation of GoogleServices using reqwest blocking client
pub struct GooglePhotosClient {
    access_token: String,
    client: Client,
}

impl GooglePhotosClient {
    pub fn new(access_token: String) -> Self {
        Self {
            access_token,
            client: Client::new(),
        }
    }
}

impl GoogleServices for GooglePhotosClient {
    fn ensure_authenticated(&self) -> Result<(), Box<dyn std::error::Error>> {
        // Token is already provided during initialization
        Ok(())
    }

    fn list_album_media_items(
        &self,
        album_id: &str,
    ) -> Result<Vec<AlbumMediaItem>, Box<dyn std::error::Error>> {
        let url = "https://photoslibrary.googleapis.com/v1/mediaItems:search";

        #[derive(Serialize)]
        struct MediaItemsSearchRequest<'a> {
            #[serde(rename = "albumId")]
            album_id: &'a str,
            #[serde(rename = "pageSize")]
            page_size: u32,
            #[serde(rename = "pageToken")]
            page_token: Option<String>,
        }

        #[derive(Deserialize)]
        struct MediaItemResponse {
            #[serde(default)]
            #[serde(rename = "mediaItems")]
            media_items: Vec<MediaItemDto>,
            #[serde(rename = "nextPageToken")]
            next_page_token: Option<String>,
        }

        #[derive(Deserialize)]
        struct MediaItemDto {
            id: String,
            filename: Option<String>,
            #[serde(rename = "mimeType")]
            mime_type: Option<String>,
            #[serde(rename = "baseUrl")]
            base_url: Option<String>,
        }

        let mut all_items: Vec<AlbumMediaItem> = Vec::new();
        let mut next_page_token: Option<String> = None;

        loop {
            let request_body = MediaItemsSearchRequest {
                album_id,
                page_size: 100,
                page_token: next_page_token.clone(),
            };

            info!(
                "Photos API request: POST {} (album_id={}, page_size={}, page_token_present={})",
                url,
                album_id,
                request_body.page_size,
                request_body.page_token.is_some()
            );

            let response = self
                .client
                .post(url)
                .bearer_auth(&self.access_token)
                .json(&request_body)
                .send()?;

            if !response.status().is_success() {
                let status = response.status();
                let body = response.text().unwrap_or_default();
                return Err(format!("Failed to list media items: {} - {}", status, body).into());
            }

            let body: MediaItemResponse = response.json()?;
            let items = body.media_items.into_iter().map(|dto| AlbumMediaItem {
                media_item_id: dto.id,
                filename: dto.filename,
                mime_type: dto.mime_type,
                base_url: dto.base_url,
            });

            all_items.extend(items);
            next_page_token = body.next_page_token;

            if next_page_token.is_none() {
                break;
            }
        }

        Ok(all_items)
    }

    fn download_media_item(
        &self,
        media_item: &AlbumMediaItem,
        destination: &Path,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let base_url = media_item
            .base_url
            .as_ref()
            .ok_or("Media item has no base_url")?;

        // Download with =w2048 parameter for maximum quality
        let download_url = format!("{}=w2048", base_url);

        let response = self.client.get(&download_url).send()?;

        if !response.status().is_success() {
            return Err(format!("Failed to download media item: {}", response.status()).into());
        }

        let bytes = response.bytes()?;
        fs::write(destination, &bytes)?;

        info!(
            "Downloaded media item {} to {:?}",
            media_item.media_item_id, destination
        );

        Ok(())
    }

    fn ensure_drive_folder_exists(
        &self,
        _folder_id: &str,
    ) -> Result<bool, Box<dyn std::error::Error>> {
        // TODO: Implement Drive API call to verify folder exists
        Ok(true)
    }

    fn upload_file_to_drive_folder(
        &self,
        _folder_id: &str,
        _local_file: &Path,
    ) -> Result<DriveUploadResult, Box<dyn std::error::Error>> {
        // TODO: Implement Drive API call to upload file
        Err("Not yet implemented".into())
    }
}
