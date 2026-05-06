use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;
use std::process::Command;
use tracing::{info, warn};

/// Risultato del processamento di una singola foto
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessingResult {
    pub filename: String,
    pub status: String, // "success" o "failed"
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

/// Report completo dal processamento dello script
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessingReport {
    #[serde(default)]
    pub results: Vec<ProcessingResult>,
}

/// Configurazione per eseguire il script di processing
pub struct ProcessingConfig {
    pub binary_path: PathBuf,
    pub binary_arguments: String,
    pub input_dir: PathBuf,
    pub output_dir: PathBuf,
    pub report_path: PathBuf,
}

impl ProcessingConfig {
    pub fn new(
        binary_path: PathBuf,
        binary_arguments: String,
        input_dir: PathBuf,
        output_dir: PathBuf,
        report_path: PathBuf,
    ) -> Self {
        Self {
            binary_path,
            binary_arguments,
            input_dir,
            output_dir,
            report_path,
        }
    }
}

/// Esegue lo script di processing e ritorna il report
pub fn run_processing(config: &ProcessingConfig) -> Result<ProcessingReport, String> {
    // Assicura che le directory di output esistano
    fs::create_dir_all(&config.output_dir)
        .map_err(|e| format!("Failed to create output directory: {}", e))?;

    info!(
        "Starting processing: {} with args: {}",
        config.binary_path.display(),
        config.binary_arguments
    );
    info!("  Input dir: {:?}", config.input_dir);
    info!("  Output dir: {:?}", config.output_dir);
    info!("  Report path: {:?}", config.report_path);

    // Costruisci il comando
    let mut cmd = Command::new(&config.binary_path);

    // Aggiungi gli argomenti custom dal progetto
    if !config.binary_arguments.is_empty() {
        let args: Vec<&str> = config.binary_arguments.split_whitespace().collect();
        cmd.args(&args);
    }

    // Aggiungi gli argomenti standard
    cmd.arg("--input")
        .arg(config.input_dir.to_string_lossy().to_string())
        .arg("--output")
        .arg(config.output_dir.to_string_lossy().to_string())
        .arg("--output-format")
        .arg("pfr1")
        .arg("--report")
        .arg("json")
        .arg("--report-output")
        .arg(config.report_path.to_string_lossy().to_string());

    info!(
        "Executing: {} {}",
        config.binary_path.display(),
        config.binary_arguments
    );

    // Esegui il comando
    let output = cmd
        .output()
        .map_err(|e| format!("Failed to execute processing binary: {}", e))?;

    // Log stdout/stderr
    if !output.stdout.is_empty() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        info!("Processing stdout: {}", stdout);
    }

    if !output.stderr.is_empty() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        warn!("Processing stderr: {}", stderr);
    }

    if !output.status.success() {
        return Err(format!(
            "Processing binary failed with exit code: {}",
            output.status.code().unwrap_or(-1)
        ));
    }

    // Parsa il report.json
    if !config.report_path.exists() {
        return Err(format!(
            "Processing report not found at: {}",
            config.report_path.display()
        ));
    }

    let report_content = fs::read_to_string(&config.report_path)
        .map_err(|e| format!("Failed to read processing report: {}", e))?;

    let report: ProcessingReport = serde_json::from_str(&report_content)
        .map_err(|e| format!("Failed to parse processing report: {}", e))?;

    info!(
        "Processing complete: {} results parsed",
        report.results.len()
    );

    // Log results
    let successful = report
        .results
        .iter()
        .filter(|r| r.status == "success")
        .count();
    let failed = report
        .results
        .iter()
        .filter(|r| r.status == "failed")
        .count();
    info!("  Successful: {}", successful);
    info!("  Failed: {}", failed);

    Ok(report)
}

/// Mappa risultati di processamento ai media item ID
pub fn map_results_to_media_items(
    report: &ProcessingReport,
    photos: &[&crate::PhotoSyncState],
) -> Result<Vec<(String, ProcessingResult)>, String> {
    let mut results = Vec::new();

    for photo in photos {
        let filename = photo
            .local_path
            .as_ref()
            .and_then(|p| p.file_name())
            .and_then(|n| n.to_str())
            .map(|s| s.to_string());

        if let Some(fname) = filename {
            if let Some(report_result) =
                report.results.iter().find(|r| r.filename.ends_with(&fname))
            {
                results.push((photo.media_item_id.clone(), report_result.clone()));
            } else {
                warn!(
                    "No report entry found for media item: {} ({})",
                    photo.media_item_id, fname
                );
            }
        } else {
            warn!(
                "Cannot determine filename for media item: {}",
                photo.media_item_id
            );
        }
    }

    Ok(results)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_processing_result_parsing() {
        let json = r#"{
            "results": [
                {"filename": "photo1.jpg", "status": "success"},
                {"filename": "photo2.jpg", "status": "failed", "error": "Invalid format"}
            ]
        }"#;

        let report: ProcessingReport = serde_json::from_str(json).unwrap();
        assert_eq!(report.results.len(), 2);
        assert_eq!(report.results[0].status, "success");
        assert_eq!(report.results[1].status, "failed");
        assert_eq!(report.results[1].error, Some("Invalid format".to_string()));
    }

    #[test]
    fn test_empty_processing_report() {
        let json = r#"{"results": []}"#;
        let report: ProcessingReport = serde_json::from_str(json).unwrap();
        assert!(report.results.is_empty());
    }

    #[test]
    fn test_processing_config_creation() {
        let config = ProcessingConfig::new(
            PathBuf::from("/usr/bin/processor"),
            "-v --threads 4".to_string(),
            PathBuf::from("/tmp/input"),
            PathBuf::from("/tmp/output"),
            PathBuf::from("/tmp/report.json"),
        );

        assert_eq!(config.binary_path, PathBuf::from("/usr/bin/processor"));
        assert_eq!(config.binary_arguments, "-v --threads 4");
    }
}
