use anyhow::Result;
use rayon::prelude::*;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::{Duration, Instant};

use crate::utils::ProcessingStats;

/// Batch processing statistics and progress tracking
pub struct BatchProcessor {
    pub total_files: usize,
    pub processed_count: AtomicUsize,
    pub start_time: Instant,
}

impl BatchProcessor {
    pub fn new(total_files: usize) -> Self {
        Self {
            total_files,
            processed_count: AtomicUsize::new(0),
            start_time: Instant::now(),
        }
    }

    /// Increment processed count and return current count
    pub fn increment(&self) -> usize {
        self.processed_count.fetch_add(1, Ordering::Relaxed) + 1
    }

    /// Get current progress (0.0 to 1.0)
    pub fn progress(&self) -> f64 {
        if self.total_files == 0 {
            1.0
        } else {
            (self.processed_count.load(Ordering::Relaxed) as f64) / (self.total_files as f64)
        }
    }

    /// Get estimated time remaining
    pub fn eta(&self) -> Option<Duration> {
        let processed = self.processed_count.load(Ordering::Relaxed);
        if processed == 0 {
            return None;
        }

        let elapsed = self.start_time.elapsed();
        let remaining = self.total_files - processed;
        
        if remaining == 0 {
            return Some(Duration::new(0, 0));
        }

        let time_per_item = elapsed / processed as u32;
        Some(time_per_item * remaining as u32)
    }

    /// Get processing speed (items per second)
    pub fn items_per_second(&self) -> f64 {
        let processed = self.processed_count.load(Ordering::Relaxed);
        if processed == 0 {
            return 0.0;
        }

        let elapsed_secs = self.start_time.elapsed().as_secs_f64();
        if elapsed_secs == 0.0 {
            return 0.0;
        }

        processed as f64 / elapsed_secs
    }
}

/// Process multiple files in parallel
pub fn process_files_parallel<T, F, P>(
    files: &[PathBuf],
    process_fn: F,
    progress_callback: P,
) -> Vec<Result<T>>
where
    T: Send,
    F: Fn(&Path) -> Result<T> + Send + Sync,
    P: Fn(usize, f64, Option<Duration>) + Send + Sync,
{
    let processor = BatchProcessor::new(files.len());

    files
        .par_iter()
        .map(|file_path| {
            let result = process_fn(file_path);
            
            let completed = processor.increment();
            let progress = processor.progress();
            let eta = processor.eta();
            
            progress_callback(completed, progress, eta);
            
            result
        })
        .collect()
}

/// Group files by type (portrait vs landscape)
pub fn group_by_orientation(
    files: &[PathBuf],
    is_portrait_fn: impl Fn(&Path) -> Result<bool>,
) -> Result<(Vec<PathBuf>, Vec<PathBuf>)> {
    let mut portraits = Vec::new();
    let mut landscapes = Vec::new();
    
    for file in files {
        match is_portrait_fn(file) {
            Ok(true) => portraits.push(file.clone()),
            Ok(false) => landscapes.push(file.clone()),
            Err(e) => {
                eprintln!("Warning: Failed to determine orientation for {}: {}", 
                         file.display(), e);
                // Default to landscape if orientation detection fails
                landscapes.push(file.clone());
            }
        }
    }
    
    Ok((portraits, landscapes))
}

/// Calculate comprehensive processing statistics
pub fn calculate_final_stats(
    results: &[Result<()>],
    processing_time: Duration,
    portraits_found: usize,
    landscapes_found: usize,
    portraits_combined: usize,
) -> ProcessingStats {
    let successful = results.iter().filter(|r| r.is_ok()).count();
    let failed = results.len() - successful;
    
    ProcessingStats {
        total_files: results.len(),
        successful,
        failed,
        portraits_found,
        landscapes_found,
        portraits_combined,
        total_duration: processing_time,
    }
}

/// Validate batch processing inputs
pub fn validate_batch_inputs(
    input_dirs: &[PathBuf],
    output_dir: &Path,
    extensions: &[String],
) -> Result<()> {
    // Check input directories exist
    for dir in input_dirs {
        if !dir.exists() {
            return Err(anyhow::anyhow!("Input directory does not exist: {}", dir.display()));
        }
        if !dir.is_dir() {
            return Err(anyhow::anyhow!("Input path is not a directory: {}", dir.display()));
        }
    }
    
    // Validate extensions
    if extensions.is_empty() {
        return Err(anyhow::anyhow!("No file extensions specified"));
    }
    
    // Check if we can create output directory
    if let Some(parent) = output_dir.parent() {
        if !parent.exists() {
            return Err(anyhow::anyhow!(
                "Output directory parent does not exist: {}", 
                parent.display()
            ));
        }
    }
    
    Ok(())
}

/// Shuffle a vector randomly (for portrait image randomization)
pub fn shuffle_vec<T>(mut vec: Vec<T>) -> Vec<T> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    use std::time::{SystemTime, UNIX_EPOCH};
    
    // Simple pseudo-random shuffle using system time as seed
    let seed = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    
    let mut hasher = DefaultHasher::new();
    seed.hash(&mut hasher);
    let mut rng_state = hasher.finish();
    
    for i in (1..vec.len()).rev() {
        // Simple linear congruential generator
        rng_state = rng_state.wrapping_mul(1103515245).wrapping_add(12345);
        let j = (rng_state as usize) % (i + 1);
        vec.swap(i, j);
    }
    
    vec
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn test_batch_processor_progress() {
        let processor = BatchProcessor::new(10);
        
        assert_eq!(processor.progress(), 0.0);
        
        processor.increment();
        assert!((processor.progress() - 0.1).abs() < 0.01);
        
        // Increment 9 more times
        for _ in 0..9 {
            processor.increment();
        }
        assert!((processor.progress() - 1.0).abs() < 0.01);
    }

    #[test]
    fn test_batch_processor_eta() {
        let processor = BatchProcessor::new(4);
        
        // No ETA before any processing
        assert!(processor.eta().is_none());
        
        // Process one item with a small delay
        thread::sleep(Duration::from_millis(10));
        processor.increment();
        
        let eta = processor.eta();
        assert!(eta.is_some());
        assert!(eta.unwrap() > Duration::new(0, 0));
    }

    #[test]
    fn test_group_by_orientation() {
        let files = vec![
            PathBuf::from("portrait1.jpg"),
            PathBuf::from("landscape1.jpg"),
            PathBuf::from("portrait2.jpg"),
        ];
        
        let is_portrait = |path: &Path| -> Result<bool> {
            Ok(path.to_string_lossy().contains("portrait"))
        };
        
        let (portraits, landscapes) = group_by_orientation(&files, is_portrait).unwrap();
        
        assert_eq!(portraits.len(), 2);
        assert_eq!(landscapes.len(), 1);
    }

    #[test]
    fn test_shuffle_vec() {
        let original = vec![1, 2, 3, 4, 5];
        let shuffled = shuffle_vec(original.clone());
        
        // Should have same length
        assert_eq!(original.len(), shuffled.len());
        
        // Should contain same elements (but possibly different order)
        let mut orig_sorted = original.clone();
        let mut shuf_sorted = shuffled.clone();
        orig_sorted.sort();
        shuf_sorted.sort();
        assert_eq!(orig_sorted, shuf_sorted);
    }

    #[test]
    fn test_calculate_final_stats() {
        let results = vec![
            Ok(()),
            Err(anyhow::anyhow!("error")),
            Ok(()),
        ];
        
        let stats = calculate_final_stats(
            &results,
            Duration::from_secs(10),
            5, // portraits_found
            8, // landscapes_found
            2, // portraits_combined
        );
        
        assert_eq!(stats.total_files, 3);
        assert_eq!(stats.successful, 2);
        assert_eq!(stats.failed, 1);
        assert_eq!(stats.portraits_found, 5);
        assert_eq!(stats.landscapes_found, 8);
        assert_eq!(stats.portraits_combined, 2);
        assert!((stats.success_rate() - 66.67).abs() < 0.1);
    }
}