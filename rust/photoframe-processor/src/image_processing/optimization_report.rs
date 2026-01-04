use crate::cli::DitherMethod;
use prettytable::{format, Cell, Row, Table};
/// Optimization report generation for auto-optimize feature
///
/// This module generates formatted tables showing optimization decisions and results
/// for each processed image, with special formatting for portrait pairs.
use std::path::Path;

/// Single image optimization entry for the report
#[derive(Debug, Clone)]
pub struct OptimizationEntry {
    pub input_filename: String,
    pub output_filename: String,
    pub dither_method: DitherMethod,
    pub dither_strength: f32,
    pub contrast: f32,
    pub auto_color: bool,
    pub people_detected: bool,
    pub people_count: usize,
    pub is_pastel: bool,
    pub was_rotated: bool,   // Se l'immagine è stata ruotata per EXIF
    pub color_skipped: bool, // Se auto-color è stato skippato (perché pastel o altro)
}

/// Portrait pair optimization entry (two images combined side-by-side)
#[derive(Debug, Clone)]
pub struct PortraitPairEntry {
    pub left: OptimizationEntry,
    pub right: OptimizationEntry,
    pub combined_output: String,
}

/// Landscape pair optimization entry (two images combined top-bottom)
#[derive(Debug, Clone)]
pub struct LandscapePairEntry {
    pub top: OptimizationEntry,
    pub bottom: OptimizationEntry,
    pub combined_output: String,
}

/// Complete optimization report
#[derive(Debug)]
pub struct OptimizationReport {
    pub landscape_entries: Vec<OptimizationEntry>,
    pub portrait_entries: Vec<OptimizationEntry>,
    pub portrait_pairs: Vec<PortraitPairEntry>,
    pub landscape_pairs: Vec<LandscapePairEntry>,
}

impl OptimizationReport {
    pub fn new() -> Self {
        Self {
            landscape_entries: Vec::new(),
            portrait_entries: Vec::new(),
            portrait_pairs: Vec::new(),
            landscape_pairs: Vec::new(),
        }
    }

    /// Add a landscape image entry
    pub fn add_landscape(&mut self, entry: OptimizationEntry) {
        self.landscape_entries.push(entry);
    }

    /// Add a single portrait entry
    pub fn add_portrait(&mut self, entry: OptimizationEntry) {
        self.portrait_entries.push(entry);
    }

    /// Add a portrait pair entry
    pub fn add_portrait_pair(&mut self, pair: PortraitPairEntry) {
        self.portrait_pairs.push(pair);
    }

    /// Add a landscape pair entry
    pub fn add_landscape_pair(&mut self, pair: LandscapePairEntry) {
        self.landscape_pairs.push(pair);
    }

    /// Print the complete report as a formatted table
    pub fn print(&self) {
        println!("\n╔══════════════════════════════════════════════════════════════════════════════════════════════════════════╗");
        println!("║                                                    REPORT                                                ║");
        println!("╚══════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n");

        // Print landscape images
        if !self.landscape_entries.is_empty() {
            println!(
                "📷 LANDSCAPE IMAGES ({} total)\n",
                self.landscape_entries.len()
            );
            let mut table = Table::new();
            table.set_format(*format::consts::FORMAT_BOX_CHARS);

            // Add header
            table.add_row(Row::new(vec![
                Cell::new("Input"),
                Cell::new("Output"),
                Cell::new("Dithering"),
                Cell::new("Strength"),
                Cell::new("Contrast"),
                Cell::new("Color"),
                Cell::new("Rot."),
                Cell::new("Skip"),
                Cell::new("People"),
            ]));

            // Add entries
            for entry in &self.landscape_entries {
                self.add_entry_row(&mut table, entry);
            }

            table.printstd();
            println!();
        }

        // Print portrait pairs
        if !self.portrait_pairs.is_empty() {
            println!(
                "🖼️  COMBINED PORTRAITS ({} pairs)\n",
                self.portrait_pairs.len()
            );
            let mut table = Table::new();
            table.set_format(*format::consts::FORMAT_BOX_CHARS);

            // Add header
            table.add_row(Row::new(vec![
                Cell::new("Input (L/R)"),
                Cell::new("Output"),
                Cell::new("Dithering"),
                Cell::new("Strength"),
                Cell::new("Contrast"),
                Cell::new("Color"),
                Cell::new("Rot."),
                Cell::new("Skip"),
                Cell::new("People"),
            ]));

            // Add portrait pairs
            for pair in &self.portrait_pairs {
                // Left image
                self.add_entry_row(&mut table, &pair.left);
                // Right image
                self.add_entry_row(&mut table, &pair.right);
                // Combined output row
                table.add_row(Row::new(vec![
                    Cell::new("└─> Combined:"),
                    Cell::new(&truncate(&pair.combined_output, 20)),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                ]));
            }

            table.printstd();
            println!();
        }

        // Print landscape pairs
        if !self.landscape_pairs.is_empty() {
            println!(
                "🌄 COMBINED LANDSCAPES ({} pairs)\n",
                self.landscape_pairs.len()
            );
            let mut table = Table::new();
            table.set_format(*format::consts::FORMAT_BOX_CHARS);

            // Add header
            table.add_row(Row::new(vec![
                Cell::new("Input (T/B)"),
                Cell::new("Output"),
                Cell::new("Dithering"),
                Cell::new("Strength"),
                Cell::new("Contrast"),
                Cell::new("Color"),
                Cell::new("Rot."),
                Cell::new("Skip"),
                Cell::new("People"),
            ]));

            // Add landscape pairs
            for pair in &self.landscape_pairs {
                // Top image
                self.add_entry_row(&mut table, &pair.top);
                // Bottom image
                self.add_entry_row(&mut table, &pair.bottom);
                // Combined output row
                table.add_row(Row::new(vec![
                    Cell::new("└─> Combined:"),
                    Cell::new(&truncate(&pair.combined_output, 20)),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                    Cell::new(""),
                ]));
            }

            table.printstd();
            println!();
        }

        // Print single portraits (if any)
        if !self.portrait_entries.is_empty() {
            println!(
                "🎭 INDIVIDUAL PORTRAITS ({} total)\n",
                self.portrait_entries.len()
            );
            let mut table = Table::new();
            table.set_format(*format::consts::FORMAT_BOX_CHARS);

            // Add header
            table.add_row(Row::new(vec![
                Cell::new("Input"),
                Cell::new("Output"),
                Cell::new("Dithering"),
                Cell::new("Strength"),
                Cell::new("Contrast"),
                Cell::new("Color"),
                Cell::new("Rot."),
                Cell::new("Skip"),
                Cell::new("People"),
            ]));

            // Add entries
            for entry in &self.portrait_entries {
                self.add_entry_row(&mut table, entry);
            }

            table.printstd();
            println!();
        }

        // Print summary
        let total_images = self.landscape_entries.len()
            + self.portrait_entries.len()
            + (self.portrait_pairs.len() * 2)
            + (self.landscape_pairs.len() * 2);
        let total_with_people = self
            .landscape_entries
            .iter()
            .filter(|e| e.people_detected)
            .count()
            + self
                .portrait_entries
                .iter()
                .filter(|e| e.people_detected)
                .count()
            + self
                .portrait_pairs
                .iter()
                .filter(|p| p.left.people_detected || p.right.people_detected)
                .count()
                * 2
            + self
                .landscape_pairs
                .iter()
                .filter(|p| p.top.people_detected || p.bottom.people_detected)
                .count()
                * 2;
        let total_pastel = self
            .landscape_entries
            .iter()
            .filter(|e| e.is_pastel)
            .count()
            + self.portrait_entries.iter().filter(|e| e.is_pastel).count()
            + self
                .portrait_pairs
                .iter()
                .filter(|p| p.left.is_pastel || p.right.is_pastel)
                .count()
            + self
                .landscape_pairs
                .iter()
                .filter(|p| p.top.is_pastel || p.bottom.is_pastel)
                .count();

        println!("📊 Summary:");
        println!("   • Total images processed: {}", total_images);
        println!(
            "   • Images with people detected: {} ({:.1}%)",
            total_with_people,
            (total_with_people as f32 / total_images as f32) * 100.0
        );
        println!(
            "   • Images with pastel tones: {} ({:.1}%)",
            total_pastel,
            (total_pastel as f32 / total_images as f32) * 100.0
        );
        println!("   • Landscape images: {}", self.landscape_entries.len());
        println!("   • Portrait pairs (side-by-side): {}", self.portrait_pairs.len());
        println!("   • Landscape pairs (top-bottom): {}", self.landscape_pairs.len());
        println!("   • Individual portraits: {}", self.portrait_entries.len());
        println!();
    }

    /// Add a single entry row to the table
    fn add_entry_row(&self, table: &mut Table, entry: &OptimizationEntry) {
        let dither_name = format_dither_method(&entry.dither_method);
        let color_check = if entry.auto_color { "✓" } else { "✗" };
        let rotation_check = if entry.was_rotated { "✓" } else { "✗" };
        let skip_check = if entry.color_skipped { "✓" } else { "✗" };
        let people_info = if entry.people_detected {
            format!("✓ ({})", entry.people_count)
        } else {
            "✗".to_string()
        };
        let pastel_marker = if entry.is_pastel { "🌸 " } else { "" };
        let input_with_marker = format!("{}{}", pastel_marker, truncate(&entry.input_filename, 25));

        table.add_row(Row::new(vec![
            Cell::new(&input_with_marker),
            Cell::new(&truncate(&entry.output_filename, 25)),
            Cell::new(&dither_name),
            Cell::new(&format!("{:.1}", entry.dither_strength)),
            Cell::new(&format!("{:+.2}", entry.contrast)),
            Cell::new(color_check),
            Cell::new(rotation_check),
            Cell::new(skip_check),
            Cell::new(&people_info),
        ]));
    }
}

/// Format dithering method name
fn format_dither_method(method: &DitherMethod) -> String {
    match method {
        DitherMethod::FloydSteinberg => "Floyd-S".to_string(),
        DitherMethod::Atkinson => "Atkinson".to_string(),
        DitherMethod::Stucki => "Stucki".to_string(),
        DitherMethod::JarvisJudiceNinke => "JJN".to_string(),
        DitherMethod::Ordered => "Ordered".to_string(),
    }
}

/// Truncate string to fit in column
fn truncate(s: &str, max_len: usize) -> String {
    if s.len() <= max_len {
        s.to_string()
    } else {
        format!("{}…", &s[..max_len - 1])
    }
}

/// Helper to extract filename from path
pub fn extract_filename(path: &Path) -> String {
    path.file_name()
        .and_then(|f| f.to_str())
        .unwrap_or("unknown")
        .to_string()
}
