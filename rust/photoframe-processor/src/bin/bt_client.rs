/// PhotoFrame BLE Client - Bluetooth operations for ESP32 Photo Frame
///
/// This standalone tool handles all Bluetooth communication with PhotoFrame devices:
/// - Scanning for available devices
/// - Uploading binary image files over BLE
///
/// Separated from the main image processor for focused functionality and cleaner dependencies.
use anyhow::{Context, Result};
use clap::Parser;
use console::style;
use std::path::PathBuf;

// Re-use the bluetooth module from the library
use photoframe_processor::bluetooth;

#[derive(Parser, Debug)]
#[command(
    name = "bt_client",
    version,
    about = "Bluetooth operations for ESP32 Photo Frame",
    long_about = "PhotoFrame BLE Client - Handle Bluetooth communication with ESP32 Photo Frame devices

This tool provides Bluetooth Low Energy (BLE) communication with PhotoFrame devices:
• Scan for nearby PhotoFrame devices
• Upload binary image files to PhotoFrame over BLE
• Query device information and status

Example Usage:
  # Scan for available devices
  bt_client scan

  # Upload a binary file to a specific device
  bt_client upload -f image.pfr1 -d PhotoFrame-ABC123

  # Upload to first available device
  bt_client upload -f image.pfr1

  # Specify device by MAC address
  bt_client upload -f image.pfr1 -d AA:BB:CC:DD:EE:FF

  # Set display orientation during upload (0-3)
  bt_client upload -f image.pfr1 -d PhotoFrame-ABC123 --orientation 1
"
)]
struct Args {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Parser, Debug)]
enum Commands {
    /// Scan for available PhotoFrame devices
    Scan,

    /// Upload a binary file to a PhotoFrame device
    Upload {
        /// Binary file to upload (must be .pfr1 format)
        #[arg(short = 'f', long = "file", value_name = "FILE")]
        file: PathBuf,

        /// Device name or MAC address (optional - will scan if not provided)
        #[arg(short = 'd', long = "device", value_name = "ADDR|NAME")]
        device: Option<String>,

        /// Display orientation (0-3: 0=landscape, 1=portrait, 2=landscape-reverse, 3=portrait-reverse)
        #[arg(long = "orientation", default_value = "0", value_name = "ORIENTATION")]
        orientation: u8,

        /// Verbose output
        #[arg(short = 'v', long = "verbose")]
        verbose: bool,
    },
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Print banner
    println!("{}", style("PhotoFrame BLE Client").bold().cyan());
    println!(
        "{}",
        style("Bluetooth operations for ESP32 Photo Frame").dim()
    );
    println!();

    match args.command {
        Commands::Scan => {
            println!("{}", style("Scanning for PhotoFrame devices...").dim());
            println!();
            bluetooth::scan_devices()
        }

        Commands::Upload {
            file,
            device,
            orientation,
            verbose,
        } => {
            // Validate the file
            if !file.exists() {
                return Err(anyhow::anyhow!("File not found: {}", file.display()));
            }

            if file.extension().and_then(|s| s.to_str()) != Some("pfr1") {
                return Err(anyhow::anyhow!(
                    "File must have .pfr1 extension: {}",
                    file.display()
                ));
            }

            if orientation > 3 {
                return Err(anyhow::anyhow!(
                    "Invalid orientation: {}. Must be 0-3",
                    orientation
                ));
            }

            if verbose {
                println!("{}", style("Upload Configuration:").bold());
                println!("  File: {}", file.display());
                println!("  Device: {}", device.as_deref().unwrap_or("auto-detect"));
                println!(
                    "  Orientation: {} ({})",
                    orientation,
                    match orientation {
                        0 => "landscape",
                        1 => "portrait",
                        2 => "landscape-reverse",
                        3 => "portrait-reverse",
                        _ => "unknown",
                    }
                );
                println!();
            }

            println!("{}", style("Uploading to PhotoFrame device...").dim());
            bluetooth::upload_image_with_dimensions(&file, 0, 0, orientation, device.as_deref())
                .with_context(|| format!("Failed to upload {}", file.display()))
        }
    }
}
