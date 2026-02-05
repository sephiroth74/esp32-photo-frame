mod messages;
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
• Send commands (shutdown/deep sleep)

Example Usage:
  # Test connection and get board configuration
  ws_client --test 192.168.4.1

  # Upload a binary file
  ws_client --upload 192.168.4.1:81 -f image.pfr1

  # Shutdown device to deep sleep
  ws_client --shutdown 192.168.4.1:81
"
)]
struct Args {
    /// Test connection and retrieve board configuration
    #[arg(
        long = "test",
        conflicts_with_all = ["upload", "shutdown"],
        help = "Test WebSocket connection and get board configuration",
        required_unless_present_any = ["upload", "shutdown"]
    )]
    test: Option<String>,

    /// Upload a binary file to PhotoFrame device
    #[arg(
        long = "upload",
        conflicts_with_all = ["test", "shutdown"],
        help = "Upload a binary file to PhotoFrame device (requires --file)",
        required_unless_present_any = ["test", "shutdown"]
    )]
    upload: Option<String>,

    /// Shutdown device to deep sleep
    #[arg(
        long = "shutdown",
        conflicts_with_all = ["test", "upload"],
        help = "Send shutdown command to put device in deep sleep",
        required_unless_present_any = ["test", "upload"]
    )]
    shutdown: Option<String>,

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

fn parse_ws_url(url: &str) -> String {
    let ws_url = if url.starts_with("ws://") {
        url.to_string()
    } else {
        format!("ws://{}", url)
    };
    ws_url
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
    if args.test.is_none() && args.upload.is_none() && args.shutdown.is_none() {
        return Err(anyhow::anyhow!(
            "Please specify either --test, --upload, or --shutdown. Use --help for more information."
        ));
    }

    if let Some(url) = args.test {
        // Test connection and get configuration
        let ws_url = parse_ws_url(&url);

        println!("{}", style("Testing WebSocket connection...").bold());
        println!();

        match websocket::test_connection(&ws_url).await {
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
    } else if let Some(url) = args.upload {
        // Upload image
        let ws_url = parse_ws_url(&url);
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
            println!("  URL: {}", url);
            println!(
                "  Orientation: {}° ({})",
                args.orientation * 90,
                args.orientation
            );
            println!();
        }

        println!("{}", style("Uploading to PhotoFrame device...").dim());
        websocket::upload_image(&ws_url, file.to_str().unwrap(), args.orientation)
            .await
            .with_context(|| format!("Failed to upload {}", file.display()))
    } else if let Some(url) = args.shutdown {
        // Send shutdown command
        let ws_url = parse_ws_url(&url);
        println!("{}", style("Sending shutdown command...").bold());
        println!();
        websocket::send_shutdown(&ws_url).await
    } else {
        unreachable!()
    }
}
