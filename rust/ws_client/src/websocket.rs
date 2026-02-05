use anyhow::{Context, Result, anyhow};
use console::style;
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use std::time::Duration;
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
        .send(Message::Text("GET_CONFIG".to_string()))
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
                let config_json =
                    String::from_utf8(data).context("Binary data is not valid UTF-8")?;

                println!("{}", style("✓ Received configuration").green());

                let config: BoardConfig = serde_json::from_str(&config_json)
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

/// Upload image to PhotoFrame (placeholder for future implementation)
pub async fn upload_image(_url: Option<&str>, _file_path: &str) -> Result<()> {
    println!(
        "{}",
        style("Upload functionality not yet implemented").yellow()
    );
    println!("This will be implemented in a future version.");
    Ok(())
}
