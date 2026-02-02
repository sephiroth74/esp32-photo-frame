//! Utility functions for image processing and detection

use ndarray::{Array3, Array4};

/// Convert image to normalized tensor suitable for ONNX inference
///
/// # Arguments
/// * `img_data` - Raw image data in RGB format (height, width, 3)
/// * `target_size` - Target size (height, width)
///
/// # Returns
/// Normalized tensor with shape (1, 3, height, width) for ONNX input
pub fn prepare_input(img_data: Array3<u8>, target_size: (i64, i64)) -> Array4<f32> {
    let (target_h, target_w) = (target_size.0 as usize, target_size.1 as usize);
    let (_h, _w, _c) = img_data.dim();

    // Resize image to target size
    let resized = resize_image(img_data, target_h, target_w);

    // Convert to float and normalize
    let mut tensor = Array4::zeros((1, 3, target_h, target_w));

    for c in 0..3 {
        for y in 0..target_h {
            for x in 0..target_w {
                let pixel = resized[[y, x, c]] as f32;
                // Normalize from [0, 255] to [0, 1]
                tensor[[0, c, y, x]] = pixel / 255.0;
            }
        }
    }

    tensor
}

/// Simple bilinear resize (nearest neighbor for simplicity)
fn resize_image(img: Array3<u8>, new_h: usize, new_w: usize) -> Array3<u8> {
    let (h, w, c) = img.dim();
    let mut result = Array3::zeros((new_h, new_w, c));

    let h_ratio = h as f32 / new_h as f32;
    let w_ratio = w as f32 / new_w as f32;

    for new_y in 0..new_h {
        for new_x in 0..new_w {
            let src_y = ((new_y as f32) * h_ratio) as usize;
            let src_x = ((new_x as f32) * w_ratio) as usize;

            let src_y = src_y.min(h - 1);
            let src_x = src_x.min(w - 1);

            for ch in 0..c {
                result[[new_y, new_x, ch]] = img[[src_y, src_x, ch]];
            }
        }
    }

    result
}

/// Non-Maximum Suppression (NMS)
///
/// Filters overlapping bounding boxes, keeping only the highest confidence ones
///
/// # Arguments
/// * `dets` - Array of detections: [x1, y1, x2, y2, score]
/// * `thresh` - IoU threshold for suppression
///
/// # Returns
/// Indices of boxes to keep
pub fn nms(dets: &[[f32; 5]], thresh: f32) -> Vec<usize> {
    if dets.is_empty() {
        return Vec::new();
    }

    let mut indices: Vec<usize> = (0..dets.len()).collect();
    indices.sort_by(|&a, &b| {
        dets[b][4]
            .partial_cmp(&dets[a][4])
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    let mut keep = Vec::new();
    let mut suppress = vec![false; dets.len()];

    for &i in &indices {
        if suppress[i] {
            continue;
        }

        keep.push(i);

        for &j in &indices {
            if i != j && !suppress[j] {
                let iou = compute_iou(&dets[i], &dets[j]);
                if iou > thresh {
                    suppress[j] = true;
                }
            }
        }
    }

    keep
}

/// Compute Intersection over Union (IoU)
fn compute_iou(box1: &[f32; 5], box2: &[f32; 5]) -> f32 {
    let x1_min = box1[0].max(box2[0]);
    let y1_min = box1[1].max(box2[1]);
    let x1_max = box1[2].min(box2[2]);
    let y1_max = box1[3].min(box2[3]);

    if x1_max < x1_min || y1_max < y1_min {
        return 0.0;
    }

    let intersection = (x1_max - x1_min) * (y1_max - y1_min);
    let area1 = (box1[2] - box1[0]) * (box1[3] - box1[1]);
    let area2 = (box2[2] - box2[0]) * (box2[3] - box2[1]);
    let union = area1 + area2 - intersection;

    intersection / union
}

/// Compute similarity distance between two embeddings (L2 norm)
pub fn embedding_distance(emb1: &[f32], emb2: &[f32]) -> f32 {
    let mut sum = 0.0;
    for (a, b) in emb1.iter().zip(emb2.iter()) {
        let diff = a - b;
        sum += diff * diff;
    }
    sum.sqrt()
}

/// Compute cosine similarity between two embeddings
pub fn cosine_similarity(emb1: &[f32], emb2: &[f32]) -> f32 {
    let mut dot = 0.0;
    let mut norm1 = 0.0;
    let mut norm2 = 0.0;

    for (a, b) in emb1.iter().zip(emb2.iter()) {
        dot += a * b;
        norm1 += a * a;
        norm2 += b * b;
    }

    let norm1 = norm1.sqrt();
    let norm2 = norm2.sqrt();

    if norm1 == 0.0 || norm2 == 0.0 {
        0.0
    } else {
        dot / (norm1 * norm2)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_nms_basic() {
        let dets = [
            [10.0, 10.0, 20.0, 20.0, 0.9],
            [11.0, 11.0, 21.0, 21.0, 0.8],
            [100.0, 100.0, 110.0, 110.0, 0.95],
        ];

        let keep = nms(&dets, 0.5);
        assert_eq!(keep.len(), 2); // Should keep the two non-overlapping boxes
    }

    #[test]
    fn test_cosine_similarity() {
        let v1 = [1.0, 0.0];
        let v2 = [1.0, 0.0];
        assert!((cosine_similarity(&v1, &v2) - 1.0).abs() < 0.001);

        let v3 = [-1.0, 0.0];
        assert!((cosine_similarity(&v1, &v3) + 1.0).abs() < 0.001);
    }
}
