// ESP32 Photo Frame
// Copyright (C) 2026 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

use crate::messages::{
    BoardConfig, ChunkAck, ErrorMessage, FinalResponse, ReadyResponse, ShutdownCommand,
    ShutdownResponse, UploadEnd, UploadInit,
};
use anyhow::{Context, Result, anyhow};
use console::style;
use futures_util::{SinkExt, StreamExt};
use indicatif::{ProgressBar, ProgressStyle};
use photoframe_lib::validate_bin_file;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::fs;
use tokio::net::TcpStream;
use tokio::time::timeout;
use tokio_tungstenite::{MaybeTlsStream, WebSocketStream, connect_async, tungstenite::Message};

/// Type alias for WebSocket stream
pub type WsStream = WebSocketStream<MaybeTlsStream<TcpStream>>;

/// Connection timeout in seconds
const CONNECTION_TIMEOUT_SECS: u16 = 10;

/// Response timeout in seconds  
const RESPONSE_TIMEOUT_SECS: u16 = 5;

const DISPLAY_REFRESH_RESPONSE_TIMEOUT_SECS: u16 = 40;

/// Connect to PhotoFrame WebSocket
pub async fn connect(ws_url: &str) -> Result<WsStream> {
    let (ws_stream, _) = timeout(
        Duration::from_secs(CONNECTION_TIMEOUT_SECS as u64),
        connect_async(ws_url),
    )
    .await
    .context("Connection timeout")?
    .context("Failed to connect to WebSocket")?;

    Ok(ws_stream)
}

/// Get board configuration using existing connection
pub async fn get_config(ws_stream: &mut WsStream) -> Result<BoardConfig> {
    // Send GET_CONFIG command
    ws_stream
        .send(Message::Text("GET_CONFIG".into()))
        .await
        .context("Failed to send GET_CONFIG command")?;

    loop {
        // Wait for response with timeout
        let response = timeout(
            Duration::from_secs(RESPONSE_TIMEOUT_SECS as u64),
            ws_stream.next(),
        )
        .await
        .context("Response timeout")?
        .ok_or_else(|| anyhow!("Connection closed by server"))?
        .context("Failed to receive response")?;

        match response {
            Message::Text(text) => {
                // Check if it's an error message first
                if let Ok(error_msg) = serde_json::from_str::<ErrorMessage>(&text) {
                    if error_msg.msg_type == "error" {
                        let error_text = error_msg
                            .message
                            .or(error_msg.error)
                            .unwrap_or_else(|| "Unknown error".to_string());
                        println!(
                            "{}",
                            style(format!("✗ Server error: {}", error_text))
                                .bold()
                                .red()
                        );
                        return Err(anyhow!("Server error: {}", error_text));
                    }
                }

                let config: BoardConfig = serde_json::from_str(&text)
                    .context("Failed to parse board configuration JSON")?;

                config
                    .validate()
                    .context("Board configuration validation failed")?;

                return Ok(config);
            }
            Message::Binary(data) => {
                let config: BoardConfig =
                    serde_json::from_str(String::from_utf8_lossy(&data).as_ref())
                        .context("Failed to parse board configuration JSON")?;

                config
                    .validate()
                    .context("Board configuration validation failed")?;

                return Ok(config);
            }
            Message::Ping(data) => {
                ws_stream
                    .send(Message::Pong(data))
                    .await
                    .context("Failed to send PONG")?;
            }
            Message::Pong(_) => {}
            Message::Close(frame) => {
                return Err(anyhow!("Server closed connection: {:?}", frame));
            }
            Message::Frame(_) => return Err(anyhow!("Received raw frame, unexpected")),
        }
    }
}

/// Upload image using existing WebSocket connection
pub async fn upload_image_with_connection(
    ws_stream: &mut WsStream,
    file_path: &str,
    orientation: u8,
) -> Result<()> {
    // Validate orientation
    if orientation > 3 {
        return Err(anyhow!("Orientation must be 0-3 (got: {})", orientation));
    }

    // Read image file
    let image_data = fs::read(file_path)
        .await
        .context("Failed to read image file")?;

    // Validate PFR1 file before upload
    validate_bin_file(&image_data).map_err(|e| anyhow!("File validation failed: {}", e))?;

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

    // Session token
    let token = format!("{:08x}", timestamp);

    let (mut write, mut read) = ws_stream.split();

    // Send upload initiation message
    let init_msg = UploadInit {
        msg_type: "init".to_string(),
        filename: filename.clone(),
        token: token.clone(),
        timestamp,
        orientation,
    };

    let init_json =
        serde_json::to_string(&init_msg).context("Failed to serialize upload init message")?;

    write
        .send(Message::Text(init_json.into()))
        .await
        .context("Failed to send upload init")?;

    // Wait for ready response
    loop {
        let response = timeout(
            Duration::from_secs(RESPONSE_TIMEOUT_SECS as u64),
            read.next(),
        )
        .await
        .context("Timeout waiting for ready response")?
        .ok_or_else(|| anyhow!("Connection closed by server"))?
        .context("Failed to receive ready response")?;

        match response {
            Message::Text(text) => {
                // Check if it's a generic error message first
                if let Ok(error_msg) = serde_json::from_str::<ErrorMessage>(&text) {
                    if error_msg.msg_type == "error" {
                        let error_text = error_msg
                            .message
                            .or(error_msg.error)
                            .unwrap_or_else(|| "Unknown error".to_string());
                        println!(
                            "{}",
                            style(format!("✗ Server error: {}", error_text))
                                .bold()
                                .red()
                        );
                        return Err(anyhow!("Server error: {}", error_text));
                    }
                }

                let ready: ReadyResponse =
                    serde_json::from_str(&text).context("Failed to parse ready response")?;

                if let Some(error) = ready.error {
                    println!(
                        "{}",
                        style(format!("✗ Server error: {}", error)).bold().red()
                    );
                    return Err(anyhow!("Server rejected upload: {}", error));
                }
                break;
            }
            Message::Ping(data) => {
                write
                    .send(Message::Pong(data))
                    .await
                    .context("Failed to send PONG")?;
            }
            Message::Pong(_) => {}
            _ => return Err(anyhow!("Unexpected ready response type")),
        }
    }

    // Upload binary data in chunks
    const CHUNK_SIZE: usize = 4096;
    let total_size = image_data.len();

    // Create progress bar
    let progress_bar = ProgressBar::new(total_size as u64);
    progress_bar.set_style(
        ProgressStyle::default_bar()
            .template("{msg} [{bar:40.cyan/blue}] {bytes}/{total_bytes} ({percent}%) {bytes_per_sec} ETA: {eta}")
            .unwrap()
            .progress_chars("=>-"),
    );
    progress_bar.set_message("Uploading");

    for chunk in image_data.chunks(CHUNK_SIZE) {
        write
            .send(Message::Binary(chunk.to_vec().into()))
            .await
            .context("Failed to send chunk")?;

        // Wait for chunk ACK
        loop {
            let ack_response = timeout(
                Duration::from_secs(RESPONSE_TIMEOUT_SECS as u64),
                read.next(),
            )
            .await
            .context("Timeout waiting for chunk ACK")?
            .ok_or_else(|| anyhow!("Connection closed by server"))?
            .context("Failed to receive chunk ACK")?;

            match ack_response {
                Message::Text(text) => {
                    // Check if it's a generic error message first
                    if let Ok(error_msg) = serde_json::from_str::<ErrorMessage>(&text) {
                        if error_msg.msg_type == "error" {
                            let error_text = error_msg
                                .message
                                .or(error_msg.error)
                                .unwrap_or_else(|| "Unknown error".to_string());
                            progress_bar.finish_with_message("Upload failed");
                            println!(
                                "{}",
                                style(format!("✗ Server error: {}", error_text))
                                    .bold()
                                    .red()
                            );
                            return Err(anyhow!("Server error: {}", error_text));
                        }
                    }

                    let ack: ChunkAck =
                        serde_json::from_str(&text).context("Failed to parse chunk ACK")?;

                    if let Some(error) = ack.error {
                        progress_bar.finish_with_message("Upload failed");
                        println!(
                            "{}",
                            style(format!("✗ Server error: {}", error)).bold().red()
                        );
                        return Err(anyhow!("Server error during upload: {}", error));
                    }

                    if let Some(received) = ack.received {
                        progress_bar.set_position(received as u64);
                    }
                    break;
                }
                Message::Ping(data) => {
                    write
                        .send(Message::Pong(data))
                        .await
                        .context("Failed to send PONG")?;
                }
                Message::Pong(_) => {}
                _ => {
                    progress_bar.finish_with_message("Upload failed");
                    return Err(anyhow!("Unexpected ACK response"));
                }
            }
        }
    }

    progress_bar.finish_with_message(style("✓ Upload complete").green().to_string());

    // Send upload completion message
    let end_msg = UploadEnd {
        msg_type: "end".to_string(),
    };

    let end_json =
        serde_json::to_string(&end_msg).context("Failed to serialize upload end message")?;

    write
        .send(Message::Text(end_json.into()))
        .await
        .context("Failed to send upload end")?;

    // now show a spinner while waiting for the display to refresh and show the new image, which can take a while
    println!(
        "{}",
        style("✓ Upload completed. Waiting for display to be ready...".to_string()).green()
    );

    loop {
        let final_response = timeout(
            Duration::from_secs(DISPLAY_REFRESH_RESPONSE_TIMEOUT_SECS as u64),
            read.next(),
        )
        .await
        .context("Response timeout waiting for final response")?
        .ok_or_else(|| anyhow!("Connection closed by server"))?
        .context("Failed to receive final response")?;

        match final_response {
            Message::Text(text) => {
                // check the type first
                let v = serde_json::from_str::<serde_json::Value>(&text)
                    .context("Failed to parse final response JSON")?;
                let value = v
                    .get("type")
                    .ok_or_else(|| anyhow!("No type field in final response"))?;

                match value.as_str() {
                    Some("display_ready") => {
                        println!("{}", style("✓ Display ready".to_string()).green());
                        break; // Wait for the actual final response
                    }

                    Some("final_response") => {
                        let response: FinalResponse = serde_json::from_str(&text)
                            .context("Failed to parse final response")?;

                        if !response.success {
                            return Err(anyhow!(
                                "Upload failed: {}",
                                response
                                    .message
                                    .unwrap_or_else(|| "Unknown error".to_string())
                            ));
                        }
                        continue;
                    }

                    Some("error") => {
                        let error_msg: ErrorMessage =
                            serde_json::from_str(&text).context("Failed to parse error message")?;
                        let error_text = error_msg
                            .message
                            .or(error_msg.error)
                            .unwrap_or_else(|| "Unknown error".to_string());
                        println!(
                            "{}",
                            style(format!("✗ Server error: {}", error_text))
                                .bold()
                                .red()
                        );
                        return Err(anyhow!("Server error: {}", error_text));
                    }

                    Some(message_type) => {
                        println!(
                            "{}",
                            style(format!(
                                "Received unexpected message type: {}",
                                message_type
                            ))
                            .yellow()
                        );
                        continue; // Ignore and keep waiting for final response
                    }

                    None => {
                        println!("{}", style("Received message without type field").yellow());
                        continue; // Ignore and keep waiting for final response
                    }
                }
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
            _ => {
                return Err(anyhow!("Unexpected final response type"));
            }
        }
    }

    Ok(())
}

/// Send shutdown command using existing WebSocket connection
pub async fn send_shutdown_with_connection(ws_stream: &mut WsStream) -> Result<()> {
    let (mut write, mut read) = ws_stream.split();

    // Send shutdown command
    let shutdown_msg = ShutdownCommand {
        msg_type: "shutdown".to_string(),
    };

    let shutdown_json =
        serde_json::to_string(&shutdown_msg).context("Failed to serialize shutdown command")?;

    write
        .send(Message::Text(shutdown_json.into()))
        .await
        .context("Failed to send shutdown command")?;

    // Wait for acknowledgement
    loop {
        let response = timeout(
            Duration::from_secs(RESPONSE_TIMEOUT_SECS as u64),
            read.next(),
        )
        .await
        .context("Response timeout")?
        .ok_or_else(|| anyhow!("Connection closed by server"))?
        .context("Failed to receive response")?;

        match response {
            Message::Text(text) => {
                // Check if it's a generic error message first
                if let Ok(error_msg) = serde_json::from_str::<ErrorMessage>(&text) {
                    if error_msg.msg_type == "error" {
                        let error_text = error_msg
                            .message
                            .or(error_msg.error)
                            .unwrap_or_else(|| "Unknown error".to_string());
                        println!(
                            "{}",
                            style(format!("✗ Server error: {}", error_text))
                                .bold()
                                .red()
                        );
                        return Err(anyhow!("Server error: {}", error_text));
                    }
                }

                let response: ShutdownResponse =
                    serde_json::from_str(&text).context("Failed to parse shutdown response")?;

                if let Some(error) = response.error {
                    println!(
                        "{}",
                        style(format!("✗ Server error: {}", error)).bold().red()
                    );
                    return Err(anyhow!("Shutdown failed: {}", error));
                }
                break;
            }
            Message::Ping(data) => {
                write
                    .send(Message::Pong(data))
                    .await
                    .context("Failed to send PONG")?;
            }
            Message::Pong(_) => {}
            _ => return Err(anyhow!("Unexpected response type")),
        }
    }

    Ok(())
}
