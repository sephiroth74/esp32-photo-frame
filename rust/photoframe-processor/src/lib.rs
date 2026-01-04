// Library exports for reuse by GUI and other applications
pub mod cli;
pub mod image_processing;
pub mod utils;
pub mod json_output;

// Re-export commonly used types
pub use image_processing::{
    ProcessingConfig, ProcessingEngine, ProcessingResult, ProcessingType, ImageType, SkipReason,
};
pub use cli::{ColorType, DitherMethod, OutputType, TargetOrientation};
pub use json_output::JsonMessage;
