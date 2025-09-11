
#include "_locale.h"

const char* TXT_NO_ERROR = "No error";
const char* TXT_CARD_MOUNT_FAILED = "Failed to mount SD card";
const char* TXT_NO_SD_CARD_ATTACHED = "No SD card attached";
const char* TXT_UNKNOWN_SD_CARD_TYPE = "Unknown SD card type";
const char* TXT_CARD_OPEN_FILE_FAILED = "Failed to open SD card file";
const char* TXT_SD_CARD_FILE_CREATE_FAILED = "Failed to create SD card file";
const char* TXT_SD_CARD_DIR_CREATE_FAILED = "Failed to create SD card directory";
const char* TXT_SD_CARD_FILE_NOT_FOUND = "SD card file not found";
const char* TXT_SD_CARD_FILE_OPEN_FAILED = "Failed to open SD card file";
const char* TXT_IMAGE_FORMAT_NOT_SUPPORTED = "Image format not supported";
const char* TXT_ERROR_CODE = "Error code: ";
const char* TXT_BATTERY_LEVEL_CRITICAL = "Battery level critical";
const char* TXT_CARD_TOC_OPEN_FILE_FAILED = "Failed to open SD card TOC file";
const char* TXT_PREFERENCES_OPEN_FAILED = "Failed to open preferences";
const char* TXT_NO_IMAGES_FOUND = "No images found";
const char* TXT_BATTERY_EMPTY = "Battery empty";
const char* TXT_RTC_MODULE_NOT_FOUND = "RTC module not found";
const char* TXT_JWT_CREATION_FAILED = "JWT creation failed";
const char* TXT_HTTP_POST_FAILED = "HTTP POST request failed";
const char* TXT_JSON_PARSE_FAILED = "JSON parsing failed";
const char* TXT_TOKEN_MISSING = "Token missing";
const char* TXT_FILE_OPEN_FAILED = "File open failed";
const char* TXT_HTTP_CONNECT_FAILED = "HTTP connection failed";
const char* TXT_HTTP_GET_FAILED = "HTTP GET request failed";
const char* TXT_DOWNLOAD_FAILED = "Download failed";
const char* TXT_SSL_CERTIFICATE_LOAD_FAILED = "SSL certificate load failed";
const char* TXT_RATE_LIMIT_TIMEOUT_EXCEEDED = "Rate limit timeout exceeded";
const char* TXT_WIFI_CREDENTIALS_NOT_FOUND = "WiFi credentials not found";
const char* TXT_WIFI_CONNECTION_FAILED = "WiFi connection failed";

// Google Drive configuration validation error messages
const char* TXT_CONFIG_MISSING_SECTION = "Missing configuration section";
const char* TXT_CONFIG_MISSING_FIELD = "Missing required field";
const char* TXT_CONFIG_INVALID_EMAIL = "Invalid email address format";
const char* TXT_CONFIG_INVALID_PEM_KEY = "Invalid PEM key format";
const char* TXT_CONFIG_INVALID_PATH = "Invalid file path";
const char* TXT_CONFIG_INVALID_FILENAME = "Invalid filename";
const char* TXT_CONFIG_VALUE_OUT_OF_RANGE = "Value out of valid range";

// ========================================
// COMPREHENSIVE ERROR SYSTEM - ENGLISH
// ========================================

// SD Card Specific Errors (100-119)
const char* TXT_SD_CARD_WRITE_PROTECTED = "SD card is write protected";
const char* TXT_SD_CARD_CORRUPTED = "SD card file system is corrupted";
const char* TXT_SD_CARD_INSUFFICIENT_SPACE = "SD card has insufficient free space";
const char* TXT_SD_CARD_SLOW_RESPONSE = "SD card is responding slowly";
const char* TXT_SD_CARD_READ_ONLY = "SD card is in read-only mode";
const char* TXT_SD_CARD_BAD_SECTOR = "SD card has bad sectors";
const char* TXT_SD_CARD_SIZE_INVALID = "SD card reports invalid size";
const char* TXT_SD_CARD_INIT_FAILED = "SD card initialization failed";
const char* TXT_SD_CARD_VERSION_UNSUPPORTED = "SD card version not supported";
const char* TXT_SD_CARD_FILESYSTEM_UNSUPPORTED = "SD card file system not supported";
// LittleFS Specific Errors (110-119)  
const char* TXT_LITTLEFS_INIT_FAILED = "LittleFS initialization failed";
const char* TXT_LITTLEFS_FILE_CREATE_FAILED = "Failed to create LittleFS file";
const char* TXT_LITTLEFS_FILE_OPEN_FAILED = "Failed to open LittleFS file";

// Network/WiFi Specific Errors (120-139)
const char* TXT_WIFI_SIGNAL_TOO_WEAK = "WiFi signal is too weak";
const char* TXT_WIFI_AUTHENTICATION_FAILED = "WiFi authentication failed";
const char* TXT_WIFI_DHCP_FAILED = "WiFi DHCP assignment failed";
const char* TXT_WIFI_DNS_RESOLUTION_FAILED = "WiFi DNS resolution failed";
const char* TXT_WIFI_NETWORK_NOT_FOUND = "WiFi network not found";
const char* TXT_WIFI_PASSWORD_INCORRECT = "WiFi password is incorrect";
const char* TXT_WIFI_CHANNEL_BUSY = "WiFi channel is busy or congested";
const char* TXT_WIFI_FREQUENCY_NOT_SUPPORTED = "WiFi frequency not supported";
const char* TXT_HTTP_REQUEST_TIMEOUT = "HTTP request timed out";
const char* TXT_HTTP_INVALID_RESPONSE = "HTTP response is invalid or malformed";
const char* TXT_SSL_HANDSHAKE_FAILED = "SSL/TLS handshake failed";
const char* TXT_NETWORK_INTERFACE_DOWN = "Network interface is down";
const char* TXT_NETWORK_CONFIG_INVALID = "Network configuration is invalid";
const char* TXT_PROXY_CONNECTION_FAILED = "Proxy connection failed";
const char* TXT_NETWORK_TIMEOUT_EXCEEDED = "Network operation timeout exceeded";

// Configuration Specific Errors (140-159)
const char* TXT_CONFIG_FILE_CORRUPTED = "Configuration file is corrupted";
const char* TXT_CONFIG_JSON_SYNTAX_ERROR = "Configuration JSON syntax error";
const char* TXT_CONFIG_VERSION_MISMATCH = "Configuration version mismatch";
const char* TXT_CONFIG_FIELD_TYPE_MISMATCH = "Configuration field type mismatch";
const char* TXT_CONFIG_ENCRYPTION_KEY_INVALID = "Configuration encryption key invalid";
const char* TXT_CONFIG_BACKUP_FAILED = "Configuration backup failed";
const char* TXT_CONFIG_RESTORE_FAILED = "Configuration restore failed";
const char* TXT_CONFIG_VALIDATION_FAILED = "Configuration validation failed";
const char* TXT_CONFIG_DEFAULTS_MISSING = "Configuration defaults missing";
const char* TXT_CONFIG_SCHEMA_INVALID = "Configuration schema is invalid";
const char* TXT_CONFIG_ACCESS_DENIED = "Configuration file access denied";
const char* TXT_CONFIG_FORMAT_UNSUPPORTED = "Configuration format not supported";
const char* TXT_CONFIG_SIZE_LIMIT_EXCEEDED = "Configuration size limit exceeded";
const char* TXT_CONFIG_DEPENDENCY_MISSING = "Configuration dependency missing";
const char* TXT_CONFIG_ENVIRONMENT_MISMATCH = "Configuration environment mismatch";

// OAuth/Authentication Specific Errors (40-49)
const char* TXT_OAUTH_TOKEN_EXPIRED = "OAuth access token has expired";
const char* TXT_OAUTH_TOKEN_INVALID = "OAuth access token is invalid or malformed";
const char* TXT_OAUTH_REFRESH_TOKEN_MISSING = "OAuth refresh token is missing";
const char* TXT_OAUTH_REFRESH_TOKEN_INVALID = "OAuth refresh token is invalid";
const char* TXT_OAUTH_SCOPE_INSUFFICIENT = "OAuth token has insufficient permissions";
const char* TXT_OAUTH_JWT_SIGNING_FAILED = "Failed to sign JWT for service account";
const char* TXT_OAUTH_SERVICE_ACCOUNT_KEY_INVALID = "Service account private key is invalid";
const char* TXT_OAUTH_TOKEN_REQUEST_FAILED = "Failed to request OAuth access token";
const char* TXT_OAUTH_TOKEN_REFRESH_FAILED = "Failed to refresh OAuth access token";

// Google Drive API Specific Errors (50-69)
const char* TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED = "Google Drive API quota exceeded";
const char* TXT_GOOGLE_DRIVE_API_RATE_LIMITED = "Google Drive API rate limit exceeded";
const char* TXT_GOOGLE_DRIVE_FILE_NOT_FOUND = "File not found in Google Drive";
const char* TXT_GOOGLE_DRIVE_FOLDER_NOT_FOUND = "Folder not found in Google Drive";
const char* TXT_GOOGLE_DRIVE_PERMISSION_DENIED = "Permission denied to access Google Drive resource";
const char* TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED = "Google Drive storage quota exceeded";
const char* TXT_GOOGLE_DRIVE_API_DISABLED = "Google Drive API is disabled for this project";
const char* TXT_GOOGLE_DRIVE_FILE_TOO_BIG = "File is too large to download";
const char* TXT_GOOGLE_DRIVE_FILE_CORRUPTED = "Downloaded file appears to be corrupted";
const char* TXT_GOOGLE_DRIVE_INVALID_QUERY = "Invalid query syntax for Google Drive search";
const char* TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR = "Google Drive API internal server error";
const char* TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT = "Network timeout while contacting Google Drive";
const char* TXT_GOOGLE_DRIVE_RESPONSE_MALFORMED = "Malformed response from Google Drive API";
const char* TXT_GOOGLE_DRIVE_API_UNAVAILABLE = "Google Drive API is temporarily unavailable";
const char* TXT_GOOGLE_DRIVE_DOWNLOAD_INTERRUPTED = "File download was interrupted";
const char* TXT_GOOGLE_DRIVE_METADATA_INVALID = "Invalid or missing file metadata";
const char* TXT_GOOGLE_DRIVE_FOLDER_EMPTY = "Google Drive folder contains no files";
const char* TXT_GOOGLE_DRIVE_API_VERSION_UNSUPPORTED = "Unsupported Google Drive API version";

// HTTP Specific Errors (70-79)
const char* TXT_HTTP_UNAUTHORIZED = "HTTP 401: Unauthorized access";
const char* TXT_HTTP_FORBIDDEN = "HTTP 403: Forbidden access";
const char* TXT_HTTP_NOT_FOUND = "HTTP 404: Resource not found";
const char* TXT_HTTP_TOO_MANY_REQUESTS = "HTTP 429: Too many requests";
const char* TXT_HTTP_INTERNAL_SERVER_ERROR = "HTTP 500: Internal server error";
const char* TXT_HTTP_BAD_GATEWAY = "HTTP 502: Bad gateway";
const char* TXT_HTTP_SERVICE_UNAVAILABLE = "HTTP 503: Service unavailable";
const char* TXT_HTTP_GATEWAY_TIMEOUT = "HTTP 504: Gateway timeout";
const char* TXT_HTTP_BAD_REQUEST = "HTTP 400: Bad request";

// Image Processing Specific Errors (80-99)
const char* TXT_IMAGE_FILE_CORRUPTED = "Image file appears to be corrupted or invalid";
const char* TXT_IMAGE_FILE_TOO_LARGE = "Image file is too large for processing";
const char* TXT_IMAGE_DIMENSIONS_INVALID = "Image dimensions are invalid or unsupported";
const char* TXT_IMAGE_DIMENSIONS_MISMATCH = "Image dimensions don't match expected size";
const char* TXT_IMAGE_COLOR_DEPTH_UNSUPPORTED = "Image color depth is not supported";
const char* TXT_IMAGE_PIXEL_DATA_CORRUPTED = "Pixel data is corrupted or unreadable";
const char* TXT_IMAGE_FILE_SEEK_FAILED = "Failed to seek to position in image file";
const char* TXT_IMAGE_FILE_READ_FAILED = "Failed to read data from image file";
const char* TXT_IMAGE_BUFFER_OVERFLOW = "Image processing buffer overflow";
const char* TXT_IMAGE_BUFFER_UNDERFLOW = "Image processing buffer underflow";
const char* TXT_IMAGE_RENDER_TIMEOUT = "Image rendering timed out";
const char* TXT_IMAGE_DISPLAY_WRITE_FAILED = "Failed to write pixel data to display";
const char* TXT_IMAGE_FILE_HEADER_INVALID = "Image file header is invalid or missing";
const char* TXT_IMAGE_FILE_EMPTY = "Image file is empty or has no content";
const char* TXT_IMAGE_FILE_TRUNCATED = "Image file appears to be truncated";
const char* TXT_IMAGE_MEMORY_ALLOCATION_FAILED = "Failed to allocate memory for image processing";
const char* TXT_IMAGE_PALETTE_INVALID = "Image palette data is invalid";
const char* TXT_IMAGE_CONVERSION_FAILED = "Failed to convert image data to display format";
const char* TXT_IMAGE_RESOLUTION_TOO_HIGH = "Image resolution exceeds display capabilities";
const char* TXT_IMAGE_PROCESSING_ABORTED = "Image processing was aborted due to error";

// Battery/Power Errors (160-199)
const char* TXT_BATTERY_VOLTAGE_LOW = "Battery voltage is below safe threshold";
const char* TXT_BATTERY_VOLTAGE_UNSTABLE = "Battery voltage is unstable or fluctuating";
const char* TXT_BATTERY_TEMPERATURE_HIGH = "Battery temperature is too high";
const char* TXT_BATTERY_TEMPERATURE_LOW = "Battery temperature is too low";
const char* TXT_BATTERY_AGING_DETECTED = "Battery shows signs of aging or degradation";
const char* TXT_BATTERY_CALIBRATION_NEEDED = "Battery gauge needs recalibration";
const char* TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH = "Battery discharge rate is abnormally high";
const char* TXT_BATTERY_CHARGE_RATE_TOO_SLOW = "Battery charging rate is too slow";
const char* TXT_BATTERY_CAPACITY_REDUCED = "Battery capacity has significantly reduced";
const char* TXT_BATTERY_HEALTH_POOR = "Battery health is poor";

// Charging Specific Errors (170-179)
const char* TXT_CHARGING_FAILED = "Battery charging has failed";
const char* TXT_CHARGER_NOT_CONNECTED = "Charger is not connected";
const char* TXT_CHARGER_INCOMPATIBLE = "Charger is incompatible or insufficient";
const char* TXT_CHARGING_OVERHEAT = "Charging stopped due to overheating";
const char* TXT_CHARGING_TIMEOUT = "Charging process timed out";
const char* TXT_CHARGE_CURRENT_TOO_HIGH = "Charge current is dangerously high";
const char* TXT_CHARGE_CURRENT_TOO_LOW = "Charge current is too low for efficient charging";
const char* TXT_CHARGING_CIRCUIT_FAULT = "Charging circuit has a fault";
const char* TXT_BATTERY_NOT_DETECTED = "Battery is not detected by the system";
const char* TXT_BATTERY_AUTHENTICATION_FAILED = "Battery authentication failed";

// Power Supply Errors (180-189)
const char* TXT_POWER_SUPPLY_INSUFFICIENT = "Power supply is insufficient for operation";
const char* TXT_POWER_SUPPLY_UNSTABLE = "Power supply voltage is unstable";
const char* TXT_POWER_SUPPLY_OVERVOLTAGE = "Power supply voltage is too high";
const char* TXT_POWER_SUPPLY_UNDERVOLTAGE = "Power supply voltage is too low";
const char* TXT_POWER_REGULATOR_FAILED = "Power regulator has failed";
const char* TXT_POWER_SUPPLY_NOISE = "Power supply has excessive noise or interference";
const char* TXT_POWER_SUPPLY_EFFICIENCY_LOW = "Power supply efficiency is below threshold";
const char* TXT_POWER_SUPPLY_OVERCURRENT = "Power supply overcurrent protection triggered";
const char* TXT_POWER_SUPPLY_SHORT_CIRCUIT = "Power supply short circuit detected";
const char* TXT_POWER_SUPPLY_DISCONNECTED = "Power supply has been disconnected";

// Power Management Errors (190-199)
const char* TXT_POWER_SAVING_MODE_ENTER_FAILED = "Failed to enter power saving mode";
const char* TXT_POWER_SAVING_MODE_EXIT_FAILED = "Failed to exit power saving mode";
const char* TXT_SLEEP_MODE_ACTIVATION_FAILED = "Failed to activate sleep mode";
const char* TXT_WAKEUP_SOURCE_INVALID = "Wakeup source configuration is invalid";
const char* TXT_POWER_CONSUMPTION_TOO_HIGH = "Power consumption exceeds expected limits";
const char* TXT_POWER_MANAGER_INIT_FAILED = "Power management system initialization failed";
const char* TXT_CLOCK_FREQUENCY_ERROR = "CPU clock frequency adjustment failed";
const char* TXT_VOLTAGE_SCALING_FAILED = "Dynamic voltage scaling failed";
const char* TXT_POWER_DOMAIN_ERROR = "Power domain control error";
const char* TXT_THERMAL_THROTTLING_ACTIVE = "Thermal throttling is active due to overheating";

// Display Hardware Errors (200-219)
const char* TXT_DISPLAY_INITIALIZATION_FAILED = "Display initialization failed";
const char* TXT_DISPLAY_DRIVER_ERROR = "Display driver communication error";
const char* TXT_DISPLAY_SPI_COMM_ERROR = "Display SPI communication error";
const char* TXT_DISPLAY_BUSY_TIMEOUT = "Display busy signal timeout";
const char* TXT_DISPLAY_RESET_FAILED = "Display reset sequence failed";
const char* TXT_DISPLAY_POWER_ON_FAILED = "Display power-on sequence failed";
const char* TXT_DISPLAY_POWER_OFF_FAILED = "Display power-off sequence failed";
const char* TXT_DISPLAY_WAKEUP_FAILED = "Display wakeup from sleep failed";
const char* TXT_DISPLAY_COMMAND_ERROR = "Display command execution error";
const char* TXT_DISPLAY_HARDWARE_FAULT = "Display hardware fault detected";

// E-Paper Specific Errors (210-229)
const char* TXT_EPAPER_REFRESH_FAILED = "E-paper display refresh failed";
const char* TXT_EPAPER_PARTIAL_REFRESH_NOT_SUPPORTED = "E-paper partial refresh not supported";
const char* TXT_EPAPER_GHOSTING_DETECTED = "E-paper ghosting artifacts detected";
const char* TXT_EPAPER_TEMPERATURE_COMPENSATION_FAILED = "E-paper temperature compensation failed";
const char* TXT_EPAPER_WAVEFORM_ERROR = "E-paper waveform selection error";
const char* TXT_EPAPER_VOLTAGE_REGULATION_ERROR = "E-paper voltage regulation error";
const char* TXT_EPAPER_PIXEL_STUCK_ERROR = "E-paper pixels permanently stuck";
const char* TXT_EPAPER_CONTRAST_POOR = "E-paper contrast is poor or faded";
const char* TXT_EPAPER_REFRESH_TOO_FREQUENT = "E-paper refresh rate too frequent";
const char* TXT_EPAPER_LIFETIME_EXCEEDED = "E-paper lifetime refresh count exceeded";

// Display Rendering Errors (230-249)
const char* TXT_DISPLAY_BUFFER_OVERFLOW = "Display rendering buffer overflow";
const char* TXT_DISPLAY_BUFFER_UNDERFLOW = "Display rendering buffer underflow";
const char* TXT_DISPLAY_MEMORY_ALLOCATION_FAILED = "Display memory allocation failed";
const char* TXT_DISPLAY_FRAMEBUFFER_CORRUPTED = "Display framebuffer is corrupted";
const char* TXT_DISPLAY_PIXEL_FORMAT_ERROR = "Display pixel format conversion error";
const char* TXT_DISPLAY_COLOR_SPACE_ERROR = "Display color space conversion error";
const char* TXT_DISPLAY_SCALING_ERROR = "Display image scaling error";
const char* TXT_DISPLAY_ROTATION_ERROR = "Display rotation operation error";
const char* TXT_DISPLAY_CLIPPING_ERROR = "Display content clipping error";
const char* TXT_DISPLAY_RENDERING_TIMEOUT = "Display rendering operation timed out";

// Display Configuration Errors (250-259)
const char* TXT_DISPLAY_RESOLUTION_MISMATCH = "Display resolution configuration mismatch";
const char* TXT_DISPLAY_COLOR_DEPTH_UNSUPPORTED = "Display color depth not supported";
const char* TXT_DISPLAY_ORIENTATION_INVALID = "Display orientation setting is invalid";
const char* TXT_DISPLAY_REFRESH_RATE_INVALID = "Display refresh rate setting is invalid";
const char* TXT_DISPLAY_GAMMA_CONFIG_ERROR = "Display gamma correction configuration error";
const char* TXT_DISPLAY_BRIGHTNESS_CONTROL_ERROR = "Display brightness control error";
const char* TXT_DISPLAY_CONTRAST_CONTROL_ERROR = "Display contrast control error";
const char* TXT_DISPLAY_TIMING_CONFIG_ERROR = "Display timing configuration error";
const char* TXT_DISPLAY_MODE_NOT_SUPPORTED = "Display mode not supported by hardware";
const char* TXT_DISPLAY_CALIBRATION_REQUIRED = "Display calibration is required";