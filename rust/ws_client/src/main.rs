mod websocket;

/// PhotoFrame WebSocket Client - WebSocket operations for ESP32 Photo Frame
///
/// This standalone tool handles all WebSocket communication with PhotoFrame devices:
/// - Testing connection and retrieving board configuration
/// - Uploading binary image files over WebSocket
///
/// Separated from the main image processor for focused functionality and cleaner dependencies.
use anyhow::{Context, Result};
use clap::Parser;
use console::style;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(
    name = "ws_client",
    version,
    about = "WebSocket operations for ESP32 Photo Frame",
    long_about = "PhotoFrame WebSocket Client - Handle WebSocket communication with ESP32 Photo Frame devices

This tool provides WebSocket communication with PhotoFrame devices:
• Test connection and retrieve board configuration
• Upload binary image files to PhotoFrame over WebSocket

Example Usage:
  # Test connection and get board configuration
  ws_client --test

  # Test with custom URL
  ws_client --test --url ws://192.168.4.1:81

  # Upload a binary file (not yet implemented)
  ws_client --upload -f image.pfr1

  # Upload with custom URL
  ws_client --upload -f image.pfr1 --url ws://192.168.4.1:81
"
)]
struct Args {
    /// Test connection and retrieve board configuration
    #[arg(
        long = "test",
        conflicts_with = "upload",
        help = "Test WebSocket connection and get board configuration",
        required_unless_present = "upload"
    )]
    test: Option<String>,

    /// Upload a binary file to PhotoFrame device
    #[arg(
        long = "upload",
        conflicts_with = "test",
        help = "Upload a binary file to PhotoFrame device (requires --file)",
        required_unless_present = "test"
    )]
    upload: Option<String>,

    /// Binary file to upload (required for --upload)
    #[arg(
        short = 'f',
        long = "file",
        value_name = "FILE",
        required_if_eq("upload", "true")
    )]
    file: Option<PathBuf>,

    /// Display orientation (0-3) for uploaded image
    #[arg(
        short = 'o',
        long = "orientation",
        value_name = "ORIENTATION",
        default_value = "0",
        help = "Display orientation: 0=0°, 1=90°, 2=180°, 3=270°"
    )]
    orientation: u8,

    /// Verbose output
    #[arg(short = 'v', long = "verbose")]
    verbose: bool,
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();

    // Print banner
    println!("{}", style("PhotoFrame WebSocket Client").bold().cyan());
    println!(
        "{}",
        style("WebSocket operations for ESP32 Photo Frame").dim()
    );
    println!();

    // At least one operation must be specified
    if args.test.is_none() && args.upload.is_none() {
        return Err(anyhow::anyhow!(
            "Please specify either --test or --upload. Use --help for more information."
        ));
    }

    if args.test.is_some() {
        // Test connection and get configuration
        let url = args.test.as_deref();
        println!("{}", style("Testing WebSocket connection...").bold());
        println!();

        match websocket::test_connection(url).await {
            Ok(config) => {
                config.print();
                println!("{}", style("✓ Test completed successfully").bold().green());
                Ok(())
            }
            Err(e) => {
                println!("{}", style(format!("✗ Test failed: {}", e)).bold().red());
                Err(e)
            }
        }
    } else if args.upload.is_some() {
        // Upload image
        let url = args.upload.as_deref(); // FIX: use upload URL, not test URL
        let file = args.file.as_ref().unwrap(); // Safe because of required_if_eq

        // Validate orientation
        if args.orientation > 3 {
            return Err(anyhow::anyhow!(
                "Invalid orientation: {} (must be 0-3)",
                args.orientation
            ));
        }

        // Validate file
        if !file.exists() {
            return Err(anyhow::anyhow!("File not found: {}", file.display()));
        }

        if file.extension().and_then(|s| s.to_str()) != Some("pfr1") {
            return Err(anyhow::anyhow!(
                "File must have .pfr1 extension: {}",
                file.display()
            ));
        }

        if args.verbose {
            println!("{}", style("Upload Configuration:").bold());
            println!("  File: {}", file.display());
            println!("  URL: {}", url.unwrap_or("ws://192.168.4.1:81"));
            println!(
                "  Orientation: {}° ({})",
                args.orientation * 90,
                args.orientation
            );
            println!();
        }

        println!("{}", style("Uploading to PhotoFrame device...").dim());
        websocket::upload_image(url, file.to_str().unwrap(), args.orientation)
            .await
            .with_context(|| format!("Failed to upload {}", file.display()))
    } else {
        unreachable!()
    }
}
