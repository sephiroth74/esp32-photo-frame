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

/// WebSocket message types for PhotoFrame device communication
use anyhow::{Result, anyhow};
use console::style;
use photoframe_lib::DisplayType;
use serde::{Deserialize, Serialize};

/// Board configuration returned by GET_CONFIG command
#[derive(Debug, Deserialize)]
pub struct BoardConfig {
    pub board: String,
    pub flash_size: u32,
    pub display_type: DisplayType,
    pub display_width: u16,
    pub display_height: u16,
    pub display_rotation: u8,
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
        if self.display_rotation > 3 {
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
        println!("  Rotation: {}", self.display_rotation.to_string());
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

/// Upload initiation message sent to server
#[derive(Serialize)]
pub struct UploadInit {
    #[serde(rename = "type")]
    pub msg_type: String,
    pub filename: String,
    pub token: String,
    pub timestamp: u32,
    pub orientation: u8,
}

/// Ready response from server
#[derive(Deserialize)]
pub struct ReadyResponse {
    #[serde(rename = "type")]
    #[allow(dead_code)]
    pub msg_type: String,
    pub session_id: Option<u8>,
    pub error: Option<String>,
}

/// Chunk acknowledgement from server
#[derive(Deserialize)]
pub struct ChunkAck {
    #[serde(rename = "type")]
    #[allow(dead_code)]
    pub msg_type: String,
    pub received: Option<usize>,
    pub error: Option<String>,
}

/// Upload completion message sent to server
#[derive(Serialize)]
pub struct UploadEnd {
    #[serde(rename = "type")]
    pub msg_type: String,
}

/// Final response from server after upload
#[derive(Deserialize)]
pub struct FinalResponse {
    #[serde(rename = "type")]
    #[allow(dead_code)]
    pub msg_type: String,
    pub success: bool,
    pub message: Option<String>,
    pub error: Option<String>,
}

/// Shutdown command sent to server
#[derive(Serialize)]
pub struct ShutdownCommand {
    #[serde(rename = "type")]
    pub msg_type: String,
}

/// Shutdown acknowledgement response from server
#[derive(Deserialize)]
pub struct ShutdownResponse {
    #[serde(rename = "type")]
    #[allow(dead_code)]
    pub msg_type: String,
    pub message: Option<String>,
    pub error: Option<String>,
}

/// Generic error message from server
#[derive(Deserialize)]
pub struct ErrorMessage {
    #[serde(rename = "type")]
    #[allow(dead_code)]
    pub msg_type: String,
    pub message: Option<String>,
    pub error: Option<String>,
}
