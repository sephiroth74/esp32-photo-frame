#[cfg(feature = "ai")]
pub mod detection;

/// Face detection result compatible with existing smart crop system
#[derive(Debug, Clone)]
pub struct Face {
    /// Bounding box coordinates
    pub x1: u32,
    pub y1: u32,
    pub x2: u32,
    pub y2: u32,
    /// Detection confidence score
    pub confidence: f32,
}
