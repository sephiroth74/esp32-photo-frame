// GUI entry point for photoframe-processor
// This binary provides a graphical interface for the image processor

use eframe::egui;

mod app;
use app::PhotoFrameApp;

fn main() -> Result<(), eframe::Error> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([800.0, 600.0])
            .with_min_inner_size([600.0, 400.0]),
        ..Default::default()
    };

    eframe::run_native(
        "ESP32 Photo Frame Processor",
        options,
        Box::new(|cc| Ok(Box::new(PhotoFrameApp::new(cc)))),
    )
}
