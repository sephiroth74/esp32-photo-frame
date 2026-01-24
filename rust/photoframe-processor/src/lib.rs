// Library exports for reuse by GUI and other applications
#[cfg(feature = "bluetooth")]
pub mod bluetooth;
pub mod cli;
pub mod config_file;
pub mod image_processing;
pub mod json_output;
pub mod utils;

// Re-export commonly used types
#[cfg(feature = "bluetooth")]
pub use bluetooth::{scan_devices, upload_image_with_dimensions};
pub use cli::{ColorType, DitherMethod, OutputType, TargetOrientation};
pub use image_processing::{
    ImageType, ProcessingConfig, ProcessingEngine, ProcessingResult, ProcessingType,
};
pub use json_output::JsonMessage;
