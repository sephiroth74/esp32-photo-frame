//! Face structure and related types

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Represents a detected face in an image
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Face {
    /// Bounding box [x1, y1, x2, y2]
    pub bbox: [f32; 4],

    /// Detection confidence score
    pub det_score: f32,

    /// Keypoints (landmarks) - optional
    /// Shape: (5, 2) for 5 facial landmarks
    pub kps: Option<Vec<[f32; 2]>>,

    /// Face embedding vector - optional
    pub embedding: Option<Vec<f32>>,

    /// Gender: 0 = Female, 1 = Male - optional
    pub gender: Option<i32>,

    /// Estimated age - optional
    pub age: Option<i32>,

    /// Additional attributes
    #[serde(skip_serializing_if = "Option::is_none")]
    pub attributes: Option<HashMap<String, serde_json::Value>>,
}

impl Face {
    /// Create a new face with minimal information
    pub fn new(bbox: [f32; 4], det_score: f32) -> Self {
        Face {
            bbox,
            det_score,
            kps: None,
            embedding: None,
            gender: None,
            age: None,
            attributes: None,
        }
    }

    /// Get the width of the bounding box
    pub fn width(&self) -> f32 {
        self.bbox[2] - self.bbox[0]
    }

    /// Get the height of the bounding box
    pub fn height(&self) -> f32 {
        self.bbox[3] - self.bbox[1]
    }

    /// Get the area of the bounding box
    pub fn area(&self) -> f32 {
        self.width() * self.height()
    }

    /// Get human-readable gender string
    pub fn sex(&self) -> Option<&'static str> {
        self.gender.map(|g| if g == 1 { "M" } else { "F" })
    }

    /// Compute L2 norm of the embedding
    pub fn embedding_norm(&self) -> Option<f32> {
        self.embedding.as_ref().map(|emb| {
            let sum: f32 = emb.iter().map(|x| x * x).sum();
            sum.sqrt()
        })
    }

    /// Get normalized embedding (unit vector)
    pub fn normed_embedding(&self) -> Option<Vec<f32>> {
        self.embedding.as_ref().and_then(|emb| {
            self.embedding_norm().map(|norm| {
                if norm > 0.0 {
                    emb.iter().map(|x| x / norm).collect()
                } else {
                    emb.clone()
                }
            })
        })
    }
}
