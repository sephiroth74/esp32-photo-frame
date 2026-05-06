use clap::Parser;
use dirs;
use photoframe_common::{
    GoogleAuthStatus, GooglePhotosClient, GoogleServices, ProcessingConfig, ProjectFileManager,
    SyncStateManager, get_google_auth_status, get_valid_access_token, perform_google_oauth_login,
    run_processing,
};
use std::fs::{OpenOptions, create_dir_all};
use std::io::Write;
use std::path::PathBuf;
use tempdir::TempDir;
use tracing::{debug, error, info, warn};
use tracing_subscriber::EnvFilter;
use tracing_subscriber::fmt::format::FmtSpan;
use tracing_subscriber::fmt::time::FormatTime;
use tracing_subscriber::layer::SubscriberExt;
use tracing_subscriber::util::SubscriberInitExt;

#[derive(Parser, Debug)]
#[command(
    name = "photoframe_worker",
    about = "Photo frame worker process",
    version
)]
struct Args {
    /// Path to the log file (optional)
    #[arg(short, long, value_name = "FILE")]
    log_file: Option<PathBuf>,

    /// Path to the project directory (mandatory)
    #[arg(short, long, value_name = "PROJECT_DIR")]
    project_dir: PathBuf,
}

fn main() {
    let args = Args::parse();

    let log_file_path = if let Some(log_file) = &args.log_file {
        Some(log_file.clone())
    } else {
        if let Some(mut log_path) = dirs::data_local_dir() {
            log_path.push("PhotoFrameWorker/Logs/worker.log");
            println!("Il path dei log utente è: {:?}", log_path);
            Some(log_path)
        } else {
            None
        }
    };

    // Setup logging
    if let Err(e) = setup_logging(log_file_path.as_ref()) {
        eprintln!("Failed to setup logging: {}", e);
        std::process::exit(1);
    }

    info!("Photo frame worker started");
    info!("Command line arguments: {:?}", args);

    info!("Project directory: {}", args.project_dir.display());
    info!(
        "Log file: {}",
        log_file_path
            .as_ref()
            .map_or("None".into(), |p| p.display().to_string())
    );

    let project_manager = match ProjectFileManager::open(&args.project_dir) {
        Ok(pm) => pm,
        Err(err) => {
            error!("Failed to open project: {}", err);
            std::process::exit(1);
        }
    };

    let project_config = match project_manager.read_config() {
        Ok(config) => config,
        Err(err) => {
            error!("Failed to read project configuration: {}", err);
            std::process::exit(1);
        }
    };

    if !project_config.is_minimally_ready_for_worker() {
        error!(
            "Project is not ready for worker execution (missing album/folder/binary configuration)"
        );
        std::process::exit(1);
    }

    info!("Worker bootstrap config loaded successfully");
    info!("Album ID present: {}", project_config.album_id.is_some());
    info!(
        "Drive folder ID present: {}",
        project_config.drive_folder_id.is_some()
    );
    info!(
        "Binary path present: {}",
        project_config.binary_path.is_some()
    );

    let credentials_path = project_manager.credentials_path();
    let token_path = project_manager.token_path();

    let auth_status = match get_google_auth_status(&credentials_path, &token_path) {
        Ok(status) => status,
        Err(err) => {
            error!("Failed to determine Google auth status: {}", err);
            std::process::exit(1);
        }
    };

    match auth_status {
        GoogleAuthStatus::MissingCredentials => {
            error!(
                "Google credentials are missing. Expected file at: {}",
                credentials_path.display()
            );
            std::process::exit(1);
        }
        GoogleAuthStatus::NeedsLogin { .. } => {
            info!("Google login required. Starting OAuth flow...");
            if let Err(err) = perform_google_oauth_login(&token_path, &credentials_path) {
                error!("Google OAuth login failed: {}", err);
                std::process::exit(1);
            }
        }
        GoogleAuthStatus::LoggedIn { .. } => {
            info!("Google token already available");
        }
    }

    let access_token = match get_valid_access_token(&token_path, &credentials_path) {
        Ok(token) => token,
        Err(err) => {
            error!("Failed to get valid access token: {}", err);
            std::process::exit(1);
        }
    };
    info!(
        "Google authentication ready (token length: {})",
        access_token.len()
    );

    // Load sync state manager
    let sync_manager = SyncStateManager::new(&args.project_dir);
    info!(
        "Sync state manager initialized at: {:?}",
        sync_manager.db_path()
    );

    // Initialize database (creates tables if not exist)
    if let Err(err) = sync_manager.init_db() {
        error!("Failed to initialize sync database: {}", err);
        std::process::exit(1);
    }

    // Load album sync state
    let album_id = project_config.album_id.as_ref().unwrap().clone();
    let drive_folder_id = project_config.drive_folder_id.as_ref().unwrap().clone();

    let sync_state = match sync_manager.load_album_state(&album_id, &drive_folder_id) {
        Ok(state) => state,
        Err(err) => {
            error!("Failed to load sync state: {}", err);
            std::process::exit(1);
        }
    };

    info!("Album sync state loaded");
    info!("  Album ID: {}", sync_state.album_id);
    info!("  Drive folder ID: {}", sync_state.drive_folder_id);
    info!("  Total photos in DB: {}", sync_state.photos.len());
    info!(
        "  Pending downloads: {}",
        sync_state.get_pending_downloads().len()
    );
    info!(
        "  Pending processing: {}",
        sync_state.get_pending_processing().len()
    );
    info!(
        "  Pending upload: {}",
        sync_state.get_pending_upload().len()
    );
    info!("  Failed: {}", sync_state.get_failed().len());
    if let Some(last_sync) = &sync_state.last_sync {
        info!("  Last sync: {}", last_sync);
    }

    // Step 1: List photos from album
    info!("Starting photo synchronization workflow...");
    let google_client = GooglePhotosClient::new(access_token.clone());

    let all_media_items = match google_client.list_album_media_items(&album_id) {
        Ok(items) => items,
        Err(err) => {
            error!("Failed to list album media items: {}", err);
            std::process::exit(1);
        }
    };

    info!("Retrieved {} media items from album", all_media_items.len());

    // Step 2: Filter photos not already processed
    let new_items: Vec<_> = all_media_items
        .iter()
        .filter(|item| !sync_state.photos.contains_key(&item.media_item_id))
        .collect();

    if new_items.is_empty() {
        info!("No new photos to download");
        std::process::exit(0);
    }

    info!("Found {} new photos to process", new_items.len());

    // Step 3: Create temporary download directory
    let input_dir = args.project_dir.join(".temp/input");
    if let Err(err) = create_dir_all(&input_dir) {
        error!("Failed to create input directory: {}", err);
        std::process::exit(1);
    }
    info!("Created temporary input directory: {:?}", input_dir);

    // Step 4: Download new photos
    let mut updated_sync_state = sync_state.clone();
    let mut download_count = 0;
    let mut download_errors = 0;
    let mut filename_to_media_item: std::collections::HashMap<String, String> =
        std::collections::HashMap::new();

    for media_item in &new_items {
        let filename = media_item
            .filename
            .as_ref()
            .map(|f| f.clone())
            .unwrap_or_else(|| {
                let now = chrono::Utc::now().timestamp_nanos_opt().unwrap_or(0);
                format!("photo_{}.jpg", now)
            });

        let local_path = input_dir.join(&filename);

        match google_client.download_media_item(media_item, &local_path) {
            Ok(()) => {
                info!(
                    "Downloaded: {} -> {:?}",
                    media_item.media_item_id, local_path
                );
                download_count += 1;

                // Map filename to media_item_id for processing phase
                filename_to_media_item.insert(filename.clone(), media_item.media_item_id.clone());

                // Update sync state
                let mut photo_state =
                    photoframe_common::PhotoSyncState::new(media_item.media_item_id.clone())
                        .with_local_path(local_path.clone());
                photo_state.set_status(photoframe_common::PhotoSyncStatus::Downloaded);
                updated_sync_state
                    .photos
                    .insert(media_item.media_item_id.clone(), photo_state);
            }
            Err(err) => {
                error!("Failed to download {}: {}", media_item.media_item_id, err);
                download_errors += 1;

                // Mark as failed in sync state
                let mut photo_state =
                    photoframe_common::PhotoSyncState::new(media_item.media_item_id.clone());
                photo_state.mark_failed(format!("Download error: {}", err));
                updated_sync_state
                    .photos
                    .insert(media_item.media_item_id.clone(), photo_state);
            }
        }
    }

    // Step 5: Execute processing script
    info!("Starting photo processing phase...");

    let temp_dir =
        TempDir::new("photo_frame_worker").expect("Failed to create temporary directory");

    debug!(
        "Using temporary directory for processing: {:?}",
        temp_dir.path()
    );

    let output_dir = temp_dir.path().join("output");
    let report_path = temp_dir.path().join("report.json");

    let processing_config = ProcessingConfig::new(
        project_config.binary_path.as_ref().unwrap().clone(),
        project_config
            .binary_arguments
            .as_ref()
            .unwrap_or(&String::new())
            .clone(),
        input_dir.clone(),
        output_dir.clone(),
        report_path.clone(),
    );

    match run_processing(&processing_config) {
        Ok(report) => {
            info!("Processing completed successfully");

            // Step 6: Parse report and update sync state
            for result in &report.results {
                info!(
                    "Processing result for {}: {}",
                    result.filename, result.status
                );

                // Find the media_item_id from filename mapping
                if let Some(media_item_id) = filename_to_media_item.get(&result.filename) {
                    if let Some(photo_state) = updated_sync_state.photos.get_mut(media_item_id) {
                        if result.status == "success" {
                            photo_state.set_status(photoframe_common::PhotoSyncStatus::Processed);
                            info!("Photo {} marked as Processed", media_item_id);
                        } else {
                            let error_msg = result
                                .error
                                .as_ref()
                                .map(|e| e.clone())
                                .unwrap_or_else(|| "Unknown error".to_string());
                            photo_state.mark_failed(format!("Processing failed: {}", error_msg));
                            error!("Photo {} marked as Failed: {}", media_item_id, error_msg);
                        }
                    }
                } else {
                    warn!(
                        "No media item found for processed file: {}",
                        result.filename
                    );
                }
            }
        }
        Err(err) => {
            error!("Processing failed: {}", err);
            // Mark all downloaded photos as failed
            for photo_state in updated_sync_state.photos.values_mut() {
                if matches!(
                    photo_state.status,
                    photoframe_common::PhotoSyncStatus::Downloaded
                ) {
                    photo_state.mark_failed(format!("Processing script failed: {}", err));
                }
            }
        }
    }

    // Step 7: Save final sync state
    updated_sync_state.mark_sync_complete();
    if let Err(err) = sync_manager.save_album_state(&updated_sync_state) {
        error!("Failed to save sync state: {}", err);
        std::process::exit(1);
    }

    info!(
        "Worker run complete. Downloaded: {} photos, synced to DB",
        download_count
    );
    info!("Sync state saved to: {:?}", sync_manager.db_path());

    for dir in systemd_directories::logs_dirs() {
        info!("System log directory: {}", dir.display());
    }

    info!("Photo frame worker finished successfully");
}

fn setup_logging(log_file: Option<&PathBuf>) -> Result<(), Box<dyn std::error::Error>> {
    let env_filter = EnvFilter::try_from_default_env()
        .or_else(|_| EnvFilter::try_new("info"))
        .unwrap();

    // Custom timer that formats timestamp with milliseconds
    let timer = CustomTimer;

    let stdout_layer = tracing_subscriber::fmt::layer()
        .with_ansi(true)
        .with_target(false)
        .with_thread_ids(false)
        .with_thread_names(false)
        .with_file(false)
        .with_line_number(false)
        .with_span_events(FmtSpan::NONE)
        .with_timer(timer.clone())
        .with_writer(std::io::stdout);

    match log_file {
        Some(path) => {
            // Create or open the log file
            if let Some(parent) = path.parent() {
                create_dir_all(parent)?;
            }

            let file = OpenOptions::new().create(true).append(true).open(path)?;

            let file_layer = tracing_subscriber::fmt::layer()
                .with_ansi(false)
                .with_target(false)
                .with_thread_ids(false)
                .with_thread_names(false)
                .with_file(false)
                .with_line_number(false)
                .with_span_events(FmtSpan::NONE)
                .with_timer(timer)
                .with_writer(move || FileWriter::new(file.try_clone().unwrap()));

            tracing_subscriber::registry()
                .with(env_filter)
                .with(stdout_layer)
                .with(file_layer)
                .init();

            info!("Logging to file: {}", path.display());
        }
        None => {
            tracing_subscriber::registry()
                .with(env_filter)
                .with(stdout_layer)
                .init();
        }
    }

    Ok(())
}

/// Custom timer that formats timestamp with date, hours, minutes, seconds, and milliseconds
#[derive(Clone)]
struct CustomTimer;

impl FormatTime for CustomTimer {
    fn format_time(&self, w: &mut tracing_subscriber::fmt::format::Writer<'_>) -> std::fmt::Result {
        let now = chrono::Local::now();
        w.write_fmt(format_args!("{}", now.format("%Y-%m-%d %H:%M:%S%.3f")))
    }
}

/// A simple file writer wrapper
struct FileWriter {
    file: std::fs::File,
}

impl FileWriter {
    fn new(file: std::fs::File) -> Self {
        Self { file }
    }
}

impl Write for FileWriter {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.file.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.file.flush()
    }
}
