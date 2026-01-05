//! JSON output for GUI integration
//!
//! When --json-progress flag is enabled, all progress and status information
//! is emitted as JSON lines to stdout, suppressing all other output.

use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{SystemTime, UNIX_EPOCH};

/// Last progress emission timestamp (milliseconds since epoch)
/// Used for throttling progress updates to ~25 FPS (40ms between updates)
static LAST_PROGRESS_MS: AtomicU64 = AtomicU64::new(0);

#[derive(Debug, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum JsonMessage {
    /// Progress update
    Progress {
        current: usize,
        total: usize,
        message: String,
    },
    /// File processing completed
    FileCompleted {
        input_path: String,
        output_paths: Vec<String>,
        processing_time_ms: u128,
    },
    /// File processing failed
    FileFailed { input_path: String, error: String },
    /// Processing summary
    Summary {
        total_files: usize,
        processed: usize,
        failed: usize,
        duration_secs: f64,
    },
}

impl JsonMessage {
    /// Emit JSON message to stdout
    pub fn emit(&self) {
        if let Ok(json) = serde_json::to_string(self) {
            println!("{}", json);
        }
    }

    /// Create and emit progress message (throttled to ~25 FPS for smooth GUI updates)
    ///
    /// Progress updates are throttled to emit at most every 40ms (25 FPS target).
    /// The final progress (current == total) is always emitted to ensure 100% completion.
    pub fn progress(current: usize, total: usize, message: impl Into<String>) {
        // Get current time in milliseconds
        let now_ms = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_millis() as u64)
            .unwrap_or(0);

        let last_ms = LAST_PROGRESS_MS.load(Ordering::Relaxed);

        // Emit if:
        // 1. At least 40ms have passed since last emission (25 FPS), OR
        // 2. This is the final progress update (current == total)
        if now_ms - last_ms >= 40 || current == total {
            LAST_PROGRESS_MS.store(now_ms, Ordering::Relaxed);
            Self::Progress {
                current,
                total,
                message: message.into(),
            }
            .emit();
        }
    }

    /// Create and emit file completed message
    #[allow(dead_code)]
    pub fn file_completed(
        input_path: &PathBuf,
        output_paths: &[(PathBuf, String)],
        processing_time_ms: u128,
    ) {
        Self::FileCompleted {
            input_path: input_path.display().to_string(),
            output_paths: output_paths
                .iter()
                .map(|(p, _)| p.display().to_string())
                .collect(),
            processing_time_ms,
        }
        .emit();
    }

    /// Create and emit file failed message
    #[allow(dead_code)]
    pub fn file_failed(input_path: &PathBuf, error: impl Into<String>) {
        Self::FileFailed {
            input_path: input_path.display().to_string(),
            error: error.into(),
        }
        .emit();
    }

    /// Create and emit summary message
    pub fn summary(total_files: usize, processed: usize, failed: usize, duration_secs: f64) {
        Self::Summary {
            total_files,
            processed,
            failed,
            duration_secs,
        }
        .emit();
    }
}
