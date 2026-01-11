// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFiClientSecure.h>
#include <vector>

#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"

#include "config.h"
#include "errors.h"
#include "sd_card.h"

// Google Drive page token buffer size (Google Drive tokens are typically 100-200 chars)
// "nextPageToken":
// "~!!~AI9FV7Q4Gfow5ZkBdhXuhx79-W0ocOSHEiiXYs6okeePWmpITPB3mY8gXAqzrAn0xKRgyNc_9pAkEUrUBajPHnQQa49js9-njz_IfwjmDmIdm3yyyJ1KCPS3mZrcx_CQPbiZyqLY2odPLf0CbzvALTrPtZN4TODshZX2YQRraUVrc_bZWm_-6BCifCS0xcweij0n1aIQlIDIrCbwQwgYMxypRaCed1uKZVGiUfsqBC20HcM69OQ86bnhW9GDCDRInVT27vdLmyB_LnN8krrIjELMWZsVFMHQz5nS1Q20opzAnsDe_LQIlTKHbyce29oeFDyzToz9QoTx"
#define GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE 512

namespace photo_frame {

/**
 * @brief Configuration structure for Google Drive client.
 *
 * Contains the necessary credentials and configuration parameters
 * required to authenticate with Google Drive API using a service account.
 */
typedef struct {
    const char* serviceAccountEmail; ///< Service account email address
    const char* privateKeyPem;       ///< PEM-encoded private key for JWT signing
    const char* clientId;            ///< Client ID from Google Cloud Console
    bool useInsecureTls;             ///< Whether to use insecure TLS connections

    // Rate limiting configuration
    int rateLimitWindowSeconds; ///< Time window for rate limiting in seconds
    int minRequestDelayMs;      ///< Minimum delay between requests in milliseconds
    int maxRetryAttempts;       ///< Maximum retry attempts for failed requests
    int backoffBaseDelayMs;     ///< Base delay for exponential backoff in milliseconds
    int maxWaitTimeMs;          ///< Maximum wait time for rate limiting in milliseconds
} GoogleDriveClient_config;

typedef struct {
    char accessToken[512];    ///< Access token for Google Drive API
    unsigned long expiresAt;  ///< Expiration time of the access token
    unsigned long obtainedAt; ///< Timestamp when the access token was obtained

    bool expired(int marginSeconds = 0) const {
        time_t now = time(NULL);
        return (now + marginSeconds) >= expiresAt;
    }

    unsigned long expires_in() const {
        time_t now = time(NULL);
        return (now < expiresAt) ? (expiresAt - now) : 0;
    }

} GoogleDrive_access_token;

/**
 * @brief Error codes for Google Drive operations.
 *
 * Enumeration of possible error conditions that can occur during
 * Google Drive API interactions.
 */
enum class GoogleDrive_error {
    None,              ///< No error occurred
    JwtCreationFailed, ///< Failed to create JWT token for authentication
    HttpPostFailed,    ///< HTTP POST request failed
    JsonParseFailed,   ///< Failed to parse JSON response
    TokenMissing,      ///< Access token is missing or invalid
    FileOpenFailed,    ///< Failed to open local file for writing
    HttpConnectFailed, ///< Failed to establish HTTP connection
    HttpGetFailed,     ///< HTTP GET request failed
    DownloadFailed     ///< File download operation failed
};

/**
 * @brief HTTP response information for retry logic
 */
struct HttpResponse {
    int statusCode;
    String statusMessage;
    String body;
    bool hasContent;

    HttpResponse() : statusCode(0), hasContent(false) {}
};

/**
 * @brief Parsed HTTP response headers
 */
struct HttpResponseHeaders {
    bool isChunked;       ///< Whether response uses chunked transfer encoding
    long contentLength;   ///< Content-Length header value (-1 if not present)
    int headerCount;      ///< Number of headers parsed
    bool parseSuccessful; ///< Whether parsing completed successfully

    HttpResponseHeaders() :
        isChunked(false),
        contentLength(-1),
        headerCount(0),
        parseSuccessful(false) {}
};

/**
 * @brief Classification of failure types for retry logic
 */
enum class failure_type {
    Permanent,    // Don't retry (4xx errors, authentication failures)
    Transient,    // Retry with backoff (5xx errors, network issues)
    RateLimit,    // Special handling for 429 responses
    TokenExpired, // Token refresh needed (401 responses)
    Unknown       // Default fallback
};

/**
 * @brief Represents a file from Google Drive.
 *
 * Contains metadata information about a file stored in Google Drive,
 * including its unique identifier and name.
 */
class GoogleDriveFile {
  public:
    String id;   ///< Unique file identifier in Google Drive
    String name; ///< Display name of the file

    /** Default constructor */
    GoogleDriveFile() : id(""), name("") {}

    /**
     * @brief Constructor for GoogleDriveFile.
     * @param id Unique file identifier in Google Drive
     * @param name Display name of the file
     */
    GoogleDriveFile(const String& id, const String& name) : id(id), name(name) {}
};

/**
 * @class GoogleDriveClient
 * @brief A client for interacting with Google Drive using a service account.
 *
 * This class provides methods to authenticate with Google Drive via JWT, list files in folders,
 * download files to LittleFS, and manage access tokens. It uses mbedtls for cryptographic
 * operations.
 *
 * @param serviceAccountEmail The email address of the Google service account.
 * @param privateKeyPem The PEM-encoded private key for the service account.
 * @param clientId The client ID associated with the service account.
 *
 * @note Requires mbedtls for cryptographic operations and assumes LittleFS is available for file
 * storage.
 */
class GoogleDriveClient {
  public:
    /**
     * @brief Constructor for GoogleDriveClient.
     * @param config Configuration containing service account credentials
     */
    GoogleDriveClient(const GoogleDriveClient_config& config);

    /**
     * @brief Destructor for GoogleDriveClient.
     * Cleans up cryptographic contexts and resources.
     */
    ~GoogleDriveClient();

    /**
     * @brief Sets the access token for the Google Drive client.
     * @param token The access token to set
     */
    void set_access_token(const GoogleDrive_access_token& token);

    /**
     * @brief Retrieves an access token for authenticating requests.
     *
     * This method attempts to obtain a valid access token, which is required
     * for making authorized API calls. The implementation may involve refreshing
     * an expired token or acquiring a new one using stored credentials.
     *
     * @return GoogleDrive_error indicating the result of the operation.
     */
    photo_frame_error_t get_access_token();

    /**
     * @brief Lists files in a specified Google Drive folder and streams them directly to a TOC
     * file.
     *
     * This memory-efficient version writes files directly to the TOC file as they are parsed
     * from the API response, avoiding the need to keep all files in memory at once.
     *
     * @param folderId The ID of the Google Drive folder to list files from.
     * @param sdCard Reference to the SD card instance for file operations
     * @param tocFilePath Path to the TOC file to write entries to
     * @param pageSize The maximum number of files to retrieve per request (default is 50).
     * @return Total number of files written to the TOC, or 0 on error
     */
    size_t list_files_streaming(const char* folderId,
                                SdCard& sdCard,
                                const char* tocFilePath,
                                int pageSize = 50);

    /**
     * @brief Downloads a file from Google Drive to the specified file.
     *
     * @param fileId The unique identifier of the file on Google Drive.
     * @param outFile The file object to write the downloaded content to.
     * @return GoogleDrive_error indicating the result of the operation.
     */
    photo_frame_error_t download_file(const String& fileId, fs::File* outFile);

    /**
     * @brief Retrieves the current access token value.
     *
     * @return The access token as a String.
     */
    const GoogleDrive_access_token* get_access_token_value() const;

    /**
     * @brief Set the root CA certificate for SSL/TLS connections
     * @param rootCA The root CA certificate in PEM format
     */
    void set_root_ca_certificate(const String& rootCA);

    /**
     * @brief Check if the current access token is expired or about to expire
     * @param marginSeconds Safety margin in seconds before actual expiration (default: 60)
     * @return true if token is expired or will expire within margin
     */
    bool is_token_expired(int marginSeconds = 60);

  private:
    /**
     * @brief Creates a JWT token for Google Drive authentication.
     * @return JWT token as a string, or empty string on failure
     */
    String create_jwt();

    /**
     * @brief Signs data using RSA-SHA256 algorithm.
     * @param input Data to be signed
     * @param sig_b64url Output parameter for base64url-encoded signature
     * @return true if signing was successful, false otherwise
     */
    bool rsaSignRS256(const String& input, String& sig_b64url);

    /**
     * @brief Lists files in a folder with pagination support, streaming directly to TOC file.
     * @param folderId The ID of the Google Drive folder
     * @param sdCard Reference to the SD card instance for file operations
     * @param tocFilePath Path to the TOC file to write entries to
     * @param pageSize Maximum number of files per request (default: 10)
     * @param nextPageToken Pointer to store next page token (optional)
     * @param pageToken Token for requesting specific page (empty for first page)
     * @return Number of files written to TOC, or 0 on error
     */
    size_t list_files_in_folder_streaming(const char* folderId,
                                          SdCard& sdCard,
                                          const char* tocFilePath,
                                          int pageSize          = 10,
                                          char* nextPageToken   = nullptr,
                                          const char* pageToken = "");

    /**
     * @brief Streaming parser that writes files directly to TOC file with extension filtering.
     *
     * Parses Google Drive API JSON response and writes file entries to TOC file.
     * Automatically filters files based on allowed extensions at runtime, supporting
     * both binary (.bin) and bitmap (.bmp) formats.
     *
     * @param jsonBody JSON response body to parse
     * @param sdCard Reference to the SD card instance for file operations
     * @param tocFilePath Path to the TOC file to write entries to
     * @param nextPageToken Pointer to store next page token if present
     * @return Number of files written to TOC (after filtering), or 0 on error
     *
     * @note Only files with extensions in ALLOWED_FILE_EXTENSIONS array are written to TOC
     * @see ALLOWED_FILE_EXTENSIONS for supported file types
     */
    size_t parse_file_list_to_toc(const String& jsonBody,
                                  SdCard& sdCard,
                                  const char* tocFilePath,
                                  char* nextPageToken);

    /**
     * @brief Build HTTP request string with optimized memory allocation
     * @param method HTTP method (GET, POST, etc.)
     * @param path Request path/URL
     * @param host Target host
     * @param headers Additional headers (optional)
     * @param body Request body for POST requests (optional)
     * @return Complete HTTP request string
     */
    String build_http_request(const char* method,
                              const char* path,
                              const char* host,
                              const char* headers = nullptr,
                              const char* body    = nullptr);

    /**
     * @brief Parses HTTP response headers from a WiFi client connection.
     *
     * @param client WiFiClient to read headers from
     * @param verbose Whether to log each header for debugging
     * @return HttpResponseHeaders struct containing parsed header information
     */
    HttpResponseHeaders parse_http_headers(WiFiClientSecure& client, bool verbose = false);

    // Member variables

    /// Pointer to Google Drive API configuration containing credentials
    const GoogleDriveClient_config* config;

    /// Current OAuth2 access token for API authentication
    GoogleDrive_access_token g_access_token;

    /// mbedTLS entropy context for cryptographic operations
    mbedtls_entropy_context entropy;
    /// mbedTLS deterministic random bit generator context
    mbedtls_ctr_drbg_context ctr_drbg;

    // Rate limiting variables
    /// Timestamp of the last API request made
    unsigned long lastRequestTime;
    /// Circular buffer storing timestamps of recent requests for rate limiting
    unsigned long requestHistory[GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW];
    /// Current index in the request history circular buffer
    uint8_t requestHistoryIndex;
    /// Number of requests made in the current time window
    uint8_t requestCount;

    /**
     * @brief Check if we can make a request without violating rate limits
     * @return true if request is allowed, false if we need to wait
     */
    bool can_make_request();

    /**
     * @brief Wait for rate limit compliance before making a request
     * @return photo_frame_error_t indicating success or timeout
     */
    photo_frame_error_t wait_for_rate_limit();

    /**
     * @brief Record a new API request timestamp
     */
    void record_request();

    /**
     * @brief Handle rate limit response (HTTP 429) with exponential backoff
     * @param attempt Current retry attempt number (0-based)
     * @return true if should retry, false if max attempts reached
     */
    bool handle_rate_limit_response(uint8_t attempt);

    /**
     * @brief Clean old requests from history that are outside the time window
     */
    void clean_old_requests();

    /**
     * @brief Parse HTTP response to extract status code and headers
     * @param client WiFi client with response data
     * @param response HttpResponse structure to fill
     * @return true if parsing was successful
     */
    bool parse_http_response(WiFiClientSecure& client, HttpResponse& response);

    /**
     * @brief Classify failure type for retry logic
     * @param statusCode HTTP status code
     * @param hasNetworkError Whether there was a network-level error
     * @return failure_type classification
     */
    failure_type classify_failure(int statusCode, bool hasNetworkError = false);

    /**
     * @brief Handle transient failures with exponential backoff and jitter
     * @param attempt Current retry attempt number (0-based)
     * @param failureType Type of failure being handled
     * @return true if should retry, false if max attempts reached
     */
    bool handle_transient_failure(uint8_t attempt, failure_type failureType);

    /**
     * @brief Add random jitter to backoff delay to prevent thundering herd
     * @param base_delay Base delay in milliseconds
     * @return Delay with jitter applied
     */
    unsigned long add_jitter(unsigned long base_delay);

    /**
     * @brief Refresh the access token when it's expired or on 401 errors
     * @return photo_frame_error_t indicating success or failure
     */
    photo_frame_error_t refresh_token();
};

} // namespace photo_frame
