mod messages;
mod websocket;

/// PhotoFrame WebSocket Client - WebSocket operations for ESP32 Photo Frame
///
/// This standalone tool handles all WebSocket communication with PhotoFrame devices:
/// - Testing connection and retrieving board configuration
/// - Uploading binary image files over WebSocket
///
/// Separated from the main image processor for focused functionality and cleaner dependencies.
use anyhow::Result;
use clap::Parser;
use console::style;
use dialoguer::{Confirm, Input, Select, theme::ColorfulTheme};
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(
    name = "ws_client",
    version,
    about = "WebSocket Client for ESP32 Photo Frame",
    long_about = "PhotoFrame WebSocket Client - Interactive session-based WebSocket communication with ESP32 Photo Frame devices

Available Commands:
  get_config   - Retrieve and display board configuration
  upload       - Upload binary image file with optional rotation
  shutdown     - Send shutdown command for deep sleep
  disconnect   - Close connection and exit
  help         - Show available commands

Example Usage:
  ws_client connect 192.168.4.1:81
    > get_config
    > upload image.pfr1 --orientation 0
    > shutdown
"
)]
struct Args {
    /// Connect to PhotoFrame device in interactive mode
    #[arg(
        value_name = "COMMAND",
        help = "Command: 'connect' to start interactive session"
    )]
    command: Option<String>,

    /// WebSocket URL (e.g., 192.168.4.1:81)
    #[arg(value_name = "URL", help = "Device URL or IP address with port")]
    url: Option<String>,
}

fn parse_ws_url(url: &str) -> String {
    let ws_url = if url.starts_with("ws://") {
        url.to_string()
    } else {
        format!("ws://{}", url)
    };
    ws_url
}

async fn interactive_mode(ws_url: &str) -> Result<()> {
    println!("{}", style("Connecting to PhotoFrame device...").dim());

    let mut ws_stream = websocket::connect(ws_url).await?;

    println!("{}", style("✓ Connected").bold().green());
    println!();

    let theme = ColorfulTheme::default();

    loop {
        let options = vec![
            "Get device configuration",
            "Upload image file",
            "Shutdown device",
            "Disconnect and exit",
        ];

        let selection = Select::with_theme(&theme)
            .with_prompt("PhotoFrame WebSocket Client")
            .items(&options)
            .default(0)
            .interact_opt()?;

        match selection {
            Some(0) => {
                // Get config
                println!();
                match websocket::get_config(&mut ws_stream).await {
                    Ok(config) => {
                        config.print();
                        println!("{}", style("✓ Configuration retrieved").bold().green());
                    }
                    Err(e) => {
                        println!("{}", style(format!("✗ Error: {}", e)).bold().red());
                    }
                }
                println!();
            }
            Some(1) => {
                // Upload
                println!();

                let file: String = Input::with_theme(&theme)
                    .with_prompt("Enter file path")
                    .interact_text()?;

                if file.is_empty() {
                    println!("{}", style("No file specified").yellow());
                    println!();
                    continue;
                }

                let orientation_str: String = Input::with_theme(&theme)
                    .with_prompt("Enter orientation (0-3)")
                    .default("0".to_string())
                    .interact_text()?;

                let orientation = match orientation_str.parse::<u8>() {
                    Ok(o) if o <= 3 => o,
                    _ => {
                        println!(
                            "{}",
                            style("Invalid orientation. Using default (0)").yellow()
                        );
                        0u8
                    }
                };

                // Validate file
                let path = PathBuf::from(&file);
                if !path.exists() {
                    println!("{}", style(format!("File not found: {}", file)).red());
                    println!();
                    continue;
                }

                if path.extension().and_then(|s| s.to_str()) != Some("pfr1") {
                    println!("{}", style("File must have .pfr1 extension").red());
                    println!();
                    continue;
                }

                println!("{}", style("Uploading image...").dim());
                match websocket::upload_image_with_connection(&mut ws_stream, &file, orientation)
                    .await
                {
                    Ok(_) => {
                        println!("{}", style("✓ Upload completed").bold().green());
                    }
                    Err(e) => {
                        println!("{}", style(format!("✗ Upload failed: {}", e)).bold().red());
                    }
                }
                println!();
            }
            Some(2) => {
                // Shutdown
                println!();

                let confirmed = Confirm::with_theme(&theme)
                    .with_prompt("Are you sure you want to shutdown the device?")
                    .default(false)
                    .interact()?;

                if !confirmed {
                    println!("{}", style("Shutdown cancelled").yellow());
                    println!();
                    continue;
                }

                println!("{}", style("Sending shutdown command...").dim());
                match websocket::send_shutdown_with_connection(&mut ws_stream).await {
                    Ok(_) => {
                        println!("{}", style("✓ Shutdown command sent").bold().green());
                        println!(
                            "{}",
                            style("Device is going to deep sleep. Disconnecting...").dim()
                        );
                        break;
                    }
                    Err(e) => {
                        println!(
                            "{}",
                            style(format!("✗ Shutdown failed: {}", e)).bold().red()
                        );
                        println!();
                    }
                }
            }
            Some(3) | None => {
                // Disconnect or Ctrl+C
                println!();
                println!("{}", style("Disconnecting...").dim());
                break;
            }
            _ => unreachable!(),
        }
    }

    println!("{}", style("✓ Disconnected").bold().green());
    Ok(())
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();

    // Print banner
    println!("{}", style("PhotoFrame WebSocket Client").bold().cyan());
    println!(
        "{}",
        style("Interactive WebSocket session for ESP32 Photo Frame").dim()
    );
    println!();

    // Check for connect command
    match (&args.command, &args.url) {
        (Some(command), Some(url)) if command == "connect" => {
            let ws_url = parse_ws_url(url);
            interactive_mode(&ws_url).await
        }
        (None, None) => Err(anyhow::anyhow!(
            "Usage: ws_client connect <URL>\n\nExample: ws_client connect 192.168.4.1:81"
        )),
        (Some(cmd), _) => Err(anyhow::anyhow!("Unknown command: {}. Use 'connect'.", cmd)),
        _ => Err(anyhow::anyhow!(
            "URL required. Usage: ws_client connect <URL>"
        )),
    }
}
