use super::types::PairedImages;
use crate::report::ImageInfo;

/// Pair portrait images into groups of two
/// Returns (paired_images, unpaired_image)
/// unpaired_image is Some if there's an odd number of portraits
pub fn pair_portraits(portraits: Vec<ImageInfo>) -> (Vec<PairedImages>, Option<ImageInfo>) {
    let mut paired = Vec::new();
    let mut iter = portraits.into_iter();

    // Pair images two by two
    loop {
        match (iter.next(), iter.next()) {
            (Some(first), Some(second)) => {
                paired.push(PairedImages { first, second });
            }
            (Some(unpaired), None) => {
                // Odd number of images - last one is unpaired
                return (paired, Some(unpaired));
            }
            (None, None) => {
                // Even number of images - all paired
                return (paired, None);
            }
            (None, Some(_)) => unreachable!("Iterator returned None then Some"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::report::{ImageOrientation, RotationDegrees};
    use std::path::PathBuf;

    fn create_test_image_info(name: &str) -> ImageInfo {
        ImageInfo {
            path: PathBuf::from(name),
            rotation: RotationDegrees::Deg0,
            orientation: ImageOrientation::Landscape,
            people_detected: None,
        }
    }

    #[test]
    fn test_pair_even_number() {
        let images = vec![
            create_test_image_info("img1.jpg"),
            create_test_image_info("img2.jpg"),
            create_test_image_info("img3.jpg"),
            create_test_image_info("img4.jpg"),
        ];

        let (paired, unpaired) = pair_portraits(images);

        assert_eq!(paired.len(), 2);
        assert!(unpaired.is_none());
        assert_eq!(paired[0].first.path.to_str().unwrap(), "img1.jpg");
        assert_eq!(paired[0].second.path.to_str().unwrap(), "img2.jpg");
        assert_eq!(paired[1].first.path.to_str().unwrap(), "img3.jpg");
        assert_eq!(paired[1].second.path.to_str().unwrap(), "img4.jpg");
    }

    #[test]
    fn test_pair_odd_number() {
        let images = vec![
            create_test_image_info("img1.jpg"),
            create_test_image_info("img2.jpg"),
            create_test_image_info("img3.jpg"),
        ];

        let (paired, unpaired) = pair_portraits(images);

        assert_eq!(paired.len(), 1);
        assert!(unpaired.is_some());
        assert_eq!(paired[0].first.path.to_str().unwrap(), "img1.jpg");
        assert_eq!(paired[0].second.path.to_str().unwrap(), "img2.jpg");
        assert_eq!(unpaired.unwrap().path.to_str().unwrap(), "img3.jpg");
    }

    #[test]
    fn test_pair_empty() {
        let images: Vec<ImageInfo> = vec![];
        let (paired, unpaired) = pair_portraits(images);

        assert_eq!(paired.len(), 0);
        assert!(unpaired.is_none());
    }

    #[test]
    fn test_pair_single() {
        let images = vec![create_test_image_info("img1.jpg")];
        let (paired, unpaired) = pair_portraits(images);

        assert_eq!(paired.len(), 0);
        assert!(unpaired.is_some());
        assert_eq!(unpaired.unwrap().path.to_str().unwrap(), "img1.jpg");
    }
}
