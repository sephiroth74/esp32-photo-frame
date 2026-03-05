use clap::Parser;
use dirs;
use std::fs::{OpenOptions, create_dir_all};
use std::io::Write;
use std::path::PathBuf;
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

    for dir in systemd_directories::logs_dirs() {
        info!("System log directory: {}", dir.display());
    }

    // Example logs to demonstrate the logging system
    info!("This is an info message");
    warn!("This is a warning message");
    debug!("This is a debug message");
    error!("This is an error message");
    info!("Photo frame worker finished");
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
        write!(w, "{}", now.format("%Y-%m-%d %H:%M:%S%.3f"))
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
