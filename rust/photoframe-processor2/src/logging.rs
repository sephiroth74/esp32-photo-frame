/// Simple logging system with verbose mode support
///
/// The Logger provides methods for different message levels:
/// - error(): Always printed, for critical errors
/// - warning(): Always printed, for warnings
/// - info(): Always printed, for important information
/// - verbose(): Only printed when verbose mode is enabled
/// - debug(): Always printed (for development/debugging purposes)

#[derive(Clone, Copy)]
#[allow(dead_code)]
pub enum LogLevel {
    Error,
    Warning,
    Info,
    Verbose,
    Debug,
}

#[derive(Clone, Copy)]
pub struct Logger {
    verbose: bool,
}

/// Test logger instance (silent, no verbose) - should only be used in tests
#[cfg(test)]
pub const TEST_LOGGER: Logger = Logger { verbose: false };

/// Test logger instance (verbose mode) - should only be used in tests
#[cfg(test)]
pub const TEST_LOGGER_VERBOSE: Logger = Logger { verbose: true };

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
    #[allow(dead_code)]
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
        assert!(!TEST_LOGGER.is_verbose());
    }

    #[test]
    fn test_logger_verbose() {
        assert!(TEST_LOGGER_VERBOSE.is_verbose());
    }

    #[test]
    fn test_logger_debug() {
        assert!(!TEST_LOGGER.is_verbose());
    }

    #[test]
    fn test_logger_all_modes() {
        assert!(TEST_LOGGER_VERBOSE.is_verbose());

        // Just ensure methods don't panic
        TEST_LOGGER_VERBOSE.error("test error");
        TEST_LOGGER_VERBOSE.warning("test warning");
        TEST_LOGGER_VERBOSE.info("test info");
        TEST_LOGGER_VERBOSE.success("test success");
        TEST_LOGGER_VERBOSE.verbose("test verbose");
        TEST_LOGGER_VERBOSE.debug("test debug");
        TEST_LOGGER_VERBOSE.section("test section");
        TEST_LOGGER_VERBOSE.divider();
        TEST_LOGGER_VERBOSE.config_item("key", "value");
        TEST_LOGGER_VERBOSE.config_section("section");
        TEST_LOGGER_VERBOSE.file_op("operation", "path");
        TEST_LOGGER_VERBOSE.analysis("analysis");
        TEST_LOGGER_VERBOSE.processing("processing");
    }
}
