use anyhow::{Context, Result, anyhow};
use console::style;
use futures_util::{SinkExt, StreamExt};
use indicatif::{ProgressBar, ProgressStyle};
use serde::{Deserialize, Serialize};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use photoframe_lib::validate_bin_file;
use tokio::fs;
use tokio::time::timeout;
use tokio_tungstenite::{connect_async, tungstenite::Message};

/// Default WebSocket URL for PhotoFrame device
const DEFAULT_WS_URL: &str = "ws://192.168.4.1:81";

/// Connection timeout in seconds
const CONNECTION_TIMEOUT_SECS: u64 = 10;

/// Response timeout in seconds  
const RESPONSE_TIMEOUT_SECS: u64 = 5;

/// Board configuration returned by GET_CONFIG command
#[derive(Debug, Deserialize, Serialize)]
pub struct BoardConfig {
    pub board: String,
    pub flash_size: String,
    pub flash_size_bytes: u32,
    pub display_type: String,
    pub display_width: u16,
    pub display_height: u16,
    pub display_rotation: u16,
    pub server_version: String,
    pub file_version: u8,
    pub binary_file_size: u32,
    #[serde(default)]
    pub battery_level: Option<i8>,
    #[serde(default)]
    pub battery_voltage_mv: Option<i32>,
}

impl BoardConfig {
    /// Validate the board configuration
    pub fn validate(&self) -> Result<()> {
        // Check version
        if self.file_version == 0 {
            return Err(anyhow!("Invalid board config file version: 0"));
        }

        if self.binary_file_size == 0 {
            return Err(anyhow!("Binary file size is zero"));
        }

        if self.server_version.is_empty() {
            return Err(anyhow!("Server version is empty"));
        }

        // Check display dimensions
        if self.display_width == 0 || self.display_height == 0 {
            return Err(anyhow!(
                "Invalid display dimensions: {}x{}",
                self.display_width,
                self.display_height
            ));
        }

        // Check rotation
        if self.display_rotation > 270 {
            return Err(anyhow!("Invalid rotation: {}", self.display_rotation));
        }

        Ok(())
    }

    /// Print board configuration in a formatted way
    pub fn print(&self) {
        println!("{}", style("Board Configuration:").bold().green());
        println!("  File Version: {}", self.file_version);
        println!("  Server Version: {}", self.server_version);
        println!("  Display Type: {}", self.display_type);
        println!("  Binary File Size: {} bytes", self.binary_file_size);
        println!(
            "  Dimensions: {}x{}",
            self.display_width, self.display_height
        );
        println!("  Rotation: {}°", self.display_rotation);
        println!();
        println!("{}", style("Hardware:").bold().cyan());
        println!("  Chip Model: {}", self.board);
        println!("  Board Flash Size: {}", self.flash_size);
        if let (Some(level), Some(voltage)) = (self.battery_level, self.battery_voltage_mv) {
            println!("  Battery: {:.1}% ({:.2}V)", level, voltage);
        } else {
            println!("{}", style("Battery: Not available").dim());
        }
    }
}

/// Connect to PhotoFrame WebSocket and get board configuration
pub async fn test_connection(url: Option<&str>) -> Result<BoardConfig> {
    let ws_url = url.unwrap_or(DEFAULT_WS_URL);

    println!("{}", style(format!("Connecting to {}...", ws_url)).dim());

    // Connect to WebSocket with timeout
    let (ws_stream, _) = timeout(
        Duration::from_secs(CONNECTION_TIMEOUT_SECS),
        connect_async(ws_url),
    )
    .await
    .context("Connection timeout")?
    .context("Failed to connect to WebSocket")?;

    println!("{}", style("✓ Connected").green());

    let (mut write, mut read) = ws_stream.split();

    // Send GET_CONFIG command
    println!("{}", style("Sending GET_CONFIG command...").dim());
    write
        .send(Message::Text("GET_CONFIG".into()))
        .await
        .context("Failed to send GET_CONFIG command")?;

    loop {
        // Wait for response with timeout
        let response = timeout(Duration::from_secs(RESPONSE_TIMEOUT_SECS), read.next())
            .await
            .context("Response timeout")?
            .ok_or_else(|| anyhow!("Connection closed by server"))?
            .context("Failed to receive response")?;

        match response {
            Message::Text(text) => {
                println!(
                    "{}",
                    style(format!("Received text message: {} bytes", text.len())).dim()
                );

                println!("{}", style("✓ Received configuration").green());

                let config: BoardConfig = serde_json::from_str(&text)
                    .context("Failed to parse board configuration JSON")?;

                config
                    .validate()
                    .context("Board configuration validation failed")?;

                println!("{}", style("✓ Configuration validated").green());
                println!();

                return Ok(config);
            }
            Message::Binary(data) => {
                println!(
                    "{}",
                    style(format!(
                        "Received binary message: {} bytes, converting to string",
                        data.len()
                    ))
                    .dim()
                );
                let config_json = data.to_vec();

                println!("{}", style("✓ Received configuration").green());

                let config: BoardConfig = serde_json::from_str(String::from_utf8_lossy(&config_json.as_slice()).as_ref())
                    .context("Failed to parse board configuration JSON")?;

                config
                    .validate()
                    .context("Board configuration validation failed")?;

                println!("{}", style("✓ Configuration validated").green());
                println!();

                return Ok(config);
            }
            Message::Ping(data) => {
                println!("{}", style("Received PING, sending PONG").dim());
                write
                    .send(Message::Pong(data))
                    .await
                    .context("Failed to send PONG")?;
            }
            Message::Pong(_) => {
                println!("{}", style("Received PONG").dim());
            }
            Message::Close(frame) => {
                return Err(anyhow!("Server closed connection: {:?}", frame));
            }
            Message::Frame(_) => return Err(anyhow!("Received raw frame, unexpected")),
        }
    }
}

/// Upload image to PhotoFrame with PFR1 format
pub async fn upload_image(url: Option<&str>, file_path: &str, orientation: u8) -> Result<()> {
    let ws_url = url.unwrap_or(DEFAULT_WS_URL);

    // Validate orientation
    if orientation > 3 {
        return Err(anyhow!("Orientation must be 0-3 (got: {})", orientation));
    }

    // Read image file
    println!(
        "{}",
        style(format!("Reading image file: {}", file_path)).dim()
    );
    let image_data = fs::read(file_path)
        .await
        .context("Failed to read image file")?;

    println!(
        "{}",
        style(format!("Image file size: {} bytes", image_data.len())).dim()
    );

    // Validate PFR1 file before upload
    println!("{}", style("Validating PFR1 file...").dim());

    // Validate PFR1 file before upload
    validate_bin_file(&image_data).map_err(|e| anyhow!("File validation failed: {}", e))?;
    println!("{}", style("✓ File validation passed").green());

    // Extract filename from path
    let filename = std::path::Path::new(file_path)
        .file_name()
        .ok_or_else(|| anyhow!("Invalid file path"))?
        .to_string_lossy()
        .to_string();

    // Validate it's a .pfr1 file
    if !filename.ends_with(".pfr1") {
        return Err(anyhow!("File must be a .pfr1 image (got: {})", filename));
    }

    // Get current timestamp
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .context("Failed to get system time")?
        .as_secs() as u32;

    // Session token (in real implementation, could be more secure)
    let token = format!("{:08x}", timestamp);

    println!("{}", style(format!("Connecting to {}...", ws_url)).dim());

    // Connect to WebSocket
    let (ws_stream, _) = timeout(
        Duration::from_secs(CONNECTION_TIMEOUT_SECS),
        connect_async(ws_url),
    )
    .await
    .context("Connection timeout")?
    .context("Failed to connect to WebSocket")?;

    println!("{}", style("✓ Connected").green());

    let (mut write, mut read) = ws_stream.split();

    // Send upload initiation message
    #[derive(Serialize)]
    struct UploadInit {
        #[serde(rename = "type")]
        msg_type: String,
        filename: String,
        token: String,
        timestamp: u32,
        orientation: u8,
    }

    let init_msg = UploadInit {
        msg_type: "init".to_string(),
        filename: filename.clone(),
        token: token.clone(),
        timestamp,
        orientation, // Use the orientation parameter passed to the function
    };

    let init_json =
        serde_json::to_string(&init_msg).context("Failed to serialize upload init message")?;

    println!(
        "{}",
        style(format!("Sending upload init: {}", init_json)).dim()
    );
    write
        .send(Message::Text(init_json.into()))
        .await
        .context("Failed to send upload init")?;

    // Wait for server ready response (handle PING/PONG)
    println!("{}", style("Waiting for server ready...").dim());
    let session_id = loop {
        let ready_response = timeout(Duration::from_secs(RESPONSE_TIMEOUT_SECS), read.next())
            .await
            .context("Response timeout waiting for ready")?
            .ok_or_else(|| anyhow!("Connection closed by server"))?
            .context("Failed to receive ready response")?;

        match ready_response {
            Message::Text(text) => {
                #[derive(Deserialize)]
                struct ReadyResponse {
                    #[serde(rename = "type")]
                    #[allow(dead_code)]
                    msg_type: String,
                    session_id: Option<u8>,
                    error: Option<String>,
                }

                let response: ReadyResponse =
                    serde_json::from_str(&text).context("Failed to parse ready response")?;

                if let Some(error) = response.error {
                    return Err(anyhow!("Server error: {}", error));
                }

                break response
                    .session_id
                    .ok_or_else(|| anyhow!("No session_id in ready response"))?;
            }
            Message::Ping(data) => {
                println!("{}", style("Received PING, sending PONG").dim());
                write
                    .send(Message::Pong(data))
                    .await
                    .context("Failed to send PONG")?;
                // Continue waiting for ready response
            }
            Message::Pong(_) => {
                println!("{}", style("Received PONG").dim());
                // Continue waiting for ready response
            }
            _ => return Err(anyhow!("Unexpected response type while waiting for ready")),
        }
    };

    println!(
        "{}",
        style(format!("✓ Server ready with session_id: {}", session_id)).green()
    );

    // Send image data in chunks
    const CHUNK_SIZE: usize = 4096;

    // Create progress bar
    let progress_bar = ProgressBar::new(image_data.len() as u64);
    progress_bar.set_style(
        ProgressStyle::default_bar()
            .template("{msg} [{bar:40.cyan/blue}] {bytes}/{total_bytes} ({percent}%) {bytes_per_sec} [{eta}]")
            .unwrap()
            .progress_chars("=>-")
    );
    progress_bar.set_message("Uploading");

    for chunk in image_data.chunks(CHUNK_SIZE) {
        write
            .send(Message::Binary(chunk.to_vec().into()))
            .await
            .context("Failed to send image chunk")?;

        // Wait for chunk ACK (handle PING/PONG)
        loop {
            let ack_response = timeout(Duration::from_secs(RESPONSE_TIMEOUT_SECS), read.next())
                .await
                .context("Timeout waiting for chunk ACK")?
                .ok_or_else(|| anyhow!("Connection closed by server"))?
                .context("Failed to receive chunk ACK")?;

            match ack_response {
                Message::Text(text) => {
                    #[derive(Deserialize)]
                    struct ChunkAck {
                        #[serde(rename = "type")]
                        #[allow(dead_code)]
                        msg_type: String,
                        received: Option<usize>,
                        error: Option<String>,
                    }

                    let ack: ChunkAck =
                        serde_json::from_str(&text).context("Failed to parse chunk ACK")?;

                    if let Some(error) = ack.error {
                        progress_bar.finish_with_message("Upload failed");
                        return Err(anyhow!("Server error during upload: {}", error));
                    }

                    if let Some(received) = ack.received {
                        progress_bar.set_position(received as u64);
                    }
                    break; // ACK received, exit loop and continue to next chunk
                }
                Message::Ping(data) => {
                    write
                        .send(Message::Pong(data))
                        .await
                        .context("Failed to send PONG")?;
                    // Continue waiting for ACK
                }
                Message::Pong(_) => {
                    // Continue waiting for ACK
                }
                _ => {
                    progress_bar.finish_with_message("Upload failed");
                    return Err(anyhow!("Unexpected ACK response"));
                }
            }
        }
    }

    progress_bar.finish_with_message(style("✓ Upload complete").green().to_string());

    // Send upload completion message
    #[derive(Serialize)]
    struct UploadEnd {
        #[serde(rename = "type")]
        msg_type: String,
    }

    let end_msg = UploadEnd {
        msg_type: "end".to_string(),
    };

    let end_json =
        serde_json::to_string(&end_msg).context("Failed to serialize upload end message")?;

    println!("{}", style("Sending upload completion...").dim());
    write
        .send(Message::Text(end_json.into()))
        .await
        .context("Failed to send upload end")?;

    // Wait for final response (handle PING/PONG)
    println!("{}", style("Waiting for final response...").dim());
    loop {
        let final_response = timeout(Duration::from_secs(RESPONSE_TIMEOUT_SECS), read.next())
            .await
            .context("Response timeout waiting for final response")?
            .ok_or_else(|| anyhow!("Connection closed by server"))?
            .context("Failed to receive final response")?;

        match final_response {
            Message::Text(text) => {
                #[derive(Deserialize)]
                struct FinalResponse {
                    #[serde(rename = "type")]
                    #[allow(dead_code)]
                    msg_type: String,
                    message: Option<String>,
                    error: Option<String>,
                }

                let response: FinalResponse =
                    serde_json::from_str(&text).context("Failed to parse final response")?;

                if let Some(error) = response.error {
                    return Err(anyhow!("Upload failed: {}", error));
                }

                println!(
                    "{}",
                    style(format!(
                        "✓ Upload successful: {}",
                        response.message.unwrap_or_default()
                    ))
                    .green()
                );
                break; // Success, exit loop
            }
            Message::Ping(data) => {
                write
                    .send(Message::Pong(data))
                    .await
                    .context("Failed to send PONG")?;
                // Continue waiting for final response
            }
            Message::Pong(_) => {
                // Continue waiting for final response
            }
            _ => return Err(anyhow!("Unexpected final response type")),
        }
    }

    Ok(())
}
