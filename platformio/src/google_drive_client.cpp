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

#include "google_drive_client.h"
#include <HTTPClient.h>
#include <unistd.h> // For fsync()
#include <esp_heap_caps.h> // For ps_malloc/ps_free

// OAuth/Google endpoints
static const char TOKEN_HOST[] PROGMEM      = "oauth2.googleapis.com";
static const char TOKEN_PATH[] PROGMEM      = "/token";
static const char DRIVE_HOST[] PROGMEM      = "www.googleapis.com";
static const char DRIVE_LIST_PATH[] PROGMEM = "/drive/v3/files";
static const char DRIVE_FILE_PATH[] PROGMEM = "/drive/v3/files/"; // file ID will be appended
static const char SCOPE[] PROGMEM           = "https://www.googleapis.com/auth/drive.readonly";
static const char AUD[] PROGMEM             = "https://oauth2.googleapis.com/token";

// Global variable to store loaded root CA certificate
String g_google_root_ca;

String readHttpBody(WiFiClientSecure& client) {
    // Skip headers first
    uint16_t headerCount = 0;
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r")
            break; // end of headers

        // Yield every 10 headers to prevent watchdog timeout
        if (++headerCount % 10 == 0) {
            yield();
        }
    }

    // Read body in chunks to handle large responses
    String body;
    body.reserve(GOOGLE_DRIVE_BODY_RESERVE_SIZE); // Platform-specific reserve size

    char buffer[1024];
    size_t totalRead = 0;

    while (client.connected() || client.available()) {
        size_t bytesRead = client.readBytes(buffer, sizeof(buffer) - 1);
        if (bytesRead == 0) {
            yield();
            continue;
        }

        buffer[bytesRead] = '\0'; // Null terminate
        body += buffer;
        totalRead += bytesRead;

        // Yield every 4KB to prevent watchdog timeout
        if (totalRead % 4096 == 0) {
            yield();
        }

        // Safety limit to prevent memory overflow - platform specific
        if (totalRead > GOOGLE_DRIVE_SAFETY_LIMIT) {
            log_w("Response too large, truncating");
            break;
        }
    }

    log_i("Body read complete, length: %zu", body.length());
    log_i("Total bytes read: %zu", totalRead);

    return body;
}

// Helper functions
// use approved answer here: https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
static String urlEncode(const String& s) {
    String o;
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            o += c;
        } else {
            o += '%';
            o += hex[(c >> 4) & 0xF];
            o += hex[c & 0xF];
        }

        // Yield every 100 characters to prevent watchdog timeout on very long URLs
        if (i % 100 == 0) {
            yield();
        }
    }
    return o;
}

static String base64url(const uint8_t* data, size_t len) {
    size_t out_len = 4 * ((len + 2) / 3) + 4;
    std::unique_ptr<unsigned char[]> out(new unsigned char[out_len]);
    size_t olen = 0;
    if (mbedtls_base64_encode(out.get(), out_len, &olen, data, len) != 0) {
        return String();
    }
    String b64((char*)out.get(), olen);
    b64.replace("+", "-");
    b64.replace("/", "_");
    while (b64.endsWith("="))
        b64.remove(b64.length() - 1);
    return b64;
}

static String base64url(const String& s) {
    return base64url((const uint8_t*)s.c_str(), s.length());
}

// Helper function to safely copy string to char buffer
static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (dest && src && dest_size > 0) {
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else if (dest && dest_size > 0) {
        dest[0] = '\0';
    }
}

namespace photo_frame {

google_drive_client::google_drive_client(const google_drive_client_config& config) :
    config(&config),
    g_access_token{.accessToken = {0}, .expiresAt = 0, .obtainedAt = 0},
    lastRequestTime(0),
    requestHistoryIndex(0),
    requestCount(0) {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    const char* pers = "esp32jwt";
    mbedtls_ctr_drbg_seed(
        &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));

    // Initialize request history array
    for (uint8_t i = 0; i < GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW; i++) {
        requestHistory[i] = 0;
    }

#ifdef DEBUG_GOOGLE_DRIVE
    log_d("Google Drive rate limiting initialized");
#endif // DEBUG_GOOGLE_DRIVE
}

google_drive_client::~google_drive_client() {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void google_drive_client::clean_old_requests() {
    unsigned long currentTime     = millis();
    unsigned long windowStartTime = currentTime - (config->rateLimitWindowSeconds * 1000UL);

    uint8_t validRequests         = 0;
    for (uint8_t i = 0; i < requestCount; i++) {
        if (requestHistory[i] > windowStartTime) {
            validRequests++;
        }
    }

    // Compact the array if needed
    if (validRequests < requestCount) {
        uint8_t writeIndex = 0;
        for (uint8_t i = 0; i < requestCount; i++) {
            if (requestHistory[i] > windowStartTime) {
                requestHistory[writeIndex] = requestHistory[i];
                writeIndex++;
            }
        }
        requestCount        = validRequests;
        requestHistoryIndex = requestCount;
    }
}

bool google_drive_client::can_make_request() {
    clean_old_requests();

    unsigned long currentTime = millis();

    // Check minimum delay between requests
    if (lastRequestTime > 0 && (currentTime - lastRequestTime) < config->minRequestDelayMs) {
        log_w("Minimum request delay not met");
        return false;
    }

    // Check if we're within the rate limit window
    if (requestCount >= GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
        log_w("Rate limit reached: %u/%u requests in window", requestCount, GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
        return false;
    }

    return true;
}

photo_frame_error_t google_drive_client::wait_for_rate_limit() {
    unsigned long startTime = millis();

    while (!can_make_request()) {
        // Check if we've exceeded the maximum wait time
        unsigned long elapsedTime = millis() - startTime;
        if (elapsedTime >= config->maxWaitTimeMs) {
            log_w("Rate limit wait timeout after %lums - aborting to conserve battery", elapsedTime);
            return error_type::RateLimitTimeoutExceeded;
        }

        log_i("Waiting for rate limit compliance...");

        unsigned long currentTime     = millis();
        unsigned long nextAllowedTime = lastRequestTime + config->minRequestDelayMs;

        if (currentTime < nextAllowedTime) {
            unsigned long waitTime = nextAllowedTime - currentTime;

            // Ensure the wait time doesn't exceed our remaining budget
            unsigned long remainingBudget = config->maxWaitTimeMs - elapsedTime;
            if (waitTime > remainingBudget) {
                log_w("Wait time (%lums) would exceed budget, aborting", waitTime);
                return error_type::RateLimitTimeoutExceeded;
            }

            log_i("Delaying %lums for rate limit", waitTime);
            delay(waitTime);
        } else {
            // Wait a bit more for the sliding window to clear
            unsigned long remainingBudget = config->maxWaitTimeMs - elapsedTime;
            if (remainingBudget < 1000) {
                log_w("Insufficient time budget remaining, aborting");
                return error_type::RateLimitTimeoutExceeded;
            }
            delay(1000);
        }

        yield(); // Allow other tasks to run
    }

    return error_type::None;
}

void google_drive_client::record_request() {
    unsigned long currentTime = millis();
    lastRequestTime           = currentTime;

    if (requestCount < GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
        requestHistory[requestCount] = currentTime;
        requestCount++;
    } else {
        // Circular buffer - overwrite oldest entry
        requestHistory[requestHistoryIndex] = currentTime;
        requestHistoryIndex = (requestHistoryIndex + 1) % GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW;
    }

#ifdef DEBUG_GOOGLE_DRIVE
    log_d("Request recorded (%u/%u)", requestCount, GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
#endif // DEBUG_GOOGLE_DRIVE
}

bool google_drive_client::handle_rate_limit_response(uint8_t attempt) {
    if (attempt >= config->maxRetryAttempts) {
        log_w("Max retry attempts reached for rate limit");
        return false;
    }

    // Exponential backoff: base delay * 2^attempt
    unsigned long backoffDelay = config->backoffBaseDelayMs * (1UL << attempt);

    // Cap the delay to prevent excessive waiting (max 2 minutes)
    if (backoffDelay > GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS) {
        backoffDelay = GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS;
    }

    log_w("Rate limited (HTTP 429). Backing off for %lums (attempt %u/%u)", backoffDelay, attempt + 1, config->maxRetryAttempts);

    delay(backoffDelay);
    yield();

    return true;
}

bool google_drive_client::rsaSignRS256(const String& input, String& sig_b64url) {
    uint8_t hash[32];
    mbedtls_md_context_t mdctx;
    mbedtls_md_init(&mdctx);
    const mbedtls_md_info_t* mdinfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!mdinfo) {
        log_e("Failed to get MD info for SHA256");
        return false;
    }
    if (mbedtls_md_setup(&mdctx, mdinfo, 0) != 0) {
        log_e("Failed to setup MD context");
        return false;
    }
    mbedtls_md_starts(&mdctx);
    mbedtls_md_update(&mdctx, (const unsigned char*)input.c_str(), input.length());
    mbedtls_md_finish(&mdctx, hash);
    mbedtls_md_free(&mdctx);

    log_d("Parsing private key, length: %zu", strlen(config->privateKeyPem));
    log_d("First 50 chars: %.50s", config->privateKeyPem);

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-C6 and newer variants use updated mbedtls API with additional parameters
    int ret = mbedtls_pk_parse_key(&pk,
                                   (const unsigned char*)(config->privateKeyPem),
                                   strlen(config->privateKeyPem) + 1,
                                   NULL,
                                   0,
                                   NULL,
                                   NULL);
#else
    // ESP32, ESP32-S2, ESP32-S3 use older mbedtls API
    int ret = mbedtls_pk_parse_key(&pk,
                                   (const unsigned char*)(config->privateKeyPem),
                                   strlen(config->privateKeyPem) + 1,
                                   NULL,
                                   0);
#endif
    if (ret != 0) {
        log_e("Failed to parse private key, error code: -0x%04X", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }
    log_d("Private key parsed successfully");
    std::unique_ptr<unsigned char[]> sig(new unsigned char[MBEDTLS_PK_SIGNATURE_MAX_SIZE]);
    size_t sig_len = 0;
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-C6 and newer variants use updated mbedtls API with sig_size parameter
    ret = mbedtls_pk_sign(&pk,
                          MBEDTLS_MD_SHA256,
                          hash,
                          sizeof(hash),
                          sig.get(),
                          MBEDTLS_PK_SIGNATURE_MAX_SIZE,
                          &sig_len,
                          mbedtls_ctr_drbg_random,
                          &ctr_drbg);
#else
    // ESP32, ESP32-S2, ESP32-S3 use older mbedtls API without sig_size parameter
    ret = mbedtls_pk_sign(&pk,
                          MBEDTLS_MD_SHA256,
                          hash,
                          sizeof(hash),
                          sig.get(),
                          &sig_len,
                          mbedtls_ctr_drbg_random,
                          &ctr_drbg);
#endif
    mbedtls_pk_free(&pk);
    if (ret != 0)
        return false;
    sig_b64url = base64url(sig.get(), sig_len);
    return sig_b64url.length() > 0;
}

String google_drive_client::create_jwt() {
    StaticJsonDocument<64> hdr;
    hdr["alg"] = "RS256";
    hdr["typ"] = "JWT";
    String hdrStr;
    serializeJson(hdr, hdrStr);
    String hdrB64      = base64url(hdrStr);
    time_t now         = time(NULL);

    // Validate system time is reasonable (between 2020 and 2100)
    if (now < 1577836800 || now > 4102444800) {  // Unix timestamps for 2020 and 2100
        log_e("ERROR: System time is invalid: %ld", now);
        log_e("JWT will likely fail due to invalid timestamps");
        log_e("Check RTC module or NTP sync");
    }

    const uint32_t iat = (uint32_t)now;
    const uint32_t exp = iat + 3600;

#ifdef DEBUG_GOOGLE_DRIVE
    log_d("JWT timestamps - iat: %u (%ld), exp: %u", iat, now, exp);
#endif
    StaticJsonDocument<384> claims;
    claims["iss"]   = config->serviceAccountEmail;
    claims["scope"] = SCOPE;
    claims["aud"]   = AUD;
    claims["iat"]   = iat;
    claims["exp"]   = exp;
    String claimStr;
    serializeJson(claims, claimStr);
    String claimB64 = base64url(claimStr);
    // Build JWT signing input with pre-allocation
    String signingInput;
    signingInput.reserve(hdrB64.length() + claimB64.length() + 2);
    signingInput = hdrB64;
    signingInput += ".";
    signingInput += claimB64;
    String sigB64url;
    if (!rsaSignRS256(signingInput, sigB64url)) {
        return String();
    }
    return signingInput + "." + sigB64url;
}

photo_frame_error_t google_drive_client::get_access_token() {
    log_i("Requesting new access token...");

    String jwt = create_jwt();
    if (jwt.isEmpty())
        return error_type::JwtCreationFailed;

#ifdef DEBUG_GOOGLE_DRIVE
    log_d("JWT created, length: %zu", jwt.length());
#endif // DEBUG_GOOGLE_DRIVE

    String body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" +
                  urlEncode(jwt);

    // Apply rate limiting before making the request
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        log_w("Rate limit wait timeout - aborting token request");
        return rateLimitError;
    }

    uint8_t attempt = 0;
    while (attempt < config->maxRetryAttempts) {
        record_request();

        // Use HTTPClient for OAuth token request
        HTTPClient http;
        WiFiClientSecure client; // Stack allocation to avoid memory management issues
        bool networkError   = false;
        int statusCode      = 0;
        String responseBody = "";

        // Set up SSL/TLS
        if (config->useInsecureTls) {
            log_w("WARNING: Using insecure TLS connection");
            client.setInsecure();
        } else {
            if (g_google_root_ca.length() > 0) {
                client.setCACert(g_google_root_ca.c_str());
            } else {
                log_w("WARNING: No root CA certificate loaded, using insecure connection");
                client.setInsecure();
            }
        }

        log_i("Connecting to token endpoint: %s", TOKEN_HOST);

        String tokenUrl = "https://";
        tokenUrl += TOKEN_HOST;
        tokenUrl += TOKEN_PATH;

        http.begin(client, tokenUrl);
        http.setTimeout(HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("User-Agent", "ESP32-PhotoFrame");

#ifdef DEBUG_GOOGLE_DRIVE
        log_d("POST %s", tokenUrl.c_str());
        log_d("Request body length: %zu", body.length());
        log_d("Request body: %s", body.c_str());
#endif // DEBUG_GOOGLE_DRIVE

        int httpCode = http.POST(body);

        if (httpCode > 0) {
            statusCode = httpCode;

            // Get content length to allocate proper buffer size
            int contentLength  = http.getSize();
            WiFiClient* stream = http.getStreamPtr();

            if (contentLength > 0 && contentLength < GOOGLE_DRIVE_SAFETY_LIMIT) {
                // Allocate buffer in PSRAM
                char* buffer = (char*)heap_caps_malloc(contentLength + 1, MALLOC_CAP_SPIRAM);
                if (!buffer) {
                    // Fallback to regular heap if PSRAM allocation fails
                    buffer = (char*)malloc(contentLength + 1);
                }

                if (buffer) {
                    // Read response in chunks
                    int totalRead = 0;
                    while (stream->available() && totalRead < contentLength) {
                        int bytesToRead = min(1024, contentLength - totalRead);
                        int bytesRead   = stream->readBytes(buffer + totalRead, bytesToRead);
                        if (bytesRead == 0)
                            break;
                        totalRead += bytesRead;
                        yield(); // Prevent watchdog timeout
                    }
                    buffer[totalRead] = '\0';
                    responseBody      = String(buffer); // Convert to String for compatibility
                    free(buffer);

                    log_i("Read %d bytes from token response", totalRead);
                } else {
                    log_e("Failed to allocate response buffer");
                    networkError = true;
                }
            } else {
                // Fallback to getString for unknown/large content
                log_i("Using fallback getString, content length: %d", contentLength);
                responseBody = http.getString();
            }
        } else {
            log_e("HTTP POST failed: %d", httpCode);
            networkError = true;
        }

        http.end();

        // Classify failure and decide whether to retry
        failure_type failureType = classify_failure(statusCode, networkError);

        if (failureType == failure_type::Permanent) {
            log_e("Permanent failure (HTTP %d), not retrying", statusCode);

            // Special handling for HTTP 400 - Bad Request
            if (statusCode == 400) {
                log_e("HTTP 400 Bad Request - Debugging info:");
                log_e("Response body: %s", responseBody.c_str());

                // Check for specific OAuth errors
                if (responseBody.indexOf("invalid_grant") >= 0) {
                    log_e("Error: invalid_grant - Service account may not have access or credentials are incorrect");
                } else if (responseBody.indexOf("invalid_client") >= 0) {
                    log_e("Error: invalid_client - Client ID or private key may be incorrect");
                } else if (responseBody.indexOf("invalid_request") >= 0) {
                    log_e("Error: invalid_request - JWT format or claims may be incorrect");
                }

                // Log JWT creation time to check for clock skew
                log_e("System time: %ld", time(NULL));
                log_e("Note: HTTP 400 can be caused by:");
                log_e("  1. Invalid service account credentials");
                log_e("  2. System clock too far off (check RTC time)");
                log_e("  3. Malformed JWT or missing required claims");
                log_e("  4. Service account doesn't have access to Google Drive API");
            }

            // Return specific error based on failure type and status code
            return photo_frame::error_utils::map_google_drive_error(
                statusCode, responseBody.c_str(), "OAuth token request");
        }

        // Success case
        if (statusCode == 200 && responseBody.length() > 0) {
            // Parse token from response body
            size_t freeHeap = ESP.getFreeHeap();
            log_i("Free heap before token parse: %zu", freeHeap);

            StaticJsonDocument<768> doc;
            DeserializationError err = deserializeJson(doc, responseBody);
            if (err) {
                log_e("Token parse error: %s", err.c_str());
#ifdef DEBUG_GOOGLE_DRIVE
                log_d("Response body: %s", responseBody.c_str());
#endif // DEBUG_GOOGLE_DRIVE
       // Check if this is an OAuth error response
                if (responseBody.indexOf("invalid_grant") >= 0 ||
                    responseBody.indexOf("invalid_client") >= 0) {
                    return photo_frame::error_utils::create_oauth_error(
                        "invalid_grant", "OAuth token response parsing failed");
                }
                return photo_frame::error_utils::create_oauth_error(
                    "invalid_token", "Failed to parse OAuth token response");
            }

            const char* tok         = doc["access_token"];
            unsigned int expires_in = doc["expires_in"];

            if (!tok) {
                log_e("No access_token in response");
                return error_type::OAuthTokenRequestFailed;
            }

            time_t tokenExpirationTime = time(NULL) + expires_in;

            safe_strcpy(g_access_token.accessToken, tok, sizeof(g_access_token.accessToken));
            g_access_token.expiresAt  = tokenExpirationTime;
            g_access_token.obtainedAt = time(NULL);

            log_i("Token: %s", g_access_token.accessToken);
            log_i("Token expires at: %ld", tokenExpirationTime);
            return error_type::None;
        }

        // Handle transient failures with proper backoff
        if (attempt < config->maxRetryAttempts - 1) {
            if (failureType == failure_type::RateLimit) {
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
            } else {
                if (!handle_transient_failure(attempt, failureType)) {
                    break;
                }
            }
            attempt++;
        } else {
            log_w("Max retry attempts reached for access token");
            break;
        }
    }

    return error_type::HttpPostFailed;
}

photo_frame_error_t google_drive_client::download_file(const String& fileId, fs::File* outFile) {
    // Check if token is expired and refresh if needed
    if (is_token_expired()) {
        log_i("Token expired, refreshing before download");
        photo_frame_error_t refreshError = refresh_token();
        if (refreshError != error_type::None) {
            log_e("Failed to refresh expired token");
            return refreshError;
        }
    }

    // Build download path with optimized String concatenation
    String path;
    path.reserve(fileId.length() + 30); // "/drive/v3/files/" + "?alt=media"
    path = DRIVE_FILE_PATH;
    path += fileId;
    path += "?alt=media";

    // Apply rate limiting before making the request
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        log_w("Rate limit wait timeout - aborting download request");
        return rateLimitError;
    }

    uint8_t attempt = 0;
    while (attempt < config->maxRetryAttempts) {
        record_request();

        WiFiClientSecure client;

        if (config->useInsecureTls) {
            client.setInsecure();
        } else {
            if (g_google_root_ca.length() > 0) {
                client.setCACert(g_google_root_ca.c_str());
            } else {
                log_w("WARNING: No root CA certificate loaded, connection may fail");
                client.setInsecure(); // Fallback to insecure connection
            }
        }
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000);          // Set socket timeout in seconds
        client.setHandshakeTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set handshake timeout in seconds

        unsigned long connectStart = millis();
        log_i("Starting SSL connection to %s:443...", DRIVE_HOST);

        bool connected = client.connect(DRIVE_HOST, 443);
        unsigned long connectDuration = millis() - connectStart;

        log_i("Connection attempt took %lu ms (timeout: %d ms)",
              connectDuration, HTTP_CONNECT_TIMEOUT);

        if (!connected) {
            log_e("SSL connection failed after %lu ms", connectDuration);
            if (attempt < config->maxRetryAttempts - 1) {
                log_w("Download connection failed, retrying...");
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpConnectFailed;
        } else if (connectDuration > HTTP_CONNECT_TIMEOUT) {
            log_w("Download connection timeout");
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                log_w("Retrying after timeout...");
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpConnectFailed;
        }

        // Build request using helper method
        String headers = "Authorization: Bearer ";
        headers += g_access_token.accessToken;
        String req = build_http_request("GET", path.c_str(), DRIVE_HOST, headers.c_str());

#ifdef DEBUG_GOOGLE_DRIVE
        log_d("HTTP Request: %s", req.c_str());
#endif // DEBUG_GOOGLE_DRIVE

        client.print(req);

        // Read and validate HTTP status line first
        String statusLine;
        do {
            statusLine = client.readStringUntil('\n');
        } while (statusLine.length() == 0);

        statusLine.trim();
        log_i("HTTP Status: %s", statusLine.c_str());

        // Check if status is 200 OK
        if (statusLine.indexOf("200") < 0) {
            log_e("HTTP error - not 200 OK");
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                log_w("Retrying after HTTP error...");
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpGetFailed;
        }

        // Parse HTTP response headers
        HttpResponseHeaders responseHeaders =
            parse_http_headers(client, true); // verbose logging for downloads
        if (!responseHeaders.parseSuccessful) {
            log_e("Failed to parse HTTP headers");
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                log_w("Retrying after header parse failure...");
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpGetFailed;
        }

        // Download file content based on encoding type
        bool readAny            = false;
        uint32_t bytesRead      = 0;

        unsigned long startTime = millis();

        if (responseHeaders.isChunked) {
            // Handle chunked transfer encoding
            log_i("Reading chunked response");
            while (client.connected() || client.available()) {
                // Read chunk size (hex number followed by \r\n)
                String chunkSizeLine = client.readStringUntil('\n');
                chunkSizeLine.trim();

                if (chunkSizeLine.length() == 0)
                    continue;

                // Parse hex chunk size
                long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);

                if (chunkSize == 0) {
                    // End of chunks
                    log_i("End of chunked data");
                    break;
                }

                // Read the actual chunk data
                uint8_t buf[2048];
                long remainingInChunk = chunkSize;

                while (remainingInChunk > 0 && (client.connected() || client.available())) {
                    int toRead = min((long)sizeof(buf), remainingInChunk);
                    int n      = client.read(buf, toRead);

                    if (n > 0) {
                        outFile->write(buf, n);
                        outFile->flush(); // Ensure data is written to SD card
                        readAny = true;
                        bytesRead += n;
                        remainingInChunk -= n;

                        // Yield every 8KB
                        if (bytesRead % 8192 == 0) {
                            yield();
                        }
                    } else {
                        yield();
                    }
                }

                // Read trailing \r\n after chunk data
                client.readStringUntil('\n');
            }

            // Flush file buffers after chunked download
            if (readAny) {
                log_i("Flushing chunked file buffers to disk...");
                outFile->flush();
                delay(500); // Small delay to ensure flush completes
            }
        } else {
            // Handle regular (non-chunked) response
            log_i("Reading non-chunked response");
            uint8_t buf[2048];
            long remainingBytes = responseHeaders.contentLength;

            while (client.connected() || client.available()) {
                int toRead = sizeof(buf);
                if (responseHeaders.contentLength > 0 && remainingBytes > 0) {
                    toRead = min((long)sizeof(buf), remainingBytes);
                }

                int n = client.read(buf, toRead);
                if (n > 0) {
                    outFile->write(buf, n);
                    readAny = true;
                    bytesRead += n;

                    if (responseHeaders.contentLength > 0) {
                        remainingBytes -= n;
                        if (remainingBytes <= 0) {
                            log_i("Reached expected content length");
                            break;
                        }
                    }

                    // Yield every 8KB
                    if (bytesRead % 8192 == 0) {
                        yield();
                    }
                } else {
                    yield();
                }
            }
        }

        unsigned long elapsedTime = millis() - startTime;

        log_i("Total bytes downloaded: %u", bytesRead);
        log_i("Download time (ms): %lu", elapsedTime);

        client.stop();

        if (readAny) {
            // Ensure all data is flushed to disk before returning success
            log_i("Flushing file buffers to disk...");
            outFile->flush();
            delay(500);              // Small delay to ensure flush completes
            return error_type::None; // Success
        }

        // If no data was read, consider it a failure
        if (attempt < config->maxRetryAttempts - 1) {
            log_w("Download failed (no data), retrying...");
            if (!handle_rate_limit_response(attempt)) {
                break;
            }
        }
        attempt++;
    }

    return error_type::DownloadFailed;
}

void google_drive_client::set_access_token(const google_drive_access_token& token) {
    // use the copy constructor
    g_access_token = token;
}

const google_drive_access_token* google_drive_client::get_access_token_value() const {
    return &g_access_token;
}

bool google_drive_client::parse_http_response(WiFiClientSecure& client, HttpResponse& response) {
    // Read the status line first
    // keep reading while the line is empty, until we get a non-empty line
    String statusLine;
    do {
        statusLine = client.readStringUntil('\n');
    } while (statusLine.length() == 0);

    // Parse status code from "HTTP/1.1 200 OK" format
    int firstSpace  = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);

    if (firstSpace > 0 && secondSpace > firstSpace) {
        String statusCodeStr   = statusLine.substring(firstSpace + 1, secondSpace);
        response.statusCode    = statusCodeStr.toInt();
        response.statusMessage = statusLine.substring(secondSpace + 1);
        response.statusMessage.trim();
    } else {
        log_e("Failed to parse status line: %s", statusLine.c_str());
        return false;
    }

    // Parse HTTP response headers
    HttpResponseHeaders responseHeaders =
        parse_http_headers(client, false); // no verbose logging for token requests
    if (!responseHeaders.parseSuccessful) {
        log_e("Failed to parse HTTP headers");
        return false;
    }

    // Read response body based on encoding
    response.body = "";
    response.body.reserve(GOOGLE_DRIVE_BODY_RESERVE_SIZE); // Pre-allocate to prevent reallocations
    uint32_t bodyBytesRead = 0;

    if (responseHeaders.isChunked) {
        log_i("Response uses chunked transfer encoding");
        // Handle chunked transfer encoding with more robust parsing
        while (client.connected() || client.available()) {
            // Read chunk size line character by character to avoid issues
            String chunkSizeLine = "";
            char c;
            while (client.available()) {
                c = client.read();
                if (c == '\n') {
                    break;
                } else if (c != '\r') {
                    chunkSizeLine += c;
                }
            }

            if (chunkSizeLine.length() == 0) {
                yield();
                continue;
            }

            // Parse hex chunk size (ignore any chunk extensions after ';')
            int semicolonPos = chunkSizeLine.indexOf(';');
            if (semicolonPos != -1) {
                chunkSizeLine = chunkSizeLine.substring(0, semicolonPos);
            }
            chunkSizeLine.trim();

            // Convert hex string to integer
            long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);

#ifdef DEBUG_GOOGLE_DRIVE
            log_d("Chunk size: %ld (hex: %s)", chunkSize, chunkSizeLine.c_str());
#endif // DEBUG_GOOGLE_DRIVE

            // If chunk size is 0, we've reached the end
            if (chunkSize == 0) {
                // Read any trailing headers until we get an empty line
                while (client.connected() || client.available()) {
                    String trailerLine = "";
                    char c;
                    while (client.available()) {
                        c = client.read();
                        if (c == '\n') {
                            break;
                        } else if (c != '\r') {
                            trailerLine += c;
                        }
                    }
                    if (trailerLine.length() == 0) {
                        break;
                    }
                }
                break;
            }

            // Read exactly chunkSize bytes of chunk data
            long bytesRemaining = chunkSize;
            while (bytesRemaining > 0 && (client.connected() || client.available())) {
                // Read data byte by byte to ensure we don't read past chunk boundary
                if (client.available()) {
                    char byte = client.read();
                    response.body += byte;
                    bytesRemaining--;
                    bodyBytesRead++;

                    // Yield every 1KB to prevent watchdog timeout
                    if (bodyBytesRead % 1024 == 0) {
                        yield();
                    }
                } else {
                    yield();
                }
            }

            // Read and skip the trailing CRLF after chunk data
            while (client.available()) {
                char c = client.read();
                if (c == '\n') {
                    break;
                }
            }
        }
    } else {
        // Handle regular content (with or without Content-Length) using efficient block reading
        int bytesRemaining =
            responseHeaders.contentLength > 0 ? responseHeaders.contentLength : INT_MAX;
        while ((client.connected() || client.available()) && bytesRemaining > 0) {
            // Read data in blocks up to 1024 bytes or remaining content length
            size_t blockSize = min(1024, bytesRemaining);
            char buffer[1025]; // Extra byte for null terminator safety

            size_t bytesRead = client.readBytes(buffer, blockSize);
            if (bytesRead == 0) {
                yield(); // Yield when no data available
                continue;
            }

            // Null-terminate the buffer to ensure safe string operations
            buffer[bytesRead] = '\0';

            // Create a temporary String from the buffer and concatenate
            String chunk = String(buffer).substring(0, bytesRead);
            response.body += chunk;

            bodyBytesRead += bytesRead;

            // If we have Content-Length, track remaining bytes
            if (responseHeaders.contentLength > 0) {
                bytesRemaining -= bytesRead;
            }

            // Yield every 1KB to prevent watchdog timeout on large responses
            if (bodyBytesRead % 1024 == 0) {
                yield();
            }

            // For responses without Content-Length, break if no more data is immediately available
            if (responseHeaders.contentLength <= 0 && !client.available()) {
                break;
            }
        }
    }

    response.hasContent = response.body.length() > 0;

    log_i("HTTP Response: %d %s, Body length: %zu", response.statusCode, response.statusMessage.c_str(), response.body.length());

    return true;
}

failure_type google_drive_client::classify_failure(int statusCode, bool hasNetworkError) {
    // Network-level errors are always transient
    if (hasNetworkError) {
        return failure_type::Transient;
    }

    // No status code means connection failure - transient
    if (statusCode == 0) {
        return failure_type::Transient;
    }

    // Classify based on HTTP status codes
    if (statusCode == 429) {
        return failure_type::RateLimit;
    }

    // 4xx errors are generally permanent (client errors)
    if (statusCode >= 400 && statusCode < 500) {
        // Some 4xx errors might be temporary
        switch (statusCode) {
        case 408: // Request Timeout
        case 423: // Locked (temporary)
        case 424: // Failed Dependency (might be temporary)
            return failure_type::Transient;
        case 401: // Unauthorized - token refresh needed
            return failure_type::TokenExpired;
        default: return failure_type::Permanent;
        }
    }

    // 5xx errors are server errors - usually transient
    if (statusCode >= 500 && statusCode < 600) {
        return failure_type::Transient;
    }

    // 2xx and 3xx are generally success/redirect - shouldn't reach here
    if (statusCode >= 200 && statusCode < 400) {
        return failure_type::Unknown;
    }

    // Default to transient for unknown codes
    return failure_type::Transient;
}

bool google_drive_client::handle_transient_failure(uint8_t attempt, failure_type failureType) {
    if (attempt >= config->maxRetryAttempts) {
        log_w("Max retry attempts reached for transient failure");
        return false;
    }

    // Calculate base delay based on failure type
    unsigned long baseDelay;
    const char* failureTypeName;

    switch (failureType) {
    case failure_type::RateLimit:
        baseDelay       = config->backoffBaseDelayMs * 2; // Longer delay for rate limits
        failureTypeName = "rate limit";
        break;
    case failure_type::Transient:
        baseDelay       = config->backoffBaseDelayMs;
        failureTypeName = "transient error";
        break;
    default:
        baseDelay       = config->backoffBaseDelayMs;
        failureTypeName = "unknown error";
        break;
    }

    // Exponential backoff: base delay * 2^attempt
    unsigned long backoffDelay = baseDelay * (1UL << attempt);

    // Cap the delay to prevent excessive waiting (max 2 minutes)
    if (backoffDelay > 120000UL) {
        backoffDelay = 120000UL;
    }

    // Add jitter to prevent thundering herd
    backoffDelay = add_jitter(backoffDelay);

    log_w("Transient failure (%s). Backing off for %lums (attempt %u/%u)", failureTypeName, backoffDelay, attempt + 1, config->maxRetryAttempts);

    delay(backoffDelay);
    yield();

    return true;
}

unsigned long google_drive_client::add_jitter(unsigned long base_delay) {
    // Add up to 25% random jitter to prevent thundering herd
    unsigned long max_jitter = base_delay / 4;
    unsigned long jitter     = random(0, max_jitter + 1);
    return base_delay + jitter;
}

bool google_drive_client::is_token_expired(int marginSeconds) {
    return g_access_token.expired(marginSeconds);
}

photo_frame_error_t google_drive_client::refresh_token() {
    log_i("Refreshing access token...");

    // Clear the current token
    g_access_token.accessToken[0] = '\0';
    g_access_token.expiresAt      = 0;
    g_access_token.obtainedAt     = 0;

    // Get a new token
    return get_access_token();
}

String google_drive_client::build_http_request(const char* method,
                                               const char* path,
                                               const char* host,
                                               const char* headers,
                                               const char* body) {
    // Calculate required buffer size
    size_t reqLen = strlen(method) + strlen(path) + strlen(host) +
                    50; // Base size for HTTP/1.1, Host, Connection: close

    if (headers) {
        reqLen += strlen(headers);
    }

    if (body) {
        reqLen += strlen(body) + 50; // Extra space for Content-Length header
    }

    // Build request with pre-allocated buffer
    String req;
    req.reserve(reqLen);

    // Request line: METHOD path HTTP/1.0 (to avoid chunked transfer encoding)
    req = method;
    req += " ";
    req += path;
#ifdef USE_HTTP_1_0
    req += " HTTP/1.0\r\nHost: ";
#else
    req += " HTTP/1.1\r\nHost: ";
#endif // USE_HTTP_1_0
    req += host;
    req += "\r\n";

    // Add custom headers if provided
    if (headers) {
        req += headers;
        if (!String(headers).endsWith("\r\n")) {
            req += "\r\n";
        }
    }

    // Add body-related headers and body for POST requests
    if (body) {
        req += "Content-Length: ";
        req += String(strlen(body));
        req += "\r\n";
    }

    // 1lMQwII5ZbCT-Sus3efmA6AfyMf67-IS4

    // End headers and add body
    req += "Connection: close\r\n\r\n";

    if (body) {
        req += body;
    }

#ifdef DEBUG_GOOGLE_DRIVE
    log_d("Built HTTP request:\n%s\n", req.c_str());
#endif // DEBUG_GOOGLE_DRIVE

    return req;
}

void google_drive_client::set_root_ca_certificate(const String& rootCA) {
    g_google_root_ca = rootCA;
    log_i("Root CA certificate set for Google Drive client");
}

size_t google_drive_client::list_files_streaming(const char* folderId,
                                                 sd_card& sdCard,
                                                 const char* tocFilePath,
                                                 int pageSize) {
    log_i("list_files_streaming with folderId=%s, tocFilePath=%s, pageSize=%d", folderId, tocFilePath, pageSize);

    char nextPageToken[GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE] = "";
    int pageNumber                                          = 1;
    size_t totalFilesWritten                                = 0;

    do {
        log_i("Fetching page %d with pageSize=%d, current total files=%zu", pageNumber, pageSize, totalFilesWritten);

        size_t filesThisPage = list_files_in_folder_streaming(
            folderId, sdCard, tocFilePath, pageSize, nextPageToken, nextPageToken);
        if (filesThisPage == 0) {
            log_e("Error fetching page %d - no files returned", pageNumber);
            return totalFilesWritten; // Return what we have written so far
        }

        totalFilesWritten += filesThisPage;

#ifdef DEBUG_GOOGLE_DRIVE
        log_d("Page %d complete: got %zu files, total now %zu, nextPageToken size='%zu'", pageNumber, filesThisPage, totalFilesWritten, strlen(nextPageToken));
#endif // DEBUG_GOOGLE_DRIVE

        pageNumber++;

        // Safety check to prevent infinite loops
        if (pageNumber > 50) {
            log_w("Warning: Reached maximum page limit (50)");
            break;
        }
    } while (strlen(nextPageToken) > 0);

    log_i("Pagination complete: streamed %zu total files to TOC", totalFilesWritten);

    return totalFilesWritten;
}

size_t google_drive_client::list_files_in_folder_streaming(const char* folderId,
                                                           sd_card& sdCard,
                                                           const char* tocFilePath,
                                                           int pageSize,
                                                           char* nextPageToken,
                                                           const char* pageToken) {
    log_i("list_files_in_folder_streaming folderId=%s, tocFilePath=%s, pageToken=%s", folderId, tocFilePath, pageToken);

    // Check if token is expired or not acquired yet and refresh/acquire if needed
    if (is_token_expired() || g_access_token.accessToken[0] == '\0') {
        log_i("Token expired or not acquired, acquiring before file list request");
        photo_frame_error_t tokenError = refresh_token();
        if (tokenError != error_type::None) {
            log_e("Failed to acquire access token for file list request");
            return 0;
        }
    }

    // Check rate limiting
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        return 0;
    }

    // Build the API request URL
    String q = "'";
    q += folderId;
    q += "' in parents";
    String encodedQ = urlEncode(q);

    String url      = "https://www.googleapis.com";
    url += DRIVE_LIST_PATH;
    url += "?q=";
    url += encodedQ;
    url += "&fields=nextPageToken,files(id,name)&pageSize=";
    url += String(pageSize);
    if (strlen(pageToken) > 0) {
        String encodedToken = urlEncode(String(pageToken));
        url += "&pageToken=";
        url += encodedToken;
    }

    // Use HTTPClient for automatic chunked transfer encoding handling
    HTTPClient http;
    WiFiClientSecure client; // Stack allocation to avoid memory management issues

    if (config->useInsecureTls) {
        client.setInsecure();
        log_w("WARNING: Using insecure TLS connection");
    } else if (g_google_root_ca.length() > 0) {
        client.setCACert(g_google_root_ca.c_str());
    } else {
        client.setInsecure();
    }

    http.begin(client, url);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);

    // Set authorization header
    String authHeader = "Bearer ";
    authHeader += g_access_token.accessToken;
    http.addHeader("Authorization", authHeader);
    http.addHeader("User-Agent", "ESP32-PhotoFrame");

    record_request();

    log_i("Requesting URL: %s", url.c_str());

    int httpCode = http.GET();

    if (httpCode != 200) {
        log_e("HTTP Error: %d", httpCode);
        http.end();
        return 0;
    }

    String response;

    // Get content length to allocate proper buffer size
    int contentLength  = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    if (contentLength > 0 && contentLength < GOOGLE_DRIVE_SAFETY_LIMIT) {
        // Allocate buffer in PSRAM
        char* buffer = (char*)heap_caps_malloc(contentLength + 1, MALLOC_CAP_SPIRAM);
        if (!buffer) {
            // Fallback to regular heap if PSRAM allocation fails
            buffer = (char*)malloc(contentLength + 1);
        }

        if (buffer) {
            // Read response in chunks
            int totalRead = 0;
            while (stream->available() && totalRead < contentLength) {
                int bytesToRead =
                    min(4096, contentLength - totalRead); // Larger chunks for file lists
                int bytesRead = stream->readBytes(buffer + totalRead, bytesToRead);
                if (bytesRead == 0)
                    break;
                totalRead += bytesRead;
                yield(); // Prevent watchdog timeout
            }
            buffer[totalRead] = '\0';
            response          = String(buffer);
            free(buffer);

#ifdef BOARD_HAS_PSRAM
            log_i("Read %d bytes from file list response (allocated in PSRAM)", totalRead);
#else
            log_i("Read %d bytes from file list response (allocated in heap)", totalRead);
#endif
        } else {
            log_e("Failed to allocate file list buffer, using getString");
            response = http.getString();
        }
    } else {
        // Fallback to getString for unknown/large content
        log_i("Using fallback getString for file list, content length: %d", contentLength);
        response = http.getString();
    }

    http.end();

    log_i("Response body length: %zu", response.length());

    // Parse the JSON response and write directly to TOC file
    return parse_file_list_to_toc(response, sdCard, tocFilePath, nextPageToken);
}

size_t google_drive_client::parse_file_list_to_toc(const String& jsonBody,
                                                   sd_card& sdCard,
                                                   const char* tocFilePath,
                                                   char* nextPageToken) {
    size_t filesWritten = 0;

    // Clear the next page token
    if (nextPageToken) {
        nextPageToken[0] = '\0';
    }

    // Parse the JSON using ArduinoJson with static allocation
    StaticJsonDocument<GOOGLE_DRIVE_JSON_DOC_SIZE> doc;
    DeserializationError error = deserializeJson(doc, jsonBody);
    if (error) {
        log_e("JSON deserialization failed: %s", error.c_str());
        log_e("Failed JSON:");
        // print line by line to avoid large output
        int lineStart = 0;
        int lineEnd   = jsonBody.indexOf('\n');
        while (lineEnd != -1) {
            log_e("%s", jsonBody.substring(lineStart, lineEnd).c_str());
            lineStart = lineEnd + 1;
            lineEnd   = jsonBody.indexOf('\n', lineStart);
        }
        log_e("%s", jsonBody.substring(lineStart).c_str()); // print last line
        return 0;
    }

    // Extract nextPageToken if present
    if (nextPageToken && doc.containsKey("nextPageToken")) {
        String token = doc["nextPageToken"].as<String>();
        if (token.length() < GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE) {
            strcpy(nextPageToken, token.c_str());
        }
    }

    // Process files array
    if (doc.containsKey("files")) {
        JsonArray files = doc["files"];
        log_i("Found files array with %zu entries", files.size());

        // Open TOC file for appending using sdCard instance
        fs::File tocFile = sdCard.open(tocFilePath, FILE_APPEND, true);
        if (!tocFile) {
            log_e("ERROR: Failed to open TOC file for writing: %s", tocFilePath);
            return 0;
        }

        // Set buffer size for better performance
        tocFile.setBufferSize(8192);

        for (JsonVariant f : files) {
            if (!f.containsKey("id") || !f.containsKey("name")) {
                continue;
            }

            const char* id   = f["id"];
            const char* name = f["name"];

            // Check file extension filter - accept both .bin and .bmp
            const char* fileExtension = strrchr(name, '.');
            bool extensionAllowed     = false;
            if (fileExtension) {
                for (size_t i = 0; i < ALLOWED_EXTENSIONS_COUNT; i++) {
                    if (strcmp(fileExtension, ALLOWED_FILE_EXTENSIONS[i]) == 0) {
                        extensionAllowed = true;
                        break;
                    }
                }
            }
            if (!extensionAllowed) {
                log_i("Skipping file with unsupported extension: %s", name);
                continue;
            }

#ifdef DEBUG_GOOGLE_DRIVE
            log_d("File ID: %zu, name: %s", filesWritten, name);
#endif // DEBUG_GOOGLE_DRIVE

            // Write directly to TOC file: id|name using efficient write() operations
            size_t bytesWritten = 0;

            // Write ID
            bytesWritten += tocFile.write((const uint8_t*)id, strlen(id));

            // Write separator
            bytesWritten += tocFile.write((const uint8_t*)"|", 1);

            // Write name
            bytesWritten += tocFile.write((const uint8_t*)name, strlen(name));

            // Write newline
            bytesWritten += tocFile.write((const uint8_t*)"\n", 1);

            // Debug: Verify write operations succeeded
            if (bytesWritten == 0) {
                log_e("ERROR: Failed to write to TOC file!");
                tocFile.close();
                return filesWritten;
            }

#ifdef DEBUG_GOOGLE_DRIVE
            log_d("Wrote %zu bytes to TOC file", bytesWritten);
#endif // DEBUG_GOOGLE_DRIVE

            filesWritten++;

            // Flush and yield every 50 files to prevent SD card timeout
            if (filesWritten % 50 == 0) {
                tocFile.flush();
                delay(10); // Give SD card time to complete write operations
                yield();
            }
        }

        // Final flush before close to ensure all data is written
        log_i("Flushing final data to TOC file...");
        tocFile.flush();
        delay(50); // Give SD card time to complete all pending writes

        tocFile.close();

        // Add delay after close to ensure SD card completes file system operations
        delay(50);

        log_i("Final files written: %zu", filesWritten);
    } else {
        log_e("ERROR: No 'files' array found in JSON response");

#ifdef DEBUG_GOOGLE_DRIVE
        log_d("Available JSON keys:");
        for (JsonPair p : doc.as<JsonObject>()) {
            log_d("  - Key: %s", p.key().c_str());
        }
#endif // DEBUG_GOOGLE_DRIVE
    }

    return filesWritten;
}

HttpResponseHeaders google_drive_client::parse_http_headers(WiFiClientSecure& client,
                                                            bool verbose) {
    HttpResponseHeaders headers;

    while (true) {
        String line = client.readStringUntil('\n');
        line.trim();

        if (verbose) {
            log_i("Header[%u]: %s", headers.headerCount, line.c_str());
        }

        // Check for end of headers
        if (line.length() == 0 || line == "\r") {
            if (verbose) {
                log_i("End of headers detected");
            }
            headers.parseSuccessful = true;
            break;
        }

        // Check for connection loss
        if (!client.connected() && !client.available()) {
            if (verbose) {
                log_w("Connection lost while reading headers");
            }
            break;
        }

        // Parse Transfer-Encoding header (case-insensitive)
        if (line.startsWith("Transfer-Encoding:") || line.startsWith("transfer-encoding:")) {
            if (line.indexOf("chunked") >= 0) {
                headers.isChunked = true;
                if (verbose) {
                    log_i("Detected chunked transfer encoding");
                }
            }
        }
        // Parse Content-Length header (case-insensitive)
        else if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
            int colonPos = line.indexOf(':');
            if (colonPos >= 0) {
                String lengthStr = line.substring(colonPos + 1);
                lengthStr.trim();
                headers.contentLength = lengthStr.toInt();
                if (verbose) {
                    log_i("Content-Length: %d", headers.contentLength);
                }
            }
        }

        headers.headerCount++;

        // Yield every 10 headers to prevent watchdog timeout
        if (headers.headerCount % 10 == 0) {
            yield();
        }
    }

    return headers;
}

} // namespace photo_frame
