use eframe::egui;
use photoframe_processor::cli::{ColorType, DitherMethod, TargetOrientation};
use std::sync::mpsc::Receiver;
use image::Rgb;

#[path = "app_processing.rs"]
mod app_processing;

pub struct PhotoFrameApp {
    // Input/Output paths
    input_path: String,
    output_path: String,

    // Processing configuration
    processing_type: ColorType,
    target_orientation: TargetOrientation,
    auto_color_correct: bool,

    // Dithering settings
    dithering_method: DitherMethod,
    dither_strength: f32,
    contrast: f32,
    auto_optimize: bool,

    // Output formats
    output_bmp: bool,
    output_bin: bool,
    output_jpg: bool,
    output_png: bool,

    // People detection
    detect_people: bool,
    python_script_path: String,
    python_path: String,
    confidence_threshold: f32,

    // Annotation settings
    annotate: bool,
    font: String,
    pointsize: u32,
    annotate_background: String,

    // Divider settings
    divider_width: u32,
    divider_color: String,

    // Processing options
    force: bool,
    dry_run: bool,
    debug: bool,
    validate_only: bool,
    report: bool,
    verbose: bool,
    jobs: usize,
    extensions: String,

    // Processing state
    is_processing: bool,
    progress: f32,
    current_file: String,
    processed_count: usize,
    total_count: usize,

    // Results
    results_message: String,
    error_message: String,

    // Communication channel for background processing
    progress_receiver: Option<Receiver<ProgressMessage>>,
}

#[derive(Debug)]
pub(crate) enum ProgressMessage {
    Progress { current: usize, total: usize, file: String },
    Complete { success: usize, failed: usize, message: String },
    Error(String),
}

// Helper function to parse hex color strings
pub(crate) fn parse_hex_color(hex: &str) -> Option<Rgb<u8>> {
    let hex = hex.trim_start_matches('#');
    if hex.len() == 6 {
        let r = u8::from_str_radix(&hex[0..2], 16).ok()?;
        let g = u8::from_str_radix(&hex[2..4], 16).ok()?;
        let b = u8::from_str_radix(&hex[4..6], 16).ok()?;
        Some(Rgb([r, g, b]))
    } else {
        None
    }
}

impl PhotoFrameApp {
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        Self {
            // Default values
            input_path: String::new(),
            output_path: String::new(),
            processing_type: ColorType::SixColor,
            target_orientation: TargetOrientation::Landscape,
            auto_color_correct: true,
            dithering_method: DitherMethod::JarvisJudiceNinke,
            dither_strength: 1.0,
            contrast: 0.0,
            auto_optimize: false,
            output_bmp: false,
            output_bin: true,
            output_jpg: true,
            output_png: false,
            detect_people: false,
            python_script_path: String::new(),
            python_path: String::new(),
            confidence_threshold: 0.6,
            annotate: false,
            font: "Arial".to_string(),
            pointsize: 22,
            annotate_background: "#00000040".to_string(),
            divider_width: 3,
            divider_color: "#FFFFFF".to_string(),
            force: false,
            dry_run: false,
            debug: false,
            validate_only: false,
            report: false,
            verbose: false,
            jobs: 0,
            extensions: "jpg,jpeg,png,heic,webp,tiff".to_string(),
            is_processing: false,
            progress: 0.0,
            current_file: String::new(),
            processed_count: 0,
            total_count: 0,
            results_message: String::new(),
            error_message: String::new(),
            progress_receiver: None,
        }
    }

    fn render_file_selection(&mut self, ui: &mut egui::Ui) {
        ui.heading("File Selection");
        ui.separator();

        // Input path
        ui.horizontal(|ui| {
            ui.label("Input:");
            ui.text_edit_singleline(&mut self.input_path);
            if ui.button("Browse...").clicked() {
                if let Some(path) = rfd::FileDialog::new().pick_folder() {
                    self.input_path = path.display().to_string();
                }
            }
        });

        // Output path
        ui.horizontal(|ui| {
            ui.label("Output:");
            ui.text_edit_singleline(&mut self.output_path);
            if ui.button("Browse...").clicked() {
                if let Some(path) = rfd::FileDialog::new().pick_folder() {
                    self.output_path = path.display().to_string();
                }
            }
        });

        ui.add_space(10.0);
    }

    fn render_display_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Display Settings");
        ui.separator();

        // Processing type
        ui.horizontal(|ui| {
            ui.label("Display Type:");
            egui::ComboBox::from_id_salt("processing_type")
                .selected_text(format!("{:?}", self.processing_type))
                .show_ui(ui, |ui| {
                    ui.selectable_value(&mut self.processing_type, ColorType::BlackWhite, "Black & White");
                    ui.selectable_value(&mut self.processing_type, ColorType::SixColor, "6-Color");
                    ui.selectable_value(&mut self.processing_type, ColorType::SevenColor, "7-Color");
                });
        });

        // Display orientation (size determined automatically)
        ui.horizontal(|ui| {
            ui.label("Target Orientation:");
            egui::ComboBox::from_id_salt("target_orientation")
                .selected_text(self.get_target_orientation_name())
                .show_ui(ui, |ui| {
                    ui.selectable_value(
                        &mut self.target_orientation,
                        TargetOrientation::Landscape,
                        "Landscape (display mounted horizontally)"
                    );
                    ui.selectable_value(
                        &mut self.target_orientation,
                        TargetOrientation::Portrait,
                        "Portrait (display mounted vertically - images pre-rotated 90° CW)"
                    );
                });
        });
        ui.label("ℹ Images will be pre-rotated to match the physical display orientation");

        ui.add_space(10.0);
    }

    fn render_processing_options(&mut self, ui: &mut egui::Ui) {
        ui.heading("Processing Options");
        ui.separator();

        ui.checkbox(&mut self.auto_color_correct, "Auto color correction (6c/7c only)");
        ui.checkbox(&mut self.detect_people, "Detect people (AI)");

        if self.detect_people {
            ui.horizontal(|ui| {
                ui.label("Python script:");
                ui.text_edit_singleline(&mut self.python_script_path);
                if ui.button("Browse...").clicked() {
                    if let Some(path) = rfd::FileDialog::new()
                        .add_filter("Python", &["py"])
                        .pick_file()
                    {
                        self.python_script_path = path.display().to_string();
                    }
                }
            });

            ui.horizontal(|ui| {
                ui.label("Python interpreter:");
                ui.text_edit_singleline(&mut self.python_path);
                if ui.button("Browse...").clicked() {
                    if let Some(path) = rfd::FileDialog::new().pick_file() {
                        self.python_path = path.display().to_string();
                    }
                }
            });
            ui.label("(Leave empty to use system Python)");

            ui.horizontal(|ui| {
                ui.label("Confidence:");
                ui.add(egui::Slider::new(&mut self.confidence_threshold, 0.0..=1.0).step_by(0.05));
            });
        }

        ui.add_space(10.0);
    }

    fn render_dithering_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Dithering Settings");
        ui.separator();

        ui.checkbox(&mut self.auto_optimize, "Auto-optimize (automatic per-image optimization)");

        if !self.auto_optimize {
            // Dithering method
            ui.horizontal(|ui| {
                ui.label("Method:");
                egui::ComboBox::from_id_salt("dithering_method")
                    .selected_text(self.get_dithering_method_name())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(&mut self.dithering_method, DitherMethod::FloydSteinberg, "Floyd-Steinberg");
                        ui.selectable_value(&mut self.dithering_method, DitherMethod::Atkinson, "Atkinson");
                        ui.selectable_value(&mut self.dithering_method, DitherMethod::Stucki, "Stucki");
                        ui.selectable_value(&mut self.dithering_method, DitherMethod::JarvisJudiceNinke, "Jarvis-Judice-Ninke");
                        ui.selectable_value(&mut self.dithering_method, DitherMethod::Ordered, "Ordered/Bayer");
                    });
            });

            // Dither strength
            ui.horizontal(|ui| {
                ui.label("Strength:");
                ui.add(egui::Slider::new(&mut self.dither_strength, 0.5..=1.5).step_by(0.1));
            });

            // Contrast
            ui.horizontal(|ui| {
                ui.label("Contrast:");
                ui.add(egui::Slider::new(&mut self.contrast, -0.5..=0.5).step_by(0.05));
            });
        } else {
            ui.label("(Auto-optimize will select optimal parameters for each image)");
        }

        ui.add_space(10.0);
    }

    fn render_annotation_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Annotation Settings");
        ui.separator();

        ui.checkbox(&mut self.annotate, "Add filename annotations");

        if self.annotate {
            ui.horizontal(|ui| {
                ui.label("Font:");
                ui.text_edit_singleline(&mut self.font);
            });

            ui.horizontal(|ui| {
                ui.label("Font size:");
                ui.add(egui::DragValue::new(&mut self.pointsize).speed(1).range(8..=72));
            });

            ui.horizontal(|ui| {
                ui.label("Background color:");
                ui.text_edit_singleline(&mut self.annotate_background);
            });
            ui.label("(Hex with alpha, e.g., #00000040)");
        }

        ui.add_space(10.0);
    }

    fn render_divider_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Divider Settings");
        ui.separator();

        ui.horizontal(|ui| {
            ui.label("Divider width (pixels):");
            ui.add(egui::DragValue::new(&mut self.divider_width).speed(1).range(0..=10));
        });

        ui.horizontal(|ui| {
            ui.label("Divider color:");
            ui.text_edit_singleline(&mut self.divider_color);
        });
        ui.label("(Hex RGB, e.g., #FFFFFF for white)");

        ui.add_space(10.0);
    }

    fn render_output_formats(&mut self, ui: &mut egui::Ui) {
        ui.heading("Output Formats");
        ui.separator();

        ui.horizontal(|ui| {
            ui.checkbox(&mut self.output_bmp, "BMP");
            ui.checkbox(&mut self.output_bin, "Binary (BIN)");
            ui.checkbox(&mut self.output_jpg, "JPEG");
            ui.checkbox(&mut self.output_png, "PNG");
        });

        ui.add_space(10.0);
    }

    fn render_advanced_options(&mut self, ui: &mut egui::Ui) {
        ui.heading("Advanced Options");
        ui.separator();

        ui.horizontal(|ui| {
            ui.checkbox(&mut self.force, "Force (overwrite existing files)");
            ui.checkbox(&mut self.report, "Generate report");
        });

        ui.horizontal(|ui| {
            ui.checkbox(&mut self.dry_run, "Dry run (simulate only)");
            ui.checkbox(&mut self.verbose, "Verbose output");
        });

        ui.horizontal(|ui| {
            ui.checkbox(&mut self.debug, "Debug mode");
            ui.checkbox(&mut self.validate_only, "Validate only");
        });

        ui.horizontal(|ui| {
            ui.label("Parallel jobs:");
            ui.add(egui::DragValue::new(&mut self.jobs).speed(1).range(0..=32));
        });
        ui.label("(0 = auto-detect CPU cores)");

        ui.horizontal(|ui| {
            ui.label("File extensions:");
            ui.text_edit_singleline(&mut self.extensions);
        });
        ui.label("(Comma-separated, e.g., jpg,jpeg,png,heic)");

        ui.add_space(10.0);
    }

    fn render_process_button(&mut self, ui: &mut egui::Ui) {
        ui.separator();

        let button_text = if self.is_processing {
            "Processing..."
        } else {
            "Process Images"
        };

        let button = egui::Button::new(button_text)
            .min_size(egui::vec2(200.0, 40.0));

        if ui.add_enabled(!self.is_processing, button).clicked() {
            self.start_processing();
        }

        ui.add_space(10.0);
    }

    fn render_progress(&mut self, ui: &mut egui::Ui) {
        if self.is_processing || !self.results_message.is_empty() {
            ui.heading("Progress");
            ui.separator();

            if self.is_processing {
                ui.label(format!("Processing: {}/{}", self.processed_count, self.total_count));
                ui.label(&self.current_file);

                let progress_bar = egui::ProgressBar::new(self.progress)
                    .show_percentage()
                    .animate(true);
                ui.add(progress_bar);
            }

            if !self.results_message.is_empty() {
                ui.label(&self.results_message);
            }

            if !self.error_message.is_empty() {
                ui.colored_label(egui::Color32::RED, &self.error_message);
            }
        }
    }

    fn get_dithering_method_name(&self) -> &str {
        match self.dithering_method {
            DitherMethod::FloydSteinberg => "Floyd-Steinberg",
            DitherMethod::Atkinson => "Atkinson",
            DitherMethod::Stucki => "Stucki",
            DitherMethod::JarvisJudiceNinke => "Jarvis-Judice-Ninke",
            DitherMethod::Ordered => "Ordered/Bayer",
        }
    }

    fn get_target_orientation_name(&self) -> &str {
        match self.target_orientation {
            TargetOrientation::Landscape => "Landscape",
            TargetOrientation::Portrait => "Portrait",
        }
    }

}

impl eframe::App for PhotoFrameApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Check for progress updates from background thread
        self.check_progress();

        egui::CentralPanel::default().show(ctx, |ui| {
            egui::ScrollArea::vertical().show(ui, |ui| {
                ui.heading("ESP32 Photo Frame Processor");
                ui.label("Process images for e-paper displays");
                ui.add_space(20.0);

                self.render_file_selection(ui);
                self.render_display_settings(ui);
                self.render_processing_options(ui);
                self.render_dithering_settings(ui);
                self.render_annotation_settings(ui);
                self.render_divider_settings(ui);
                self.render_output_formats(ui);
                self.render_advanced_options(ui);
                self.render_process_button(ui);
                self.render_progress(ui);
            });
        });

        // Request repaint if processing
        if self.is_processing {
            ctx.request_repaint();
        }
    }
}
