use anyhow::Result;
use photoframe_processor2::fs_utils;
use photoframe_processor2::types::OutputType;
use std::path::Path;

fn main() -> Result<()> {
    println!("═══════════════════════════════════════════════════════════════");
    println!("🗂️  FILE SYSTEM UTILITIES DEMO");
    println!("═══════════════════════════════════════════════════════════════");
    println!();

    // Example 1: Create format directories
    let output_dir = Path::new("/tmp/photoframe_output");
    let formats = vec![OutputType::Pfr1, OutputType::Bmp, OutputType::Jpg];

    println!(
        "📁 Creating format directories in: {}",
        output_dir.display()
    );
    fs_utils::create_format_directories(output_dir, &formats)?;

    for format in &formats {
        let dir = fs_utils::get_format_directory(output_dir, format);
        println!("   ✓ Created: {}", dir.display());
    }
    println!();

    // Example 2: Generate output paths
    println!("📄 Generating output paths:");
    let input_path = Path::new("/input/photos/IMG_1234.jpg");

    for format in &formats {
        let output_path = fs_utils::get_format_output_path(output_dir, input_path, format, None);
        println!(
            "   {} → {}",
            fs_utils::get_format_extension(format),
            output_path.display()
        );
    }
    println!();

    // Example 3: Generate paths with suffix
    println!("📄 Generating paths with suffix:");
    let output_with_suffix = fs_utils::get_format_output_path(
        output_dir,
        input_path,
        &OutputType::Pfr1,
        Some("portrait"),
    );
    println!(
        "   With 'portrait' suffix: {}",
        output_with_suffix.display()
    );
    println!();

    // Example 4: Combined image paths
    println!("🖼️  Generating combined image paths:");
    let left_path = Path::new("/input/IMG_1234.jpg");
    let right_path = Path::new("/input/IMG_5678.jpg");

    let combined_path = fs_utils::get_combined_format_output_path(
        output_dir,
        left_path,
        right_path,
        &OutputType::Pfr1,
    );
    println!("   Combined: {}", combined_path.display());
    println!();

    // Example 5: Check paths
    println!("🔍 Checking paths:");
    println!(
        "   Output dir exists: {}",
        fs_utils::is_directory(output_dir)
    );
    println!("   Is directory: {}", fs_utils::is_directory(output_dir));
    println!();

    println!("═══════════════════════════════════════════════════════════════");
    println!("✨ All operations completed successfully!");
    println!();

    Ok(())
}
