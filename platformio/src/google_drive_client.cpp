#include "google_drive_client.h"

// OAuth/Google endpoints
static const char* TOKEN_HOST = "oauth2.googleapis.com";
static const char* TOKEN_PATH = "/token";
static const char* DRIVE_HOST = "www.googleapis.com";
static const char* DRIVE_LIST_PATH = "/drive/v3/files";
static const char* SCOPE = "https://www.googleapis.com/auth/drive.readonly";
static const char* AUD = "https://oauth2.googleapis.com/token";

// Global variable to store loaded root CA certificate
String g_google_root_ca;

String readHttpBody(WiFiClientSecure& client)
{
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
            Serial.println(F("Response too large, truncating"));
            break;
        }
    }
    
    Serial.print(F("Body read complete, length: "));
    Serial.println(body.length());
    Serial.print(F("Total bytes read: "));
    Serial.println(totalRead);

    return body;
}

// Helper functions
// use approved answer here: https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
static String urlEncode(const String& s)
{
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

#if DEBUG_MODE
    Serial.print(F("Encoded URL: "));
    Serial.println(o);
#endif // DEBUG_MODE

    return o;
}

static String base64url(const uint8_t* data, size_t len)
{
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

static String base64url(const String& s)
{
    return base64url((const uint8_t*)s.c_str(), s.length());
}

// Helper function to safely copy string to char buffer
static void safe_strcpy(char* dest, const char* src, size_t dest_size)
{
    if (dest && src && dest_size > 0) {
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else if (dest && dest_size > 0) {
        dest[0] = '\0';
    }
}

namespace photo_frame {

google_drive_client::google_drive_client(const google_drive_client_config& config)
    : config(&config)
    , g_access_token { .accessToken = { 0 }, .expiresAt = 0, .obtainedAt = 0 }
    , lastRequestTime(0)
    , requestHistoryIndex(0)
    , requestCount(0)
{
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    const char* pers = "esp32jwt";
    mbedtls_ctr_drbg_seed(
        &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));

    // Initialize request history array
    for (uint8_t i = 0; i < GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW; i++) {
        requestHistory[i] = 0;
    }

    Serial.println(F("Google Drive rate limiting initialized"));
}

google_drive_client::~google_drive_client()
{
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void google_drive_client::clean_old_requests()
{
    unsigned long currentTime = millis();
    unsigned long windowStartTime = currentTime - (config->rateLimitWindowSeconds * 1000UL);

    uint8_t validRequests = 0;
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
        requestCount = validRequests;
        requestHistoryIndex = requestCount;
    }
}

bool google_drive_client::can_make_request()
{
    clean_old_requests();

    unsigned long currentTime = millis();

    // Check minimum delay between requests
    if (lastRequestTime > 0 && (currentTime - lastRequestTime) < config->minRequestDelayMs) {
        Serial.println(F("Minimum request delay not met"));
        return false;
    }

    // Check if we're within the rate limit window
    if (requestCount >= GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
        Serial.print(F("Rate limit reached: "));
        Serial.print(requestCount);
        Serial.print(F("/"));
        Serial.print(GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
        Serial.println(F(" requests in window"));
        return false;
    }

    return true;
}

photo_frame_error_t google_drive_client::wait_for_rate_limit()
{
    unsigned long startTime = millis();

    while (!can_make_request()) {
        // Check if we've exceeded the maximum wait time
        unsigned long elapsedTime = millis() - startTime;
        if (elapsedTime >= config->maxWaitTimeMs) {
            Serial.print(F("Rate limit wait timeout after "));
            Serial.print(elapsedTime);
            Serial.println(F("ms - aborting to conserve battery"));
            return error_type::RateLimitTimeoutExceeded;
        }

        Serial.println(F("Waiting for rate limit compliance..."));

        unsigned long currentTime = millis();
        unsigned long nextAllowedTime = lastRequestTime + config->minRequestDelayMs;

        if (currentTime < nextAllowedTime) {
            unsigned long waitTime = nextAllowedTime - currentTime;

            // Ensure the wait time doesn't exceed our remaining budget
            unsigned long remainingBudget = config->maxWaitTimeMs - elapsedTime;
            if (waitTime > remainingBudget) {
                Serial.print(F("Wait time ("));
                Serial.print(waitTime);
                Serial.print(F("ms) would exceed budget, aborting"));
                return error_type::RateLimitTimeoutExceeded;
            }

            Serial.print(F("Delaying "));
            Serial.print(waitTime);
            Serial.println(F("ms for rate limit"));
            delay(waitTime);
        } else {
            // Wait a bit more for the sliding window to clear
            unsigned long remainingBudget = config->maxWaitTimeMs - elapsedTime;
            if (remainingBudget < 1000) {
                Serial.println(F("Insufficient time budget remaining, aborting"));
                return error_type::RateLimitTimeoutExceeded;
            }
            delay(1000);
        }

        yield(); // Allow other tasks to run
    }

    return error_type::None;
}

void google_drive_client::record_request()
{
    unsigned long currentTime = millis();
    lastRequestTime = currentTime;

    if (requestCount < GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW) {
        requestHistory[requestCount] = currentTime;
        requestCount++;
    } else {
        // Circular buffer - overwrite oldest entry
        requestHistory[requestHistoryIndex] = currentTime;
        requestHistoryIndex = (requestHistoryIndex + 1) % GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW;
    }

    Serial.print(F("Request recorded ("));
    Serial.print(requestCount);
    Serial.print(F("/"));
    Serial.print(GOOGLE_DRIVE_MAX_REQUESTS_PER_WINDOW);
    Serial.println(F(")"));
}

bool google_drive_client::handle_rate_limit_response(uint8_t attempt)
{
    if (attempt >= config->maxRetryAttempts) {
        Serial.println(F("Max retry attempts reached for rate limit"));
        return false;
    }

    // Exponential backoff: base delay * 2^attempt
    unsigned long backoffDelay = config->backoffBaseDelayMs * (1UL << attempt);

    // Cap the delay to prevent excessive waiting (max 2 minutes)
    if (backoffDelay > GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS) {
        backoffDelay = GOOGLE_DRIVE_BACKOFF_MAX_DELAY_MS;
    }

    Serial.print(F("Rate limited (HTTP 429). Backing off for "));
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

bool google_drive_client::rsaSignRS256(const String& input, String& sig_b64url)
{
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

String google_drive_client::create_jwt()
{
    StaticJsonDocument<64> hdr;
    hdr["alg"] = "RS256";
    hdr["typ"] = "JWT";
    String hdrStr;
    serializeJson(hdr, hdrStr);
    String hdrB64 = base64url(hdrStr);
    time_t now = time(NULL);
    const uint32_t iat = (uint32_t)now;
    const uint32_t exp = iat + 3600;
    StaticJsonDocument<384> claims;
    claims["iss"] = config->serviceAccountEmail;
    claims["scope"] = SCOPE;
    claims["aud"] = AUD;
    claims["iat"] = iat;
    claims["exp"] = exp;
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

photo_frame_error_t google_drive_client::get_access_token()
{
    Serial.println(F("Requesting new access token..."));

    String jwt = create_jwt();
    if (jwt.isEmpty())
        return error_type::JwtCreationFailed;

    Serial.print(F("JWT created, length: "));
    Serial.println(jwt.length());

    String body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + urlEncode(jwt);

    // Apply rate limiting before making the request
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        Serial.println(F("Rate limit wait timeout - aborting token request"));
        return rateLimitError;
    }

    uint8_t attempt = 0;
    while (attempt < config->maxRetryAttempts) {
        record_request();

        WiFiClientSecure client;
        HttpResponse response;
        bool networkError = false;

        // Set up SSL/TLS
        if (config->useInsecureTls) {
            Serial.println(F("WARNING: Using insecure TLS connection"));
            client.setInsecure();
        } else {
            if (g_google_root_ca.length() > 0) {
                client.setCACert(g_google_root_ca.c_str());
            } else {
                Serial.println(F("WARNING: No root CA certificate loaded, using insecure connection"));
                client.setInsecure();
            }
        }

        Serial.print(F("Connecting to token endpoint: "));
        Serial.println(TOKEN_HOST);

        // Attempt connection with timeout
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set socket timeout in seconds
        client.setHandshakeTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set handshake timeout in seconds
        unsigned long connectStart = millis();

        if (!client.connect(TOKEN_HOST, 443)) {
            Serial.print(F("Failed to connect to token endpoint: "));
            Serial.println(TOKEN_HOST);
            networkError = true;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("Google Drive API connection timeout"));
            client.stop();
            networkError = true;
        } else {
            // Build and send request using helper method
            String headers = "Content-Type: application/x-www-form-urlencoded";
            String req = build_http_request("POST", TOKEN_PATH, TOKEN_HOST, headers.c_str(), body.c_str());

            client.print(req);

            // Parse response
            if (!parse_http_response(client, response)) {
                Serial.println(F("Failed to parse HTTP response"));
                networkError = true;
            }
        }

        client.stop();

        // Classify failure and decide whether to retry
        failure_type failureType = classify_failure(response.statusCode, networkError);

        if (failureType == failure_type::Permanent) {
            Serial.print(F("Permanent failure (HTTP "));
            Serial.print(response.statusCode);
            Serial.println(F("), not retrying"));
            // Return specific error based on failure type and status code
            return photo_frame::error_utils::map_google_drive_error(response.statusCode, response.body.c_str(), "OAuth token request");
        }

        // Success case
        if (response.statusCode == 200 && response.hasContent) {
            // Parse token from response body
            size_t freeHeap = ESP.getFreeHeap();
            Serial.print(F("Free heap before token parse: "));
            Serial.println(freeHeap);

            StaticJsonDocument<768> doc;
            DeserializationError err = deserializeJson(doc, response.body);
            if (err) {
                Serial.print(F("Token parse error: "));
                Serial.println(err.c_str());
                Serial.print(F("Response body: "));
                Serial.println(response.body);
                // Check if this is an OAuth error response
                if (response.body.indexOf("invalid_grant") >= 0 || response.body.indexOf("invalid_client") >= 0) {
                    return photo_frame::error_utils::create_oauth_error("invalid_grant", "OAuth token response parsing failed");
                }
                return photo_frame::error_utils::create_oauth_error("invalid_token", "Failed to parse OAuth token response");
            }

            const char* tok = doc["access_token"];
            unsigned int expires_in = doc["expires_in"];

            if (!tok) {
                Serial.println(F("No access_token in response"));
                return error_type::OAuthTokenRequestFailed;
            }

            time_t tokenExpirationTime = time(NULL) + expires_in;

            safe_strcpy(g_access_token.accessToken, tok, sizeof(g_access_token.accessToken));
            g_access_token.expiresAt = tokenExpirationTime;
            g_access_token.obtainedAt = time(NULL);

            Serial.print(F("Token: "));
            Serial.println(g_access_token.accessToken);
            Serial.print(F("Token expires at: "));
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
            Serial.println(F("Max retry attempts reached for access token"));
            break;
        }
    }

    return error_type::HttpPostFailed;
}

photo_frame_error_t google_drive_client::list_files(const char* folderId,
    std::vector<google_drive_file>& outFiles,
    int pageSize)
{
    char nextPageToken[GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE] = "";
    int pageNumber = 1;
    size_t totalFilesRetrieved = 0;

    do {
        size_t filesBefore = outFiles.size();
        Serial.print(F("Fetching page "));
        Serial.print(pageNumber);
        Serial.print(F(" with pageSize="));
        Serial.print(pageSize);
        Serial.print(F(", current total files="));
        Serial.println(filesBefore);

        photo_frame_error_t err = list_files_in_folder(folderId, outFiles, pageSize, nextPageToken, nextPageToken);
        if (err != error_type::None) {
            Serial.print(F("Error fetching page "));
            Serial.print(pageNumber);
            Serial.print(F(": "));
            Serial.println(err.code);
            return err;
        }

        size_t filesThisPage = outFiles.size() - filesBefore;
        totalFilesRetrieved = outFiles.size();

        Serial.print(F("Page "));
        Serial.print(pageNumber);
        Serial.print(F(" complete: got "));
        Serial.print(filesThisPage);
        Serial.print(F(" files, total now "));
        Serial.print(totalFilesRetrieved);
        Serial.print(F(", nextPageToken size='"));
        Serial.print(strlen(nextPageToken));
        Serial.println(F("'"));

        pageNumber++;

        // Safety check to prevent infinite loops
        if (pageNumber > 20) {
            Serial.println(F("WARNING: Too many pages, breaking to prevent infinite loop"));
            break;
        }

    } while (strlen(nextPageToken) > 0);

    Serial.print(F("Pagination complete: retrieved "));
    Serial.print(totalFilesRetrieved);
    Serial.println(F(" total files"));

    return error_type::None;
}

photo_frame_error_t
google_drive_client::list_files_in_folder(const char* folderId,
    std::vector<google_drive_file>& outFiles,
    int pageSize,
    char* nextPageToken,
    const char* pageToken)
{
    Serial.print(F("Listing files in folder: "));
    Serial.print(folderId);

#if DEBUG_MODE
    Serial.print(F(", page token: "));
    if (pageToken && strlen(pageToken) > 0) {
        Serial.print(pageToken);
    } else {
        Serial.print(F("N/A"));
    }

    Serial.print(F(", page size: "));
    Serial.print(pageSize);

    if (nextPageToken && strlen(nextPageToken) > 0) {
        Serial.print(F(", next page token: "));
        Serial.print(nextPageToken);
    } else {
        Serial.print(F(", next page token: "));
        Serial.print(F("N/A"));
    }
#endif

    Serial.println();

    // Check if token is expired and refresh if needed
    if (is_token_expired()) {
        Serial.println(F("Token expired, refreshing before API call"));
        photo_frame_error_t refreshError = refresh_token();
        if (refreshError != error_type::None) {
            Serial.println(F("Failed to refresh expired token"));
            return refreshError;
        }
    }

    // Build Google Drive query with optimized String concatenation
    String q;
    q.reserve(strlen(folderId) + 15);
    q = "'";
    q += folderId;
    q += "' in parents";

    String encodedQ = urlEncode(q);
    String path;
    path.reserve(strlen(DRIVE_LIST_PATH) + encodedQ.length() + 100);
    path = DRIVE_LIST_PATH;
    path += "?q=";
    path += encodedQ;
    path += "&fields=nextPageToken,files(id,name)&pageSize=";
    path += String(pageSize);
    if (strlen(pageToken) > 0) {
        String encodedToken = urlEncode(String(pageToken));
        path += "&pageToken=";
        path += encodedToken;
    }

    // Apply rate limiting before making the request
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        Serial.println(F("Rate limit wait timeout - aborting files list request"));
        return rateLimitError;
    }

    uint8_t attempt = 0;
    while (attempt < config->maxRetryAttempts) {
        record_request();

        HttpResponse response;
        bool networkError = false;

        WiFiClientSecure client;

        if (config->useInsecureTls) {
            client.setInsecure();
        } else {
            if (g_google_root_ca.length() > 0) {
                client.setCACert(g_google_root_ca.c_str());
            } else {
                Serial.println(F("WARNING: No root CA certificate loaded, using insecure connection"));
                client.setInsecure();
            }
        }

        // Attempt connection with timeout
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set socket timeout in seconds
        client.setHandshakeTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set handshake timeout in seconds
        unsigned long connectStart = millis();
        if (!client.connect(DRIVE_HOST, 443)) {
            Serial.println(F("Failed to connect to Google Drive API"));
            networkError = true;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("Google Drive API connection timeout"));
            client.stop();
            networkError = true;
        } else {
            // Build and send request using helper method
            String headers = "Authorization: Bearer ";
            headers += g_access_token.accessToken;
            String req = build_http_request("GET", path.c_str(), DRIVE_HOST, headers.c_str());
            client.print(req);

            // Parse response
            if (!parse_http_response(client, response)) {
                Serial.println(F("Failed to parse HTTP response"));
                networkError = true;
            }
        }

        Serial.print(F("HTTP response code: "));
        Serial.print(response.statusCode);
        Serial.println();

        client.stop();

        // Classify failure and decide whether to retry
        failure_type failureType = classify_failure(response.statusCode, networkError);

        if (failureType == failure_type::Permanent) {
            Serial.print(F("Permanent failure (HTTP "));
            Serial.print(response.statusCode);
            Serial.println(F("), not retrying file listing"));
            return photo_frame::error_utils::map_google_drive_error(response.statusCode, response.body.c_str(), "File listing request");
        }

        // Success case
        if (response.statusCode == 200 && response.hasContent) {
            // Parse the JSON response
            photo_frame_error_t parseError = error_type::None;
            if (response.body.length() > GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD) { // threshold in bytes
                Serial.print(F("Using streaming parser for large response, size is: "));
                Serial.println(response.body.length());

                parseError = parse_file_list_streaming(response.body, outFiles, nextPageToken);
            } else {
                // For smaller responses, use platform-optimized static allocation
                StaticJsonDocument<GOOGLE_DRIVE_JSON_DOC_SIZE> doc;
                auto err = deserializeJson(doc, response.body);
                if (err) {
                    Serial.print(F("JSON parse error: "));
                    Serial.println(err.c_str());

                    // Fallback to streaming parser
                    Serial.println(F("Falling back to streaming parser"));
                    parseError = parse_file_list_streaming(response.body, outFiles, nextPageToken);
                } else {
                    if (nextPageToken) {
                        const char* token = doc["nextPageToken"] | "";
                        safe_strcpy(nextPageToken, token, GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE);
                    }

                    JsonArray arr = doc["files"].as<JsonArray>();
                    outFiles.reserve(outFiles.size() + arr.size());

                    for (JsonObject f : arr) {
                        const char* filename = (const char*)f["name"];
                        const char* fileExtension = strrchr(filename, '.');
                        if (!fileExtension || strcmp(fileExtension, LOCAL_FILE_EXTENSION) != 0) {
#if DEBUG_MODE
                            Serial.print(F("Skipping file with unsupported extension: "));
                            Serial.println(filename);
#endif // DEBUG_MODE
                            continue;
                        }

                        outFiles.emplace_back((const char*)f["id"],
                            (const char*)f["name"]);

                        // Yield every 10 files to prevent watchdog reset
                        if (outFiles.size() % 10 == 0) {
                            yield();
                        }
                    }
                    parseError = error_type::None;
                }
            }

            return parseError;
        }

        // Handle transient failures with proper backoff
        if (attempt < config->maxRetryAttempts - 1) {
            if (failureType == failure_type::RateLimit) {
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
            } else if (failureType == failure_type::TokenExpired) {
                Serial.println(F("Token expired, refreshing..."));
                photo_frame_error_t refreshError = refresh_token();
                if (refreshError != error_type::None) {
                    Serial.println(F("Failed to refresh token, stopping retries"));
                    break;
                }
            } else {
                if (!handle_transient_failure(attempt, failureType)) {
                    break;
                }
            }
            attempt++;
        } else {
            Serial.println(F("Max retry attempts reached for file listing"));
            break;
        }
    }

    return error_type::HttpGetFailed;
}

photo_frame_error_t
google_drive_client::parse_file_list_streaming(const String& jsonBody,
    std::vector<google_drive_file>& outFiles,
    char* nextPageToken)
{
    Serial.println(F("Using streaming JSON parser for file list"));

    // Extract nextPageToken first using simple string search
    const char* tokenKey = "\"nextPageToken\":\"";
    int tokenStart = jsonBody.indexOf(tokenKey);

    // if token was not found, let's try in another way
    if (tokenStart < 0) {
        tokenKey = "\"nextPageToken\": \"";
        tokenStart = jsonBody.indexOf(tokenKey);
    }

    size_t tokenLength = strlen(tokenKey);

    if (tokenStart >= 0 && nextPageToken) {
        // we need to find the very next '"' char
        tokenStart = jsonBody.indexOf("\"", tokenStart + tokenLength);
        int tokenEnd = jsonBody.indexOf("\"", tokenStart + 1);

#if DEBUG_MODE
        Serial.print(F("nextPageToken value start position: "));
        Serial.println(tokenStart);
        Serial.print(F("nextPageToken value end position: "));
        Serial.println(tokenEnd);
#endif // DEBUG_MODE

        if (tokenEnd > tokenStart && tokenStart > 0) { // tokenStart cannot be 0
            String token = jsonBody.substring(tokenStart, tokenEnd);
            safe_strcpy(nextPageToken, token.c_str(), GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE);
        } else {
            nextPageToken[0] = '\0';
        }
    } else if (nextPageToken) {
        Serial.println(F("No nextPageToken found in response"));
        nextPageToken[0] = '\0';
        
        Serial.println(F("jsonBody: "));
        // print the jsonBody line by line
        for (int i = 0; i < jsonBody.length(); i++) {
            Serial.print(jsonBody.charAt(i));
            if (jsonBody.charAt(i) == '\n') {
                yield();
            }
        }
    }

    // Find files array start
    // it can be also "files": [
    int filesStart = jsonBody.indexOf("\"files\":[");
    int filesMatchSize = 9;

    if (filesStart < 0) {
        Serial.println(F("No files array found in response, trying alternative format"));
        filesStart = jsonBody.indexOf("\"files\": [");
        if (filesStart < 0) {
            Serial.println(F("No files array found in response"));
            return error_type::JsonParseFailed;
        }
        filesMatchSize = 10;
    }

    filesStart += filesMatchSize; // Move past "files":["

    // Parse individual file objects
    int pos = filesStart;
    int braceCount = 0;
    int fileStart = -1;

    for (int i = pos; i < jsonBody.length(); i++) {
        char c = jsonBody.charAt(i);

        // Yield every 500 characters to prevent watchdog timeout on large JSON
        if (i % 500 == 0) {
            yield();
        }

        if (c == '{') {
            if (braceCount == 0) {
                fileStart = i;
            }
            braceCount++;
        } else if (c == '}') {
            braceCount--;
            if (braceCount == 0 && fileStart >= 0) {
                // Extract single file JSON
                String fileJson = jsonBody.substring(fileStart, i + 1);

                // Parse single file with small buffer
                StaticJsonDocument<512> fileDoc;
                DeserializationError err = deserializeJson(fileDoc, fileJson);

                if (err == DeserializationError::Ok) {
                    const char* filename = fileDoc["name"];
                    // we should skip those files which extension doesn't match LOCAL_FILE_EXTENSION
                    const char* fileExtension = strrchr(filename, '.');
                    if (!fileExtension || strcmp(fileExtension, LOCAL_FILE_EXTENSION) != 0) {
#if DEBUG_MODE
                        Serial.print(F("Skipping file with unsupported extension: "));
                        Serial.println(filename);
#endif // DEBUG_MODE
                        continue;
                    }

                    outFiles.emplace_back((const char*)fileDoc["id"],
                        (const char*)fileDoc["name"]);
                } else {
                    Serial.print(F("Failed to parse file entry: "));
                    Serial.println(err.c_str());
                }

                fileStart = -1;

                // Yield every 5 files to prevent watchdog reset
                if (outFiles.size() % 5 == 0) {
                    yield();
                }
            }
        } else if (c == ']' && braceCount == 0) {
            // End of files array
            break;
        }
    }

    Serial.print(F("Streaming parser processed "));
    Serial.print(outFiles.size());
    Serial.println(F(" files"));

    return error_type::None;
}

photo_frame_error_t google_drive_client::download_file(const String& fileId, fs::File* outFile)
{
    // Check if token is expired and refresh if needed
    if (is_token_expired()) {
        Serial.println(F("Token expired, refreshing before download"));
        photo_frame_error_t refreshError = refresh_token();
        if (refreshError != error_type::None) {
            Serial.println(F("Failed to refresh expired token"));
            return refreshError;
        }
    }

    // Build download path with optimized String concatenation
    String path;
    path.reserve(fileId.length() + 30); // "/drive/v3/files/" + "?alt=media"
    path = "/drive/v3/files/";
    path += fileId;
    path += "?alt=media";

    // Apply rate limiting before making the request
    photo_frame_error_t rateLimitError = wait_for_rate_limit();
    if (rateLimitError != error_type::None) {
        Serial.println(F("Rate limit wait timeout - aborting download request"));
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
                Serial.println(F("WARNING: No root CA certificate loaded, connection may fail"));
                client.setInsecure(); // Fallback to insecure connection
            }
        }
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set socket timeout in seconds
        client.setHandshakeTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set handshake timeout in seconds

        unsigned long connectStart = millis();
        if (!client.connect(DRIVE_HOST, 443)) {
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("Download connection failed, retrying..."));
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpConnectFailed;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("Download connection timeout"));
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("Retrying after timeout..."));
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

#if DEBUG_MODE
        Serial.print(F("HTTP Request: "));
        Serial.println(req);
#endif

        client.print(req);

        // Read and validate HTTP status line first
        String statusLine;
        do {
            statusLine = client.readStringUntil('\n');
        } while (statusLine.length() == 0);

        statusLine.trim();
        Serial.print(F("HTTP Status: "));
        Serial.println(statusLine);

        // Check if status is 200 OK
        if (statusLine.indexOf("200") < 0) {
            Serial.println(F("HTTP error - not 200 OK"));
            client.stop();
            if (attempt < config->maxRetryAttempts - 1) {
                Serial.println(F("Retrying after HTTP error..."));
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return error_type::HttpGetFailed;
        }

        // Parse remaining headers to check for chunked encoding
        bool isChunked = false;
        long contentLength = -1;
        int headerCount = 0;

        while (true) {
            String line = client.readStringUntil('\n');
            line.trim();

            Serial.print(F("Header["));
            Serial.print(headerCount++);
            Serial.print(F("]: "));
            Serial.println(line);

            if (line.length() == 0) {
                Serial.println(F("End of headers detected"));
                break;
            }
            if (!client.connected() && !client.available()) {
                Serial.println(F("Connection lost while reading headers"));
                break;
            }

            // Check for Transfer-Encoding: chunked
            if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
                isChunked = true;
                Serial.println(F("Detected chunked transfer encoding"));
            }
            // Check for Content-Length
            else if (line.startsWith("Content-Length:")) {
                int colonPos = line.indexOf(':');
                if (colonPos >= 0) {
                    String lengthStr = line.substring(colonPos + 1);
                    lengthStr.trim();
                    contentLength = lengthStr.toInt();
                    Serial.print(F("Content-Length: "));
                    Serial.println(contentLength);
                }
            }
        }

        // Download file content based on encoding type
        bool readAny = false;
        uint32_t bytesRead = 0;

        if (isChunked) {
            // Handle chunked transfer encoding
            Serial.println(F("Reading chunked response"));
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
                    Serial.println(F("End of chunked data"));
                    break;
                }

                // Read the actual chunk data
                uint8_t buf[2048];
                long remainingInChunk = chunkSize;

                while (remainingInChunk > 0 && (client.connected() || client.available())) {
                    int toRead = min((long)sizeof(buf), remainingInChunk);
                    int n = client.read(buf, toRead);

                    if (n > 0) {
                        outFile->write(buf, n);
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
        } else {
            // Handle regular (non-chunked) response
            Serial.println(F("Reading non-chunked response"));
            uint8_t buf[2048];
            long remainingBytes = contentLength;

            while (client.connected() || client.available()) {
                int toRead = sizeof(buf);
                if (contentLength > 0 && remainingBytes > 0) {
                    toRead = min((long)sizeof(buf), remainingBytes);
                }

                int n = client.read(buf, toRead);
                if (n > 0) {
                    outFile->write(buf, n);
                    readAny = true;
                    bytesRead += n;

                    if (contentLength > 0) {
                        remainingBytes -= n;
                        if (remainingBytes <= 0) {
                            Serial.println(F("Reached expected content length"));
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

        Serial.print(F("Total bytes downloaded: "));
        Serial.println(bytesRead);

        client.stop();

        if (readAny) {
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

void google_drive_client::set_access_token(const google_drive_access_token& token)
{
    // use the copy constructor
    g_access_token = token;
}

const google_drive_access_token* google_drive_client::get_access_token_value() const
{
    return &g_access_token;
}

bool google_drive_client::parse_http_response(WiFiClientSecure& client, HttpResponse& response)
{
    // Read the status line first
    // keep reading while the line is empty, until we get a non-empty line
    String statusLine;
    do {
        statusLine = client.readStringUntil('\n');
    } while (statusLine.length() == 0);

    // Parse status code from "HTTP/1.1 200 OK" format
    int firstSpace = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);

    if (firstSpace > 0 && secondSpace > firstSpace) {
        String statusCodeStr = statusLine.substring(firstSpace + 1, secondSpace);
        response.statusCode = statusCodeStr.toInt();
        response.statusMessage = statusLine.substring(secondSpace + 1);
        response.statusMessage.trim();
    } else {
        Serial.print(F("Failed to parse status line: "));
        Serial.println(statusLine);
        return false;
    }

    // Read and parse headers
    uint16_t headerCount = 0;
    bool isChunked = false;
    int contentLength = -1;

    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            break; // End of headers
        }
        if (!client.connected() && !client.available()) {
            break;
        }

        // Check for Transfer-Encoding: chunked
        if (line.startsWith("Transfer-Encoding:") || line.startsWith("transfer-encoding:")) {
            if (line.indexOf("chunked") != -1) {
                isChunked = true;
            }
        }

        // Check for Content-Length
        if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
            int colonPos = line.indexOf(':');
            if (colonPos != -1) {
                String lengthStr = line.substring(colonPos + 1);
                lengthStr.trim();
                contentLength = lengthStr.toInt();
            }
        }

        // Yield every 10 headers to prevent watchdog timeout
        if (++headerCount % 10 == 0) {
            yield();
        }
    }

    // Read response body based on encoding
    response.body = "";
    uint32_t bodyBytesRead = 0;

    if (isChunked) {
        Serial.println(F("Response uses chunked transfer encoding"));
        // Handle chunked transfer encoding
        while (client.connected() || client.available()) {
            // Read chunk size line
            String chunkSizeLine = client.readStringUntil('\n');
            if (chunkSizeLine.length() == 0) {
                break;
            }

            // Parse hex chunk size (ignore any chunk extensions after ';')
            int semicolonPos = chunkSizeLine.indexOf(';');
            if (semicolonPos != -1) {
                chunkSizeLine = chunkSizeLine.substring(0, semicolonPos);
            }
            chunkSizeLine.trim();

            // Convert hex string to integer
            long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);

            // If chunk size is 0, we've reached the end
            if (chunkSize == 0) {
                // Skip any trailing headers and final CRLF
                while (client.connected() || client.available()) {
                    String trailerLine = client.readStringUntil('\n');
                    if (trailerLine == "\r" || trailerLine.length() == 0) {
                        break;
                    }
                }
                break;
            }

            // Read the chunk data
            for (long i = 0; i < chunkSize && (client.connected() || client.available()); i++) {
                char c = client.read();
                if (c < 0) {
                    break;
                }
                response.body += c;

                // Yield every 1KB to prevent watchdog timeout
                if (++bodyBytesRead % 1024 == 0) {
                    yield();
                }
            }

            // Skip the trailing CRLF after chunk data
            client.readStringUntil('\n');
        }
    } else {
        // Handle regular content (with or without Content-Length)
        int bytesToRead = contentLength;
        while (client.connected() || client.available()) {
            char c = client.read();
            if (c < 0) {
                yield(); // Yield when no data available
                break;
            }
            response.body += c;

            // If we have Content-Length, stop when we've read enough
            if (contentLength > 0 && --bytesToRead <= 0) {
                break;
            }

            // Yield every 1KB to prevent watchdog timeout on large responses
            if (++bodyBytesRead % 1024 == 0) {
                yield();
            }
        }
    }

    response.hasContent = response.body.length() > 0;

    Serial.print(F("HTTP Response: "));
    Serial.print(response.statusCode);
    Serial.print(F(" "));
    Serial.print(response.statusMessage);
    Serial.print(F(", Body length: "));
    Serial.println(response.body.length());
    return true;
}

failure_type google_drive_client::classify_failure(int statusCode, bool hasNetworkError)
{
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
        default:
            return failure_type::Permanent;
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

bool google_drive_client::handle_transient_failure(uint8_t attempt, failure_type failureType)
{
    if (attempt >= config->maxRetryAttempts) {
        Serial.println(F("Max retry attempts reached for transient failure"));
        return false;
    }

    // Calculate base delay based on failure type
    unsigned long baseDelay;
    const char* failureTypeName;

    switch (failureType) {
    case failure_type::RateLimit:
        baseDelay = config->backoffBaseDelayMs * 2; // Longer delay for rate limits
        failureTypeName = "rate limit";
        break;
    case failure_type::Transient:
        baseDelay = config->backoffBaseDelayMs;
        failureTypeName = "transient error";
        break;
    default:
        baseDelay = config->backoffBaseDelayMs;
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

    Serial.print(F("Transient failure ("));
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

unsigned long google_drive_client::add_jitter(unsigned long base_delay)
{
    // Add up to 25% random jitter to prevent thundering herd
    unsigned long max_jitter = base_delay / 4;
    unsigned long jitter = random(0, max_jitter + 1);
    return base_delay + jitter;
}

bool google_drive_client::is_token_expired(int marginSeconds)
{
    return g_access_token.expired(marginSeconds);
}

photo_frame_error_t google_drive_client::refresh_token()
{
    Serial.println(F("Refreshing access token..."));

    // Clear the current token
    g_access_token.accessToken[0] = '\0';
    g_access_token.expiresAt = 0;
    g_access_token.obtainedAt = 0;

    // Get a new token
    return get_access_token();
}

String google_drive_client::build_http_request(const char* method,
    const char* path,
    const char* host,
    const char* headers,
    const char* body)
{
    // Calculate required buffer size
    size_t reqLen = strlen(method) + strlen(path) + strlen(host) + 50; // Base size for HTTP/1.1, Host, Connection: close

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
    req += " HTTP/1.0\r\nHost: ";
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

    // End headers and add body
    req += "Connection: close\r\n\r\n";

    if (body) {
        req += body;
    }

    return req;
}

void google_drive_client::set_root_ca_certificate(const String& rootCA)
{
    g_google_root_ca = rootCA;
    Serial.println(F("Root CA certificate set for Google Drive client"));
}

size_t google_drive_client::list_files_streaming(const char* folderId, fs::File& tocFile, int pageSize)
{
    char nextPageToken[GOOGLE_DRIVE_PAGE_TOKEN_BUFFER_SIZE] = "";
    int pageNumber = 1;
    size_t totalFilesWritten = 0;

    do {
        Serial.print(F("Fetching page "));
        Serial.print(pageNumber);
        Serial.print(F(" with pageSize="));
        Serial.print(pageSize);
        Serial.print(F(", current total files="));
        Serial.println(totalFilesWritten);

        size_t filesThisPage = list_files_in_folder_streaming(folderId, tocFile, pageSize, nextPageToken, nextPageToken);
        if (filesThisPage == 0) {
            Serial.print(F("Error fetching page "));
            Serial.print(pageNumber);
            Serial.println(F(" - no files returned"));
            return totalFilesWritten; // Return what we have written so far
        }

        totalFilesWritten += filesThisPage;

        Serial.print(F("Page "));
        Serial.print(pageNumber);
        Serial.print(F(" complete: got "));
        Serial.print(filesThisPage);
        Serial.print(F(" files, total now "));
        Serial.print(totalFilesWritten);
        Serial.print(F(", nextPageToken size='"));
        Serial.print(strlen(nextPageToken));
        Serial.println(F("'"));

        pageNumber++;

        // Safety check to prevent infinite loops
        if (pageNumber > 50) {
            Serial.println(F("Warning: Reached maximum page limit (50)"));
            break;
        }
    } while (strlen(nextPageToken) > 0);

    Serial.print(F("Pagination complete: streamed "));
    Serial.print(totalFilesWritten);
    Serial.println(F(" total files to TOC"));

    return totalFilesWritten;
}

size_t google_drive_client::list_files_in_folder_streaming(const char* folderId,
                                                          fs::File& tocFile,
                                                          int pageSize,
                                                          char* nextPageToken,
                                                          const char* pageToken)
{
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

    String path;
    path.reserve(strlen(DRIVE_LIST_PATH) + encodedQ.length() + 100);
    path = DRIVE_LIST_PATH;
    path += "?q=";
    path += encodedQ;
    path += "&fields=nextPageToken,files(id,name)&pageSize=";
    path += String(pageSize);
    if (strlen(pageToken) > 0) {
        String encodedToken = urlEncode(String(pageToken));
        path += "&pageToken=";
        path += encodedToken;
    }

    // Build and send the HTTP request
    String headers = "Authorization: Bearer ";
    headers += g_access_token.accessToken;
    headers += "\r\n";

    String httpRequest = build_http_request("GET", path.c_str(), "www.googleapis.com", headers.c_str());

    WiFiClientSecure client;
    if (config->useInsecureTls) {
        client.setInsecure();
        Serial.println(F("WARNING: Using insecure TLS connection"));
    }

    if (!client.connect("www.googleapis.com", 443)) {
        Serial.println(F("Connection to Google Drive failed"));
        return 0;
    }

    record_request();
    client.print(httpRequest);

    HttpResponse response;
    if (!parse_http_response(client, response)) {
        client.stop();
        return 0;
    }

    client.stop();

    if (response.statusCode != 200) {
        Serial.print(F("HTTP Error: "));
        Serial.println(response.statusCode);
        return 0;
    }

    // Parse the JSON response and write directly to TOC file
    return parse_file_list_to_toc(response.body, tocFile, nextPageToken);
}

size_t google_drive_client::parse_file_list_to_toc(const String& jsonBody,
                                                   fs::File& tocFile,
                                                   char* nextPageToken)
{
    size_t filesWritten = 0;

    // Clear the next page token
    if (nextPageToken) {
        nextPageToken[0] = '\0';
    }

    // Parse the JSON using ArduinoJson with static allocation
    StaticJsonDocument<GOOGLE_DRIVE_JSON_DOC_SIZE> doc;
    DeserializationError error = deserializeJson(doc, jsonBody);
    if (error) {
        Serial.print(F("JSON deserialization failed: "));
        Serial.println(error.c_str());
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
        for (JsonVariant f : files) {
            if (!f.containsKey("id") || !f.containsKey("name")) {
                continue;
            }

            const char* id = f["id"];
            const char* name = f["name"];

            // Write directly to TOC file: id|name
            tocFile.print(id);
            tocFile.print(F("|"));
            tocFile.println(name);

            filesWritten++;

            // In google_drive_client.cpp, modify the loop:
            if (filesWritten % 50 == 0) { // Every 50 files instead of just at the end
                tocFile.flush();
                yield();
            }

            // Yield every 10 files to prevent watchdog reset
            if (filesWritten % 10 == 0) {
                yield();
            }
        }
        tocFile.flush();
    }

    return filesWritten;
}

} // namespace photo_frame
