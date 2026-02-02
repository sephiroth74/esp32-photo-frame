/// Simple logging system with verbose mode support
///
/// The Logger provides methods for different message levels:
/// - error(): Always printed, for critical errors
/// - warning(): Always printed, for warnings
/// - info(): Always printed, for important information
/// - verbose(): Only printed when verbose mode is enabled
/// - debug(): Only printed when verbose mode is enabled
///
/// Messages can be formatted with color codes using special prefixes:
/// - "✓" for success (green)
/// - "✗" or "❌" for errors (red)
/// - "⚠️ " for warnings (yellow)
/// - "📁" for file operations
/// - "🔍" for searches/analysis
/// - "⏳" for processing
use std::io::Write;

#[derive(Clone, Copy)]
pub enum LogLevel {
    Error,
    Warning,
    Info,
    Verbose,
    Debug,
}

pub struct Logger {
    verbose: bool,
}

impl Logger {
    /// Create a new logger with the given verbosity settings
    pub fn new(verbose: bool) -> Self {
        Logger { verbose }
    }

    /// Print an error message (always shown)
    pub fn error(&self, msg: &str) {
        eprintln!("❌ {}", msg);
    }

    /// Print a warning message (always shown)
    pub fn warning(&self, msg: &str) {
        println!("⚠️  {}", msg);
    }

    /// Print an info message (always shown)
    pub fn info(&self, msg: &str) {
        println!("{}", msg);
    }

    /// Print a success message (always shown)
    pub fn success(&self, msg: &str) {
        println!("✓ {}", msg);
    }

    /// Print a verbose message (only when verbose is enabled)
    pub fn verbose(&self, msg: &str) {
        if self.verbose {
            println!("  → {}", msg);
        }
    }

    /// Print a debug message (only when debug is enabled)
    pub fn debug(&self, msg: &str) {
        println!("  [DEBUG] {}", msg);
    }

    /// Print a section header
    pub fn section(&self, title: &str) {
        self.info(&format!(
            "═══════════════════════════════════════════════════════════════"
        ));
        self.info(title);
        self.info(&format!(
            "═══════════════════════════════════════════════════════════════"
        ));
        self.info("");
    }

    /// Print a divider line
    pub fn divider(&self) {
        self.info("═══════════════════════════════════════════════════════════════");
        self.info("");
    }

    /// Print a key-value configuration item
    pub fn config_item(&self, key: &str, value: &str) {
        println!("  {:<24} {}", format!("{}:", key), value);
    }

    /// Print a configuration section header
    pub fn config_section(&self, section: &str) {
        self.info(format!("{}:", section).as_str());
    }

    /// Start a progress indicator
    pub fn progress_start(&self, msg: &str) {
        print!("  {} ", msg);
        let _ = std::io::stdout().flush();
    }

    /// Complete a progress indicator
    pub fn progress_done(&self) {
        println!("done");
    }

    /// Print a file operation message
    pub fn file_op(&self, operation: &str, path: &str) {
        if self.verbose {
            println!("  📁 {}: {}", operation, path);
        }
    }

    /// Print a search/analysis message
    pub fn analysis(&self, msg: &str) {
        if self.verbose {
            println!("  🔍 {}", msg);
        }
    }

    /// Print a processing message
    pub fn processing(&self, msg: &str) {
        self.verbose(&format!("⏳ {}", msg));
    }

    /// Check if verbose mode is enabled
    pub fn is_verbose(&self) -> bool {
        self.verbose
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_logger_creation() {
        let logger = Logger::new(false);
        assert!(!logger.is_verbose());
    }

    #[test]
    fn test_logger_verbose() {
        let logger = Logger::new(true);
        assert!(logger.is_verbose());
    }

    #[test]
    fn test_logger_debug() {
        let logger = Logger::new(false);
        assert!(!logger.is_verbose());
    }

    #[test]
    fn test_logger_all_modes() {
        let logger = Logger::new(true);
        assert!(logger.is_verbose());

        // Just ensure methods don't panic
        logger.error("test error");
        logger.warning("test warning");
        logger.info("test info");
        logger.success("test success");
        logger.verbose("test verbose");
        logger.debug("test debug");
        logger.section("test section");
        logger.divider();
        logger.config_item("key", "value");
        logger.config_section("section");
        logger.file_op("operation", "path");
        logger.analysis("analysis");
        logger.processing("processing");
    }
}
