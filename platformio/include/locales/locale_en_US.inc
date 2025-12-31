
#include "_locale.h"

const char TXT_NO_ERROR[] PROGMEM = "No error";
const char TXT_CARD_MOUNT_FAILED[] PROGMEM = "Failed to mount SD card";
const char TXT_NO_SD_CARD_ATTACHED[] PROGMEM = "No SD card attached";
const char TXT_UNKNOWN_SD_CARD_TYPE[] PROGMEM = "Unknown SD card type";
const char TXT_CARD_OPEN_FILE_FAILED[] PROGMEM = "Failed to open SD card file";
const char TXT_SD_CARD_FILE_CREATE_FAILED[] PROGMEM = "Failed to create SD card file";
const char TXT_SD_CARD_DIR_CREATE_FAILED[] PROGMEM = "Failed to create SD card directory";
const char TXT_SD_CARD_FILE_NOT_FOUND[] PROGMEM = "SD card file not found";
const char TXT_SD_CARD_FILE_OPEN_FAILED[] PROGMEM = "Failed to open SD card file";
const char TXT_IMAGE_FORMAT_NOT_SUPPORTED[] PROGMEM = "Image format not supported";
const char TXT_ERROR_CODE[] PROGMEM = "Error code: ";
const char TXT_BATTERY_LEVEL_CRITICAL[] PROGMEM = "Battery level critical";
const char TXT_CARD_TOC_OPEN_FILE_FAILED[] PROGMEM = "Failed to open SD card TOC file";
const char TXT_PREFERENCES_OPEN_FAILED[] PROGMEM = "Failed to open preferences";
const char TXT_NO_IMAGES_FOUND[] PROGMEM = "No images found";
const char TXT_BATTERY_EMPTY[] PROGMEM = "Battery empty";
const char TXT_RTC_MODULE_NOT_FOUND[] PROGMEM = "RTC module not found";
const char TXT_JWT_CREATION_FAILED[] PROGMEM = "JWT creation failed";
const char TXT_HTTP_POST_FAILED[] PROGMEM = "HTTP POST request failed";
const char TXT_JSON_PARSE_FAILED[] PROGMEM = "JSON parsing failed";
const char TXT_TOKEN_MISSING[] PROGMEM = "Token missing";
const char TXT_FILE_OPEN_FAILED[] PROGMEM = "File open failed";
const char TXT_HTTP_CONNECT_FAILED[] PROGMEM = "HTTP connection failed";
const char TXT_HTTP_GET_FAILED[] PROGMEM = "HTTP GET request failed";
const char TXT_DOWNLOAD_FAILED[] PROGMEM = "Download failed";
const char TXT_SSL_CERTIFICATE_LOAD_FAILED[] PROGMEM = "SSL certificate load failed";
const char TXT_RATE_LIMIT_TIMEOUT_EXCEEDED[] PROGMEM = "Rate limit timeout exceeded";
const char TXT_WIFI_CREDENTIALS_NOT_FOUND[] PROGMEM = "WiFi credentials not found";
const char TXT_WIFI_CONNECTION_FAILED[] PROGMEM = "WiFi connection failed";

// Google Drive configuration validation error messages
const char TXT_CONFIG_MISSING_SECTION[] PROGMEM = "Missing configuration section";
const char TXT_CONFIG_MISSING_FIELD[] PROGMEM = "Missing required field";
const char TXT_CONFIG_INVALID_EMAIL[] PROGMEM = "Invalid email address format";
const char TXT_CONFIG_INVALID_PEM_KEY[] PROGMEM = "Invalid PEM key format";
const char TXT_CONFIG_INVALID_PATH[] PROGMEM = "Invalid file path";
const char TXT_CONFIG_INVALID_FILENAME[] PROGMEM = "Invalid filename";
const char TXT_CONFIG_VALUE_OUT_OF_RANGE[] PROGMEM = "Value out of valid range";

// ========================================
// COMPREHENSIVE ERROR SYSTEM - ENGLISH
// ========================================

// LittleFS Specific Errors (110-119)
const char TXT_LITTLEFS_INIT_FAILED[] PROGMEM = "LittleFS initialization failed";
const char TXT_LITTLEFS_FILE_CREATE_FAILED[] PROGMEM = "Failed to create LittleFS file";
const char TXT_LITTLEFS_FILE_OPEN_FAILED[] PROGMEM = "Failed to open LittleFS file";
const char TXT_FILE_COPY_FAILED[] PROGMEM = "Failed to copy file";



// OAuth/Authentication Specific Errors (40-49)
const char TXT_OAUTH_TOKEN_EXPIRED[] PROGMEM = "OAuth access token has expired";
const char TXT_OAUTH_TOKEN_INVALID[] PROGMEM = "OAuth access token is invalid or malformed";
const char TXT_OAUTH_REFRESH_TOKEN_INVALID[] PROGMEM = "OAuth refresh token is invalid";
const char TXT_OAUTH_SCOPE_INSUFFICIENT[] PROGMEM = "OAuth token has insufficient permissions";
const char TXT_OAUTH_TOKEN_REQUEST_FAILED[] PROGMEM = "Failed to request OAuth access token";

// Google Drive API Specific Errors (50-69)
const char TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED[] PROGMEM = "Google Drive API quota exceeded";
const char TXT_GOOGLE_DRIVE_API_RATE_LIMITED[] PROGMEM = "Google Drive API rate limit exceeded";
const char TXT_GOOGLE_DRIVE_FILE_NOT_FOUND[] PROGMEM = "File not found in Google Drive";
const char TXT_GOOGLE_DRIVE_PERMISSION_DENIED[] PROGMEM = "Permission denied to access Google Drive resource";
const char TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED[] PROGMEM = "Google Drive storage quota exceeded";
const char TXT_GOOGLE_DRIVE_API_DISABLED[] PROGMEM = "Google Drive API is disabled for this project";
const char TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR[] PROGMEM = "Google Drive API internal server error";
const char TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT[] PROGMEM = "Network timeout while contacting Google Drive";

// HTTP Specific Errors (70-79)
const char TXT_HTTP_UNAUTHORIZED[] PROGMEM = "HTTP 401: Unauthorized access";
const char TXT_HTTP_FORBIDDEN[] PROGMEM = "HTTP 403: Forbidden access";
const char TXT_HTTP_NOT_FOUND[] PROGMEM = "HTTP 404: Resource not found";
const char TXT_HTTP_TOO_MANY_REQUESTS[] PROGMEM = "HTTP 429: Too many requests";
const char TXT_HTTP_INTERNAL_SERVER_ERROR[] PROGMEM = "HTTP 500: Internal server error";
const char TXT_HTTP_BAD_GATEWAY[] PROGMEM = "HTTP 502: Bad gateway";
const char TXT_HTTP_SERVICE_UNAVAILABLE[] PROGMEM = "HTTP 503: Service unavailable";
const char TXT_HTTP_GATEWAY_TIMEOUT[] PROGMEM = "HTTP 504: Gateway timeout";
const char TXT_HTTP_BAD_REQUEST[] PROGMEM = "HTTP 400: Bad request";

// Image Processing Specific Errors (80-99)
const char TXT_IMAGE_FILE_CORRUPTED[] PROGMEM = "Image file appears to be corrupted or invalid";
const char TXT_IMAGE_FILE_TOO_LARGE[] PROGMEM = "Image file is too large for processing";
const char TXT_IMAGE_DIMENSIONS_INVALID[] PROGMEM = "Image dimensions are invalid or unsupported";
const char TXT_IMAGE_DIMENSIONS_MISMATCH[] PROGMEM = "Image dimensions don't match expected size";
const char TXT_IMAGE_COLOR_DEPTH_UNSUPPORTED[] PROGMEM = "Image color depth is not supported";
const char TXT_IMAGE_PIXEL_DATA_CORRUPTED[] PROGMEM = "Pixel data is corrupted or unreadable";
const char TXT_IMAGE_FILE_SEEK_FAILED[] PROGMEM = "Failed to seek to position in image file";
const char TXT_IMAGE_FILE_READ_FAILED[] PROGMEM = "Failed to read data from image file";
const char TXT_IMAGE_BUFFER_OVERFLOW[] PROGMEM = "Image processing buffer overflow";
const char TXT_IMAGE_DISPLAY_WRITE_FAILED[] PROGMEM = "Failed to write pixel data to display";
const char TXT_IMAGE_FILE_HEADER_INVALID[] PROGMEM = "Image file header is invalid or missing";
const char TXT_IMAGE_FILE_EMPTY[] PROGMEM = "Image file is empty or has no content";
const char TXT_IMAGE_FILE_TRUNCATED[] PROGMEM = "Image file appears to be truncated";
const char TXT_IMAGE_MEMORY_ALLOCATION_FAILED[] PROGMEM = "Failed to allocate memory for image processing";
const char TXT_IMAGE_CONVERSION_FAILED[] PROGMEM = "Failed to convert image data to display format";
const char TXT_IMAGE_RESOLUTION_TOO_HIGH[] PROGMEM = "Image resolution exceeds display capabilities";
const char TXT_IMAGE_PROCESSING_ABORTED[] PROGMEM = "Image processing was aborted due to error";
const char TXT_IMAGE_DIMENSIONS_NOT_PROVIDED[] PROGMEM = "Expected image dimensions not provided";

// Battery/Power Errors (160-199)
const char TXT_BATTERY_VOLTAGE_LOW[] PROGMEM = "Battery voltage is below safe threshold";
const char TXT_BATTERY_VOLTAGE_UNSTABLE[] PROGMEM = "Battery voltage is unstable or fluctuating";
const char TXT_BATTERY_TEMPERATURE_HIGH[] PROGMEM = "Battery temperature is too high";
const char TXT_BATTERY_TEMPERATURE_LOW[] PROGMEM = "Battery temperature is too low";
const char TXT_BATTERY_AGING_DETECTED[] PROGMEM = "Battery shows signs of aging or degradation";
const char TXT_BATTERY_CALIBRATION_NEEDED[] PROGMEM = "Battery gauge needs recalibration";
const char TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH[] PROGMEM = "Battery discharge rate is abnormally high";
const char TXT_BATTERY_CHARGE_RATE_TOO_SLOW[] PROGMEM = "Battery charging rate is too slow";
const char TXT_BATTERY_CAPACITY_REDUCED[] PROGMEM = "Battery capacity has significantly reduced";
const char TXT_BATTERY_HEALTH_POOR[] PROGMEM = "Battery health is poor";

// Charging Specific Errors (170-179)
const char TXT_CHARGING_FAILED[] PROGMEM = "Battery charging has failed";
const char TXT_CHARGER_NOT_CONNECTED[] PROGMEM = "Charger is not connected";
const char TXT_CHARGER_INCOMPATIBLE[] PROGMEM = "Charger is incompatible or insufficient";
const char TXT_CHARGING_OVERHEAT[] PROGMEM = "Charging stopped due to overheating";
const char TXT_CHARGING_TIMEOUT[] PROGMEM = "Charging process timed out";
const char TXT_CHARGE_CURRENT_TOO_HIGH[] PROGMEM = "Charge current is dangerously high";
const char TXT_CHARGE_CURRENT_TOO_LOW[] PROGMEM = "Charge current is too low for efficient charging";
const char TXT_CHARGING_CIRCUIT_FAULT[] PROGMEM = "Charging circuit has a fault";
const char TXT_BATTERY_NOT_DETECTED[] PROGMEM = "Battery is not detected by the system";
const char TXT_BATTERY_AUTHENTICATION_FAILED[] PROGMEM = "Battery authentication failed";




