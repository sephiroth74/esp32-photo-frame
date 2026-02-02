use crate::report::ImageInfo;

/// A single image to be processed individually
#[derive(Debug, Clone)]
pub struct SingleImage {
    pub info: ImageInfo,
}

/// A pair of portrait images to be combined into one landscape image
#[derive(Debug, Clone)]
pub struct PairedImages {
    pub first: ImageInfo,
    pub second: ImageInfo,
}

/// Groups of images organized by processing strategy
#[derive(Debug)]
pub struct ProcessingPlan {
    /// Images to be processed individually (landscape or all if no_pairing is true)
    pub single_images: Vec<SingleImage>,
    /// Pairs of portrait images to be combined (empty if no_pairing is true)
    pub paired_images: Vec<PairedImages>,
    /// Portrait images that couldn't be paired (only when no_pairing is false)
    pub unpaired_images: Vec<ImageInfo>,
}

impl SingleImage {
    pub fn new(info: ImageInfo) -> Self {
        Self { info }
    }
}

impl ProcessingPlan {
    pub fn new() -> Self {
        Self {
            single_images: Vec::new(),
            paired_images: Vec::new(),
            unpaired_images: Vec::new(),
        }
    }

    /// Total number of output images that will be generated
    pub fn output_count(&self) -> usize {
        self.single_images.len() + self.paired_images.len()
    }

    /// Total number of input images being processed
    pub fn input_count(&self) -> usize {
        self.single_images.len() + (self.paired_images.len() * 2)
    }
}
