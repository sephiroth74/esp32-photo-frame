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
#include <SD_MMC.h>
#include <unistd.h> // For fsync()
#ifdef BOARD_HAS_PSRAM
#include <esp_heap_caps.h> // For ps_malloc/ps_free
#endif

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
            Serial.println(F("[google_drive_client] Response too large, truncating"));
            break;
        }
    }

    Serial.print(F("[google_drive_client] Body read complete, length: "));
    Serial.println(body.length());
    Serial.print(F("[google_drive_client] Total bytes read: "));
    Serial.println(totalRead);

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
    Serial.println(F("[google_drive_client] Google Drive rate limiting initialized"));
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
        Serial.println(F("[google_drive_client] Minimum request delay not met"));
        return false;
    }

    // Check if we're within the rate limit window
    if (requestCount >= GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
        Serial.print(F("[google_drive_client] Rate limit reached: "));
        Serial.print(requestCount);
        Serial.print(F("/"));
        Serial.print(GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
        Serial.println(F(" requests in window"));
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
            Serial.print(F("[google_drive_client] Rate limit wait timeout after "));
            Serial.print(elapsedTime);
            Serial.println(F("ms - aborting to conserve battery"));
            return error_type::RateLimitTimeoutExceeded;
        }

        Serial.println(F("[google_drive_client] Waiting for rate limit compliance..."));

        unsigned long currentTime     = millis();
        unsigned long nextAllowedTime = lastRequestTime + config->minRequestDelayMs;

        if (currentTime < nextAllowedTime) {
            unsigned long waitTime = nextAllowedTime - currentTime;

            // Ensure the wait time doesn't exceed our remaining budget
            unsigned long remainingBudget = config->maxWaitTimeMs - elapsedTime;
            if (waitTime > remainingBudget) {
                Serial.print(F("[google_drive_client] Wait time ("));
                Serial.print(waitTime);
                Serial.print(F("ms) would exceed budget, aborting"));
                return error_type::RateLimitTimeoutExceeded;
            }

            Serial.print(F("[google_drive_client] Delaying "));
            Serial.print(waitTime);
            Serial.println(F("ms for rate limit"));
            delay(waitTime);
        } else {
            // Wait a bit more for the sliding window to clear
            unsigned long remainingBudget = config->maxWaitTimeMs - elapsedTime;
            if (remainingBudget < 1000) {
                Serial.println(
                    F("[google_drive_client] Insufficient time budget remaining, aborting"));
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
    Serial.print(F("[google_drive_client] Request recorded ("));
    Serial.print(requestCount);
    Serial.print(F("/"));
    Serial.print(GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
    Serial.println(F(")"));
#endif // DEBUG_GOOGLE_DRIVE
}

bool google_drive_client::handle_rate_limit_response(uint8_t attempt) {
    if (attempt >= config->maxRetryAttempts) {
        Serial.println(F("[google_drive_client] Max retry attempts reached for rate limit"));
        return false;
    }

    // Exponential backoff: base delay * 2^attempt
    unsigned long backoffDelay = config->backoffBaseDelayMs * (1UL << attempt);

    // Cap the delay to prevent excessive waiting (max 2 minutes)
    if (backoffDelay > GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS) {
        backoffDelay = GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS;
    }

    Serial.print(F("[google_drive_client] Rate limited (HTTP 429). Backing off for "));
    Serial.print(backoffDelay);
    Serial.print(F("ms (attempt "));
    Serial.print(attempt + 1);
    Serial.print(F("/"));
    Serial.print(config->maxRetryAttempts);
    Serial.println(F(")"));

    delay(backoffDelay);
    yield();

    return true;
}

bool google_drive_client::rsaSignRS256(const String& input, String& sig_b64url) {
    uint8_t hash[32];
    mbedtls_md_context_t mdctx;
    mbedtls_md_init(&mdctx);
    const mbedtls_md_info_t* mdinfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!mdinfo)
        return false;
    if (mbedtls_md_setup(&mdctx, mdinfo, 0) != 0)
        return false;
    mbedtls_md_starts(&mdctx);
    mbedtls_md_update(&mdctx, (const unsigned char*)input.c_str(), input.length());
    mbedtls_md_finish(&mdctx, hash);
    mbedtls_md_free(&mdctx);
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
        mbedtls_pk_free(&pk);
        return false;
    }
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
    const uint32_t iat = (uint32_t)now;
    const uint32_t exp = iat + 3600;
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
    Serial.println(F("[google_drive_client] Requesting new access token..."));

    String jwt = create_jwt();
    if (jwt.isEmpty())
        return error_type::JwtCreationFailed;

#ifdef DEBUG_GOOGLE_DRIVE
    Serial.print(F("[google_drive_client] JWT created, length: "));
    Serial.println(jwt.length());
#endif // DEBUG_GOOGLE_DRIVE

    String body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" +
                  urlEncode(jwt);

    // Apply rate limiting before making the request
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        Serial.println(F("[google_drive_client] Rate limit wait timeout - aborting token request"));
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
            Serial.println(F("[google_drive_client] WARNING: Using insecure TLS connection"));
            client.setInsecure();
        } else {
            if (g_google_root_ca.length() > 0) {
                client.setCACert(g_google_root_ca.c_str());
            } else {
                Serial.println(F("[google_drive_client] WARNING: No root CA certificate loaded, "
                                 "using insecure connection"));
                client.setInsecure();
            }
        }

        Serial.print(F("[google_drive_client] Connecting to token endpoint: "));
        Serial.println(TOKEN_HOST);

        String tokenUrl = "https://";
        tokenUrl += TOKEN_HOST;
        tokenUrl += TOKEN_PATH;

        http.begin(client, tokenUrl);
        http.setTimeout(HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("User-Agent", "ESP32-PhotoFrame");

        int httpCode = http.POST(body);

        if (httpCode > 0) {
            statusCode = httpCode;

            // Get content length to allocate proper buffer size
            int contentLength  = http.getSize();
            WiFiClient* stream = http.getStreamPtr();

            if (contentLength > 0 && contentLength < GOOGLE_DRIVE_SAFETY_LIMIT) {
                // Allocate buffer in PSRAM if available, otherwise regular heap
                char* buffer = nullptr;
#ifdef BOARD_HAS_PSRAM
                buffer = (char*)heap_caps_malloc(contentLength + 1, MALLOC_CAP_SPIRAM);
                if (!buffer) {
                    // Fallback to regular heap if PSRAM allocation fails
                    buffer = (char*)malloc(contentLength + 1);
                }
#else
                buffer = (char*)malloc(contentLength + 1);
#endif

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

                    Serial.print(F("[google_drive_client] Read "));
                    Serial.print(totalRead);
                    Serial.println(F(" bytes from token response"));
                } else {
                    Serial.println(F("[google_drive_client] Failed to allocate response buffer"));
                    networkError = true;
                }
            } else {
                // Fallback to getString for unknown/large content
                Serial.print(F("[google_drive_client] Using fallback getString, content length: "));
                Serial.println(contentLength);
                responseBody = http.getString();
            }
        } else {
            Serial.print(F("[google_drive_client] HTTP POST failed: "));
            Serial.println(httpCode);
            networkError = true;
        }

        http.end();

        // Classify failure and decide whether to retry
        failure_type failureType = classify_failure(statusCode, networkError);

        if (failureType == failure_type::Permanent) {
            Serial.print(F("[google_drive_client] Permanent failure (HTTP "));
            Serial.print(statusCode);
            Serial.println(F("), not retrying"));
            // Return specific error based on failure type and status code
            return photo_frame::error_utils::map_google_drive_error(
                statusCode, responseBody.c_str(), "OAuth token request");
        }

        // Success case
        if (statusCode == 200 && responseBody.length() > 0) {
            // Parse token from response body
            size_t freeHeap = ESP.getFreeHeap();
            Serial.print(F("[google_drive_client] Free heap before token parse: "));
            Serial.println(freeHeap);

            StaticJsonDocument<768> doc;
            DeserializationError err = deserializeJson(doc, responseBody);
            if (err) {
                Serial.print(F("[google_drive_client] Token parse error: "));
                Serial.println(err.c_str());
#ifdef DEBUG_GOOGLE_DRIVE
                Serial.print(F("[google_drive_client] Response body: "));
                Serial.println(responseBody);
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
                Serial.println(F("[google_drive_client] No access_token in response"));
                return error_type::OAuthTokenRequestFailed;
            }

            time_t tokenExpirationTime = time(NULL) + expires_in;

            safe_strcpy(g_access_token.accessToken, tok, sizeof(g_access_token.accessToken));
            g_access_token.expiresAt  = tokenExpirationTime;
            g_access_token.obtainedAt = time(NULL);

            Serial.print(F("[google_drive_client] Token: "));
            Serial.println(g_access_token.accessToken);
            Serial.print(F("[google_drive_client] Token expires at: "));
            Serial.println(tokenExpirationTime);
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
            Serial.println(F("[google_drive_client] Max retry attempts reached for access token"));
            break;
        }
    }

    return error_type::HttpPostFailed;
}

photo_frame_error_t google_drive_client::download_file(const String& fileId, fs::File* outFile) {
    // Check if token is expired and refresh if needed
    if (is_token_expired()) {
        Serial.println(F("[google_drive_client] Token expired, refreshing before download"));
        photo_frame_error_t refreshError = refresh_token();
        if (refreshError != error_type::None) {
            Serial.println(F("[google_drive_client] Failed to refresh expired token"));
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
        Serial.println(
            F("[google_drive_client] Rate limit wait timeout - aborting download request"));
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
                Serial.println(F("[google_drive_client] WARNING: No root CA certificate loaded, "
                                 "connection may fail"));
                client.setInsecure(); // Fallback to insecure connection
            }
        }
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000);          // Set socket timeout in seconds
        client.setHandshakeTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set handshake timeout in seconds

        unsigned long connectStart = millis();
        if (!client.connect(DRIVE_HOST, 443)) {
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("[google_drive_client] Download connection failed, retrying..."));
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpConnectFailed;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("[google_drive_client] Download connection timeout"));
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("[google_drive_client] Retrying after timeout..."));
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
        Serial.print(F("[google_drive_client] HTTP Request: "));
        Serial.println(req);
#endif // DEBUG_GOOGLE_DRIVE

        client.print(req);

        // Read and validate HTTP status line first
        String statusLine;
        do {
            statusLine = client.readStringUntil('\n');
        } while (statusLine.length() == 0);

        statusLine.trim();
        Serial.print(F("[google_drive_client] HTTP Status: "));
        Serial.println(statusLine);

        // Check if status is 200 OK
        if (statusLine.indexOf("200") < 0) {
            Serial.println(F("[google_drive_client] HTTP error - not 200 OK"));
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("[google_drive_client] Retrying after HTTP error..."));
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
            Serial.println(F("[google_drive_client] Failed to parse HTTP headers"));
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("[google_drive_client] Retrying after header parse failure..."));
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
            Serial.println(F("[google_drive_client] Reading chunked response"));
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
                    Serial.println(F("[google_drive_client] End of chunked data"));
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
                Serial.println(F("[google_drive_client] Flushing chunked file buffers to disk..."));
                outFile->flush();
                delay(500); // Small delay to ensure flush completes
            }
        } else {
            // Handle regular (non-chunked) response
            Serial.println(F("[google_drive_client] Reading non-chunked response"));
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
                            Serial.println(
                                F("[google_drive_client] Reached expected content length"));
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

        Serial.print(F("[google_drive_client] Total bytes downloaded: "));
        Serial.println(bytesRead);

        Serial.print(F("[google_drive_client] Download time (ms): "));
        Serial.println(elapsedTime);

        client.stop();

        if (readAny) {
            // Ensure all data is flushed to disk before returning success
            Serial.println(F("[google_drive_client] Flushing file buffers to disk..."));
            outFile->flush();
            delay(500);              // Small delay to ensure flush completes
            return error_type::None; // Success
        }

        // If no data was read, consider it a failure
        if (attempt < config->maxRetryAttempts - 1) {
            Serial.println(F("Download failed (no data), retrying..."));
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
        Serial.print(F("[google_drive_client] Failed to parse status line: "));
        Serial.println(statusLine);
        return false;
    }

    // Parse HTTP response headers
    HttpResponseHeaders responseHeaders =
        parse_http_headers(client, false); // no verbose logging for token requests
    if (!responseHeaders.parseSuccessful) {
        Serial.println(F("[google_drive_client] Failed to parse HTTP headers"));
        return false;
    }

    // Read response body based on encoding
    response.body = "";
    response.body.reserve(GOOGLE_DRIVE_BODY_RESERVE_SIZE); // Pre-allocate to prevent reallocations
    uint32_t bodyBytesRead = 0;

    if (responseHeaders.isChunked) {
        Serial.println(F("[google_drive_client] Response uses chunked transfer encoding"));
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
            Serial.print(F("[google_drive_client] Chunk size: "));
            Serial.print(chunkSize);
            Serial.print(F(" (hex: "));
            Serial.print(chunkSizeLine);
            Serial.println(F(")"));
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

    Serial.print(F("[google_drive_client] HTTP Response: "));
    Serial.print(response.statusCode);
    Serial.print(F(" "));
    Serial.print(response.statusMessage);
    Serial.print(F(", Body length: "));
    Serial.println(response.body.length());

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
        Serial.println(F("[google_drive_client] Max retry attempts reached for transient failure"));
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

    Serial.print(F("[google_drive_client] Transient failure ("));
    Serial.print(failureTypeName);
    Serial.print(F("). Backing off for "));
    Serial.print(backoffDelay);
    Serial.print(F("ms (attempt "));
    Serial.print(attempt + 1);
    Serial.print(F("/"));
    Serial.print(config->maxRetryAttempts);
    Serial.println(F(")"));

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
    Serial.println(F("[google_drive_client] Refreshing access token..."));

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
    Serial.print(F("[google_drive_client] Built HTTP request:\n"));
    Serial.print(req);
    Serial.print(F("\n"));
#endif // DEBUG_GOOGLE_DRIVE

    return req;
}

void google_drive_client::set_root_ca_certificate(const String& rootCA) {
    g_google_root_ca = rootCA;
    Serial.println(F("[google_drive_client] Root CA certificate set for Google Drive client"));
}

size_t google_drive_client::list_files_streaming(const char* folderId,
                                                 sd_card& sdCard,
                                                 const char* tocFilePath,
                                                 int pageSize) {
    Serial.print(F("[google_drive_client] list_files_streaming with folderId="));
    Serial.print(folderId);
    Serial.print(F(", tocFilePath="));
    Serial.print(tocFilePath);
    Serial.print(F(", pageSize="));
    Serial.println(pageSize);

    char nextPageToken[GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE] = "";
    int pageNumber                                          = 1;
    size_t totalFilesWritten                                = 0;

    do {
        Serial.print(F("[google_drive_client] Fetching page "));
        Serial.print(pageNumber);
        Serial.print(F(" with pageSize="));
        Serial.print(pageSize);
        Serial.print(F(", current total files="));
        Serial.println(totalFilesWritten);

        size_t filesThisPage = list_files_in_folder_streaming(
            folderId, sdCard, tocFilePath, pageSize, nextPageToken, nextPageToken);
        if (filesThisPage == 0) {
            Serial.print(F("[google_drive_client] Error fetching page "));
            Serial.print(pageNumber);
            Serial.println(F(" - no files returned"));
            return totalFilesWritten; // Return what we have written so far
        }

        totalFilesWritten += filesThisPage;

#ifdef DEBUG_GOOGLE_DRIVE
        Serial.print(F("[google_drive_client] Page "));
        Serial.print(pageNumber);
        Serial.print(F(" complete: got "));
        Serial.print(filesThisPage);
        Serial.print(F(" files, total now "));
        Serial.print(totalFilesWritten);
        Serial.print(F(", nextPageToken size='"));
        Serial.print(strlen(nextPageToken));
        Serial.println(F("'"));
#endif // DEBUG_GOOGLE_DRIVE

        pageNumber++;

        // Safety check to prevent infinite loops
        if (pageNumber > 50) {
            Serial.println(F("[google_drive_client] Warning: Reached maximum page limit (50)"));
            break;
        }
    } while (strlen(nextPageToken) > 0);

    Serial.print(F("[google_drive_client] Pagination complete: streamed "));
    Serial.print(totalFilesWritten);
    Serial.println(F(" total files to TOC"));

    return totalFilesWritten;
}

size_t google_drive_client::list_files_in_folder_streaming(const char* folderId,
                                                           sd_card& sdCard,
                                                           const char* tocFilePath,
                                                           int pageSize,
                                                           char* nextPageToken,
                                                           const char* pageToken) {
    Serial.print(F("[google_drive_client] list_files_in_folder_streaming folderId="));
    Serial.print(folderId);
    Serial.print(F(", tocFilePath="));
    Serial.print(tocFilePath);
    Serial.print(F(", pageToken="));
    Serial.println(pageToken);

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
        Serial.println(F("[google_drive_client] WARNING: Using insecure TLS connection"));
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

    Serial.print(F("[google_drive_client] Requesting URL: "));
    Serial.println(url);

    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.print(F("[google_drive_client] HTTP Error: "));
        Serial.println(httpCode);
        http.end();
        return 0;
    }

    String response;

    // Get content length to allocate proper buffer size
    int contentLength  = http.getSize();
    WiFiClient* stream = http.getStreamPtr();

    if (contentLength > 0 && contentLength < GOOGLE_DRIVE_SAFETY_LIMIT) {
        // Allocate buffer in PSRAM if available, otherwise regular heap
        char* buffer = nullptr;
#ifdef BOARD_HAS_PSRAM
        buffer = (char*)heap_caps_malloc(contentLength + 1, MALLOC_CAP_SPIRAM);
        if (!buffer) {
            // Fallback to regular heap if PSRAM allocation fails
            buffer = (char*)malloc(contentLength + 1);
        }
#else
        buffer = (char*)malloc(contentLength + 1);
#endif

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

            Serial.print(F("[google_drive_client] Read "));
            Serial.print(totalRead);
            Serial.print(F(" bytes from file list response (allocated in "));
#ifdef BOARD_HAS_PSRAM
            Serial.println(F("PSRAM)"));
#else
            Serial.println(F("heap)"));
#endif
        } else {
            Serial.println(
                F("[google_drive_client] Failed to allocate file list buffer, using getString"));
            response = http.getString();
        }
    } else {
        // Fallback to getString for unknown/large content
        Serial.print(
            F("[google_drive_client] Using fallback getString for file list, content length: "));
        Serial.println(contentLength);
        response = http.getString();
    }

    http.end();

    Serial.print(F("[google_drive_client] Response body length: "));
    Serial.println(response.length());

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
        Serial.print(F("[google_drive_client] JSON deserialization failed: "));
        Serial.println(error.c_str());
        Serial.print(F("[google_drive_client] Failed JSON: "));
        // print line by line to avoid large output
        int lineStart = 0;
        int lineEnd   = jsonBody.indexOf('\n');
        while (lineEnd != -1) {
            Serial.println(jsonBody.substring(lineStart, lineEnd));
            lineStart = lineEnd + 1;
            lineEnd   = jsonBody.indexOf('\n', lineStart);
        }
        Serial.println(jsonBody.substring(lineStart)); // print last line
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
        Serial.print(F("[google_drive_client] Found files array with "));
        Serial.print(files.size());
        Serial.println(F(" entries"));

        // Open TOC file for appending using sdCard instance
        fs::File tocFile = sdCard.open(tocFilePath, FILE_APPEND, true);
        if (!tocFile) {
            Serial.print(F("[google_drive_client] ERROR: Failed to open TOC file for writing: "));
            Serial.println(tocFilePath);
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
                Serial.print(F("[google_drive_client] Skipping file with unsupported extension: "));
                Serial.println(name);
                continue;
            }

#ifdef DEBUG_GOOGLE_DRIVE
            Serial.print(F("[google_drive_client] File ID: "));
            Serial.print(filesWritten);
            Serial.print(F(", name: "));
            Serial.println(name);
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

            tocFile.flush();

            // Debug: Verify write operations succeeded
            if (bytesWritten == 0) {
                Serial.println(F("[google_drive_client] ERROR: Failed to write to TOC file!"));
                tocFile.close();
                return filesWritten;
            }

#ifdef DEBUG_GOOGLE_DRIVE
            Serial.print(F("[google_drive_client] Wrote "));
            Serial.print(bytesWritten);
            Serial.println(F(" bytes to TOC file"));
#endif // DEBUG_GOOGLE_DRIVE

            filesWritten++;

            // Force flush every 50 files to ensure data persists to disk
            if (filesWritten % 50 == 0) {
                yield();
            }
        }

        // Final flush and close
        tocFile.close();

        Serial.print(F("[google_drive_client] Final files written: "));
        Serial.println(filesWritten);
    } else {
        Serial.println(F("[google_drive_client] ERROR: No 'files' array found in JSON response"));

#ifdef DEBUG_GOOGLE_DRIVE
        Serial.println(F("[google_drive_client] Available JSON keys:"));
        for (JsonPair p : doc.as<JsonObject>()) {
            Serial.print(F("  - Key: "));
            Serial.println(p.key().c_str());
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
            Serial.print(F("[google_drive_client] Header["));
            Serial.print(headers.headerCount);
            Serial.print(F("]: "));
            Serial.println(line);
        }

        // Check for end of headers
        if (line.length() == 0 || line == "\r") {
            if (verbose) {
                Serial.println(F("[google_drive_client] End of headers detected"));
            }
            headers.parseSuccessful = true;
            break;
        }

        // Check for connection loss
        if (!client.connected() && !client.available()) {
            if (verbose) {
                Serial.println(F("[google_drive_client] Connection lost while reading headers"));
            }
            break;
        }

        // Parse Transfer-Encoding header (case-insensitive)
        if (line.startsWith("Transfer-Encoding:") || line.startsWith("transfer-encoding:")) {
            if (line.indexOf("chunked") >= 0) {
                headers.isChunked = true;
                if (verbose) {
                    Serial.println(F("[google_drive_client] Detected chunked transfer encoding"));
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
                    Serial.print(F("[google_drive_client] Content-Length: "));
                    Serial.println(headers.contentLength);
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
