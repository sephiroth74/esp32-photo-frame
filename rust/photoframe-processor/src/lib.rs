// Library exports for reuse by GUI and other applications
pub mod cli;
pub mod image_processing;
pub mod json_output;
pub mod utils;

// Re-export commonly used types
pub use cli::{ColorType, DitherMethod, OutputType, TargetOrientation};
pub use image_processing::{
    ImageType, ProcessingConfig, ProcessingEngine, ProcessingResult, ProcessingType,
};
pub use json_output::JsonMessage;
