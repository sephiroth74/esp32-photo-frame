use eframe::egui;
use image::Rgb;
use photoframe_processor::cli::{ColorType, DitherMethod, TargetOrientation};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::sync::mpsc::Receiver;

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
    report: bool,
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

    // Cached system fonts list
    system_fonts: Vec<String>,
}

// Helper function to get system fonts
fn get_system_fonts() -> Vec<String> {
    use font_kit::source::SystemSource;

    let mut fonts = vec![
        // Add some common fallback fonts first
        "Arial".to_string(),
        "Helvetica".to_string(),
        "Times New Roman".to_string(),
        "Courier".to_string(),
        "Verdana".to_string(),
        "Georgia".to_string(),
    ];

    // Try to get system fonts
    if let Ok(system_fonts) = SystemSource::new().all_families() {
        for family in system_fonts {
            if !fonts.contains(&family) {
                fonts.push(family);
            }
        }
    }

    fonts.sort();
    fonts.dedup();
    fonts
}

#[derive(Debug)]
pub(crate) enum ProgressMessage {
    Progress {
        current: usize,
        total: usize,
        file: String,
    },
    Complete {
        success: usize,
        failed: usize,
        message: String,
    },
    Error(String),
}

// Configuration that gets persisted to disk
#[derive(Debug, Serialize, Deserialize)]
struct AppConfig {
    input_path: String,
    output_path: String,
    processing_type: String, // Serialize as string for compatibility
    target_orientation: String,
    auto_color_correct: bool,
    dithering_method: String,
    dither_strength: f32,
    contrast: f32,
    auto_optimize: bool,
    output_bmp: bool,
    output_bin: bool,
    output_jpg: bool,
    output_png: bool,
    detect_people: bool,
    python_script_path: String,
    python_path: String,
    confidence_threshold: f32,
    annotate: bool,
    font: String,
    pointsize: u32,
    annotate_background: String,
    divider_width: u32,
    divider_color: String,
    force: bool,
    dry_run: bool,
    debug: bool,
    report: bool,
    jobs: usize,
    extensions: String,
}

// Helper function to get config file path
fn get_config_path() -> PathBuf {
    if let Some(config_dir) = dirs::config_dir() {
        let app_config_dir = config_dir.join("photoframe-processor");
        std::fs::create_dir_all(&app_config_dir).ok();
        app_config_dir.join("gui_config.json")
    } else {
        PathBuf::from("gui_config.json")
    }
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
        // Try to load saved config, otherwise use defaults
        let mut app = Self::default_app();

        // Load system fonts
        app.system_fonts = get_system_fonts();

        if let Ok(config) = Self::load_config() {
            app.apply_config(config);
        }

        app
    }

    fn default_app() -> Self {
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
            report: false,
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
            system_fonts: Vec::new(),
        }
    }

    fn load_config() -> Result<AppConfig, Box<dyn std::error::Error>> {
        let config_path = get_config_path();
        let config_str = std::fs::read_to_string(config_path)?;
        let config: AppConfig = serde_json::from_str(&config_str)?;
        Ok(config)
    }

    fn save_config(&self) -> Result<(), Box<dyn std::error::Error>> {
        let config = AppConfig {
            input_path: self.input_path.clone(),
            output_path: self.output_path.clone(),
            processing_type: format!("{:?}", self.processing_type),
            target_orientation: format!("{:?}", self.target_orientation),
            auto_color_correct: self.auto_color_correct,
            dithering_method: format!("{:?}", self.dithering_method),
            dither_strength: self.dither_strength,
            contrast: self.contrast,
            auto_optimize: self.auto_optimize,
            output_bmp: self.output_bmp,
            output_bin: self.output_bin,
            output_jpg: self.output_jpg,
            output_png: self.output_png,
            detect_people: self.detect_people,
            python_script_path: self.python_script_path.clone(),
            python_path: self.python_path.clone(),
            confidence_threshold: self.confidence_threshold,
            annotate: self.annotate,
            font: self.font.clone(),
            pointsize: self.pointsize,
            annotate_background: self.annotate_background.clone(),
            divider_width: self.divider_width,
            divider_color: self.divider_color.clone(),
            force: self.force,
            dry_run: self.dry_run,
            debug: self.debug,
            report: self.report,
            jobs: self.jobs,
            extensions: self.extensions.clone(),
        };

        let config_path = get_config_path();
        let config_str = serde_json::to_string_pretty(&config)?;
        std::fs::write(config_path, config_str)?;
        Ok(())
    }

    fn apply_config(&mut self, config: AppConfig) {
        self.input_path = config.input_path;
        self.output_path = config.output_path;

        // Parse processing type
        self.processing_type = match config.processing_type.as_str() {
            "SixColor" => ColorType::SixColor,
            "SevenColor" => ColorType::SevenColor,
            "BlackWhite" => ColorType::BlackWhite,
            _ => ColorType::SixColor,
        };

        // Parse target orientation
        self.target_orientation = match config.target_orientation.as_str() {
            "Portrait" => TargetOrientation::Portrait,
            "Landscape" => TargetOrientation::Landscape,
            _ => TargetOrientation::Landscape,
        };

        self.auto_color_correct = config.auto_color_correct;

        // Parse dithering method
        self.dithering_method = match config.dithering_method.as_str() {
            "FloydSteinberg" => DitherMethod::FloydSteinberg,
            "Atkinson" => DitherMethod::Atkinson,
            "Stucki" => DitherMethod::Stucki,
            "JarvisJudiceNinke" => DitherMethod::JarvisJudiceNinke,
            "Ordered" => DitherMethod::Ordered,
            _ => DitherMethod::JarvisJudiceNinke,
        };

        self.dither_strength = config.dither_strength;
        self.contrast = config.contrast;
        self.auto_optimize = config.auto_optimize;
        self.output_bmp = config.output_bmp;
        self.output_bin = config.output_bin;
        self.output_jpg = config.output_jpg;
        self.output_png = config.output_png;
        self.detect_people = config.detect_people;
        self.python_script_path = config.python_script_path;
        self.python_path = config.python_path;
        self.confidence_threshold = config.confidence_threshold;
        self.annotate = config.annotate;
        self.font = config.font;
        self.pointsize = config.pointsize;
        self.annotate_background = config.annotate_background;
        self.divider_width = config.divider_width;
        self.divider_color = config.divider_color;
        self.force = config.force;
        self.dry_run = config.dry_run;
        self.debug = config.debug;
        self.report = config.report;
        self.jobs = config.jobs;
        self.extensions = config.extensions;
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

        // Processing type and orientation on same line
        ui.horizontal(|ui| {
            ui.label("Display Type:");
            egui::ComboBox::from_id_salt("processing_type")
                .selected_text(format!("{:?}", self.processing_type))
                .show_ui(ui, |ui| {
                    ui.selectable_value(
                        &mut self.processing_type,
                        ColorType::BlackWhite,
                        "Black & White",
                    );
                    ui.selectable_value(&mut self.processing_type, ColorType::SixColor, "6-Color");
                    ui.selectable_value(
                        &mut self.processing_type,
                        ColorType::SevenColor,
                        "7-Color",
                    );
                });

            ui.add_space(20.0);

            ui.label("Target Orientation:");
            egui::ComboBox::from_id_salt("target_orientation")
                .selected_text(self.get_target_orientation_name())
                .show_ui(ui, |ui| {
                    ui.selectable_value(
                        &mut self.target_orientation,
                        TargetOrientation::Landscape,
                        "Landscape (display mounted horizontally)",
                    );
                    ui.selectable_value(
                        &mut self.target_orientation,
                        TargetOrientation::Portrait,
                        "Portrait (display mounted vertically - images pre-rotated 90° CW)",
                    );
                });
        });
        ui.label("ℹ Images will be pre-rotated to match the physical display orientation");

        ui.add_space(10.0);
    }

    fn render_processing_options(&mut self, ui: &mut egui::Ui) {
        ui.heading("Processing Options");
        ui.separator();

        ui.checkbox(
            &mut self.auto_color_correct,
            "Auto color correction (6c/7c only)",
        );
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

        ui.checkbox(
            &mut self.auto_optimize,
            "Auto-optimize (automatic per-image optimization)",
        );

        if !self.auto_optimize {
            // Dithering method
            ui.horizontal(|ui| {
                ui.label("Method:");
                egui::ComboBox::from_id_salt("dithering_method")
                    .selected_text(self.get_dithering_method_name())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.dithering_method,
                            DitherMethod::FloydSteinberg,
                            "Floyd-Steinberg",
                        );
                        ui.selectable_value(
                            &mut self.dithering_method,
                            DitherMethod::Atkinson,
                            "Atkinson",
                        );
                        ui.selectable_value(
                            &mut self.dithering_method,
                            DitherMethod::Stucki,
                            "Stucki",
                        );
                        ui.selectable_value(
                            &mut self.dithering_method,
                            DitherMethod::JarvisJudiceNinke,
                            "Jarvis-Judice-Ninke",
                        );
                        ui.selectable_value(
                            &mut self.dithering_method,
                            DitherMethod::Ordered,
                            "Ordered/Bayer",
                        );
                    });
            });

            // Strength and Contrast on same line
            ui.horizontal(|ui| {
                // Dither strength - wider slider
                let strength_val = self.dither_strength;
                ui.add_sized(
                    egui::vec2(250.0, 20.0),
                    egui::Slider::new(&mut self.dither_strength, 0.5..=1.5)
                        .step_by(0.01)
                        .text(format!("Strength: {:.2}", strength_val)),
                );

                ui.add_space(10.0);

                // Contrast - wider slider
                let contrast_val = self.contrast;
                ui.add_sized(
                    egui::vec2(250.0, 20.0),
                    egui::Slider::new(&mut self.contrast, -0.5..=0.5)
                        .step_by(0.01)
                        .text(format!("Contrast: {:.2}", contrast_val)),
                );
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
            // Font and size on same line
            ui.horizontal(|ui| {
                ui.label("Font:");
                egui::ComboBox::from_id_salt("font_selector")
                    .selected_text(&self.font)
                    .width(200.0)
                    .show_ui(ui, |ui| {
                        // Show all system fonts
                        for font in &self.system_fonts {
                            ui.selectable_value(&mut self.font, font.clone(), font);
                        }
                    });

                ui.add_space(10.0);
                ui.label("Size:");
                ui.add(
                    egui::DragValue::new(&mut self.pointsize)
                        .speed(1)
                        .range(8..=72),
                );
            });

            ui.horizontal(|ui| {
                ui.label("Background:");
                ui.text_edit_singleline(&mut self.annotate_background);
            });
            ui.label("ℹ Hex with alpha (e.g., #00000040)");
        }

        ui.add_space(10.0);
    }

    fn render_divider_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Divider Settings");
        ui.separator();

        // Width and color on same line
        ui.horizontal(|ui| {
            ui.label("Width (px):");
            ui.add(
                egui::DragValue::new(&mut self.divider_width)
                    .speed(1)
                    .range(0..=10),
            );

            ui.add_space(20.0);

            ui.label("Color:");
            ui.text_edit_singleline(&mut self.divider_color);
        });
        ui.label("ℹ Divider for combined portrait images (Hex RGB, e.g., #FFFFFF)");

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
            ui.checkbox(&mut self.debug, "Debug mode");
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

        ui.horizontal(|ui| {
            let button_text = if self.is_processing {
                "Processing..."
            } else {
                "Process Images"
            };

            let button = egui::Button::new(button_text).min_size(egui::vec2(200.0, 40.0));

            if ui.add_enabled(!self.is_processing, button).clicked() {
                self.start_processing();
            }

            ui.add_space(10.0);

            // Show progress bar next to button when processing
            if self.is_processing {
                let progress_bar = egui::ProgressBar::new(self.progress)
                    .show_percentage()
                    .animate(true);
                ui.add_sized(egui::vec2(300.0, 40.0), progress_bar);
            }
        });

        ui.add_space(10.0);
    }

    fn render_progress(&mut self, ui: &mut egui::Ui) {
        if self.is_processing || !self.results_message.is_empty() || !self.error_message.is_empty()
        {
            ui.heading("Progress");
            ui.separator();

            if self.is_processing {
                ui.label(format!(
                    "Processing: {}/{}",
                    self.processed_count, self.total_count
                ));
                ui.label(&self.current_file);
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
