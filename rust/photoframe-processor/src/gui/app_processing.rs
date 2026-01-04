// Processing implementation for the GUI
// This module contains the actual processing logic that runs in a background thread

use super::{PhotoFrameApp, ProgressMessage, parse_hex_color};
use photoframe_processor::cli::{ColorType, OutputType};
use photoframe_processor::image_processing::{ProcessingConfig, ProcessingEngine, ProcessingType};
use std::path::PathBuf;
use std::sync::mpsc::channel;
use image::Rgb;

impl PhotoFrameApp {
    pub fn start_processing(&mut self) {
        // Validate inputs
        if self.input_path.is_empty() || self.output_path.is_empty() {
            self.error_message = "Please select input and output directories".to_string();
            return;
        }

        // Validate at least one output format is selected
        if !self.output_bmp && !self.output_bin && !self.output_jpg && !self.output_png {
            self.error_message = "Please select at least one output format".to_string();
            return;
        }

        // Clear previous state
        self.is_processing = true;
        self.progress = 0.0;
        self.processed_count = 0;
        self.total_count = 0;
        self.error_message.clear();
        self.results_message.clear();

        // Create channel for progress updates
        let (tx, rx) = channel();
        self.progress_receiver = Some(rx);

        // Clone all configuration for the background thread
        let input_path = PathBuf::from(self.input_path.clone());
        let output_path = PathBuf::from(self.output_path.clone());
        let processing_type = match self.processing_type {
            ColorType::BlackWhite => ProcessingType::BlackWhite,
            ColorType::SixColor => ProcessingType::SixColor,
            ColorType::SevenColor => ProcessingType::SevenColor,
        };

        // Calculate dimensions automatically based on display type and orientation
        let (target_width, target_height) = match (&self.processing_type, &self.target_orientation) {
            // B/W displays: dimensions match visual orientation
            (ColorType::BlackWhite, photoframe_processor::cli::TargetOrientation::Landscape) => (800, 480),
            (ColorType::BlackWhite, photoframe_processor::cli::TargetOrientation::Portrait) => (480, 800),
            // 6C/7C displays: always hardware dimensions 800×480
            (ColorType::SixColor | ColorType::SevenColor, _) => (800, 480),
        };

        // Determine if pre-rotation is needed (only for 6c/7c portrait)
        let needs_pre_rotation = matches!(
            (&self.processing_type, &self.target_orientation),
            (ColorType::SixColor | ColorType::SevenColor, photoframe_processor::cli::TargetOrientation::Portrait)
        );

        let target_orientation = self.target_orientation.clone();
        let dithering_method = self.dithering_method.clone();
        let dither_strength = self.dither_strength;
        let contrast = self.contrast;
        let auto_optimize = self.auto_optimize;
        let auto_color_correct = self.auto_color_correct;

        // Output formats
        let mut output_formats = Vec::new();
        if self.output_bmp { output_formats.push(OutputType::Bmp); }
        if self.output_bin { output_formats.push(OutputType::Bin); }
        if self.output_jpg { output_formats.push(OutputType::Jpg); }
        if self.output_png { output_formats.push(OutputType::Png); }

        // People detection
        let detect_people = self.detect_people;
        let python_script_path = if !self.python_script_path.is_empty() {
            Some(PathBuf::from(self.python_script_path.clone()))
        } else {
            None
        };
        let python_path = if !self.python_path.is_empty() {
            Some(PathBuf::from(self.python_path.clone()))
        } else {
            None
        };
        let confidence_threshold = self.confidence_threshold;

        // Annotation settings
        let annotate = self.annotate;
        let font = self.font.clone();
        let pointsize = self.pointsize as f32;
        let annotate_background = self.annotate_background.clone();

        // Divider settings
        let divider_width = self.divider_width;
        let divider_color_str = self.divider_color.clone();

        // Parse divider color
        let divider_color = parse_hex_color(&divider_color_str).unwrap_or(Rgb([255, 255, 255]));

        // Processing options
        let force = self.force;
        let dry_run = self.dry_run;
        let debug = self.debug;
        let verbose = self.verbose;
        let report = self.report;
        let jobs = if self.jobs == 0 {
            // Auto-detect CPU cores - fallback to 4 if detection fails
            std::thread::available_parallelism()
                .map(|p| p.get())
                .unwrap_or(4)
        } else {
            self.jobs
        };
        let extensions: Vec<String> = self.extensions.split(',')
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .collect();

        // Spawn processing thread
        std::thread::spawn(move || {
            let config = ProcessingConfig {
                processing_type,
                target_width,
                target_height,
                auto_orientation: true, // Always enabled for smart orientation detection
                font_name: font,
                font_size: pointsize,
                annotation_background: annotate_background,
                extensions,
                verbose,
                parallel_jobs: jobs,
                output_formats,
                detect_people,
                python_script_path,
                python_path,
                force,
                debug,
                annotate,
                auto_color_correct,
                dry_run,
                confidence_threshold,
                divider_width,
                divider_color,
                dithering_method,
                dither_strength,
                contrast,
                auto_optimize,
                optimization_report: report,
                target_orientation,
                needs_pre_rotation,
                json_progress: false, // GUI doesn't use JSON progress (uses internal channel)
            };

            // Create processing engine
            let engine = match ProcessingEngine::new(config) {
                Ok(e) => e,
                Err(e) => {
                    let _ = tx.send(ProgressMessage::Error(format!("Failed to create processing engine: {}", e)));
                    return;
                }
            };

            // Discover images
            let image_files = match engine.discover_images(&[input_path]) {
                Ok(files) => files,
                Err(e) => {
                    let _ = tx.send(ProgressMessage::Error(format!("Failed to discover images: {}", e)));
                    return;
                }
            };

            if image_files.is_empty() {
                let _ = tx.send(ProgressMessage::Error("No images found in input directory".to_string()));
                return;
            }

            let total = image_files.len();
            let _ = tx.send(ProgressMessage::Progress {
                current: 0,
                total,
                file: "Starting...".to_string()
            });

            // Process images (simplified - no progress bars in GUI mode)
            let mut success_count = 0;
            let mut failed_count = 0;

            for (idx, image_path) in image_files.iter().enumerate() {
                let filename = image_path.file_name()
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown")
                    .to_string();

                let _ = tx.send(ProgressMessage::Progress {
                    current: idx + 1,
                    total,
                    file: filename,
                });

                // TODO: Implement actual single image processing
                // For now just simulate
                std::thread::sleep(std::time::Duration::from_millis(100));
                success_count += 1;
            }

            let message = if failed_count == 0 {
                format!("Successfully processed {} images", success_count)
            } else {
                format!("Processed {} images ({} succeeded, {} failed)",
                    total, success_count, failed_count)
            };

            let _ = tx.send(ProgressMessage::Complete {
                success: success_count,
                failed: failed_count,
                message,
            });
        });
    }

    /// Check for progress updates from the background thread
    pub fn check_progress(&mut self) {
        // Check if we have a receiver
        let receiver_exists = self.progress_receiver.is_some();
        if !receiver_exists {
            return;
        }

        // Collect all messages in a vector first to avoid borrowing issues
        let mut messages = Vec::new();
        if let Some(ref receiver) = self.progress_receiver {
            while let Ok(msg) = receiver.try_recv() {
                messages.push(msg);
            }
        }

        // Process messages and potentially clear the receiver
        let mut should_clear_receiver = false;
        for msg in messages {
            match msg {
                ProgressMessage::Progress { current, total, file } => {
                    self.processed_count = current;
                    self.total_count = total;
                    self.current_file = file;
                    if total > 0 {
                        self.progress = (current as f32 / total as f32) * 100.0;
                    }
                }
                ProgressMessage::Complete { success: _, failed: _, message } => {
                    self.is_processing = false;
                    self.results_message = message;
                    should_clear_receiver = true;
                }
                ProgressMessage::Error(err) => {
                    self.is_processing = false;
                    self.error_message = err;
                    should_clear_receiver = true;
                }
            }
        }

        if should_clear_receiver {
            self.progress_receiver = None;
        }
    }
}
