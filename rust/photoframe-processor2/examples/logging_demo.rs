/// Example demonstrating the logging system with different verbosity levels
use photoframe_processor2::logging::Logger;

fn main() {
    println!("=== Logging System Example ===\n");

    // Example 1: Normal mode (no verbose, no debug)
    println!("1. Normal Mode (no flags):");
    let logger = Logger::new(false, false);

    logger.section("Processing Images");
    logger.config_section("Settings");
    logger.config_item("Input", "/path/to/input");
    logger.config_item("Output", "/path/to/output");
    logger.info("");

    logger.info("🔄 Starting image processing...");
    logger.success("Found 10 images");
    logger.warning("Some images might be corrupted");
    logger.info("");

    // Verbose messages are NOT shown
    logger.verbose("This verbose message will NOT appear");
    logger.file_op("Reading", "/path/to/file");
    logger.analysis("Analyzing image");

    println!();

    // Example 2: Verbose mode
    println!("2. Verbose Mode (--verbose flag):");
    let logger = Logger::new(true, false);

    logger.section("Processing Images");
    logger.config_section("Settings");
    logger.config_item("Input", "/path/to/input");
    logger.config_item("Output", "/path/to/output");
    logger.info("");

    logger.info("🔄 Starting image processing...");
    logger.success("Found 10 images");
    logger.warning("Some images might be corrupted");
    logger.info("");

    // Verbose messages ARE shown
    logger.verbose("This verbose message WILL appear");
    logger.file_op("Reading", "/path/to/file.jpg");
    logger.analysis("Analyzing image dimensions");
    logger.processing("Converting to BMP format");

    println!();

    // Example 3: Debug mode
    println!("3. Debug Mode (--debug flag):");
    let logger = Logger::new(false, true);

    logger.section("Debug Information");
    logger.debug("Logger initialized with debug=true");
    logger.debug("Processing image: image.jpg");
    logger.debug("Width: 1024, Height: 768, Format: JPEG");

    println!();

    // Example 4: All modes enabled
    println!("4. All Modes (--verbose --debug):");
    let logger = Logger::new(true, true);

    logger.section("Complete Processing");
    logger.debug("Initializing processor");

    logger.config_section("Configuration");
    logger.config_item("Verbose", "true");
    logger.config_item("Debug", "true");
    logger.info("");

    logger.info("🔄 Processing images...");
    logger.verbose("Loading image metadata");
    logger.debug("Image metadata loaded: 1024×768@8bit");

    logger.success("Processing complete");
    logger.debug("Total processing time: 125ms");

    println!();

    // Example 5: Error and warning handling
    println!("5. Error Handling:");
    let logger = Logger::new(true, false);

    logger.info("Attempting to process file...");
    logger.file_op("Reading", "/path/to/missing_file.jpg");
    logger.verbose("File not found, attempting fallback");
    logger.warning("Could not read file, using default");
    logger.error("Critical processing error occurred");

    println!();

    // Example 6: Progress indicators
    println!("6. Progress Indicators:");
    let logger = Logger::new(false, false);

    logger.section("Batch Processing");

    logger.progress_start("Processing image 1");
    logger.progress_done();

    logger.progress_start("Processing image 2");
    logger.progress_done();

    logger.progress_start("Processing image 3");
    logger.progress_done();

    logger.success("All images processed");
}
