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
    // 1. Skip headers
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

    // 2. Read raw body into a buffer
    String body;
    uint32_t bodyBytesRead = 0;
    while (client.connected() || client.available()) {
        char c = client.read();
        if (c < 0) {
            yield(); // Yield when no data available
            break;
        }
        body += c;
        
        // Yield every 1KB to prevent watchdog timeout on large responses
        if (++bodyBytesRead % 1024 == 0) {
            yield();
        }
    }

    // 3. Remove chunk markers (very simple filter)
    String cleanBody;
    int i = 0;
    uint16_t chunkCount = 0;
    while (i < body.length()) {
        // chunk size is hex, then \r\n
        int pos = body.indexOf("\r\n", i);
        if (pos == -1)
            break;
        String chunkSizeHex = body.substring(i, pos);
        int chunkSize = strtol(chunkSizeHex.c_str(), NULL, 16);
        if (chunkSize == 0)
            break; // end of chunks
        i = pos + 2;
        cleanBody += body.substring(i, i + chunkSize);
        i += chunkSize + 2; // skip chunk + CRLF
        
        // Yield every 5 chunks to prevent watchdog timeout
        if (++chunkCount % 5 == 0) {
            yield();
        }
    }

    return cleanBody;
}

// Helper functions
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


google_drive_client::google_drive_client(const google_drive_client_config& config)
    : config(&config)
    , tokenExpirationTime(0)
    , lastRequestTime(0)
    , requestHistoryIndex(0)
    , requestCount(0)
{
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    const char* pers = "esp32jwt";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));
    
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

void google_drive_client::clean_old_requests() {
    unsigned long currentTime = millis();
    unsigned long windowStartTime = currentTime - (GOOGLE_DRIVE_RATE_LIMIT_WINDOW_SECONDS * 1000UL);
    
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

bool google_drive_client::can_make_request() {
    clean_old_requests();
    
    unsigned long currentTime = millis();
    
    // Check minimum delay between requests
    if (lastRequestTime > 0 && (currentTime - lastRequestTime) < GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS) {
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

void google_drive_client::wait_for_rate_limit() {
    while (!can_make_request()) {
        Serial.println(F("Waiting for rate limit compliance..."));
        
        unsigned long currentTime = millis();
        unsigned long nextAllowedTime = lastRequestTime + GOOGLE_DRIVE_MIN_REQUEST_DELAY_MS;
        
        if (currentTime < nextAllowedTime) {
            unsigned long waitTime = nextAllowedTime - currentTime;
            Serial.print(F("Delaying "));
            Serial.print(waitTime);
            Serial.println(F("ms for rate limit"));
            delay(waitTime);
        } else {
            // Wait a bit more for the sliding window to clear
            delay(1000);
        }
        
        yield(); // Allow other tasks to run
    }
}

void google_drive_client::record_request() {
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

bool google_drive_client::handle_rate_limit_response(uint8_t attempt) {
    if (attempt >= GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
        Serial.println(F("Max retry attempts reached for rate limit"));
        return false;
    }
    
    // Exponential backoff: base delay * 2^attempt
    unsigned long backoffDelay = GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS * (1UL << attempt);
    
    // Cap the delay to prevent excessive waiting (max 2 minutes)
    if (backoffDelay > 120000UL) {
        backoffDelay = 120000UL;
    }
    
    Serial.print(F("Rate limited (HTTP 429). Backing off for "));
    Serial.print(backoffDelay);
    Serial.print(F("ms (attempt "));
    Serial.print(attempt + 1);
    Serial.print(F("/"));
    Serial.print(GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS);
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
    int ret = mbedtls_pk_parse_key(&pk, (const unsigned char*)(config->privateKeyPem), strlen(config->privateKeyPem) + 1, NULL, 0, NULL, NULL);
#else
    // ESP32, ESP32-S2, ESP32-S3 use older mbedtls API
    int ret = mbedtls_pk_parse_key(&pk, (const unsigned char*)(config->privateKeyPem), strlen(config->privateKeyPem) + 1, NULL, 0);
#endif
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }
    std::unique_ptr<unsigned char[]> sig(new unsigned char[MBEDTLS_PK_SIGNATURE_MAX_SIZE]);
    size_t sig_len = 0;
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-C6 and newer variants use updated mbedtls API with sig_size parameter
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig.get(), MBEDTLS_PK_SIGNATURE_MAX_SIZE, &sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
#else
    // ESP32, ESP32-S2, ESP32-S3 use older mbedtls API without sig_size parameter
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig.get(), &sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
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

photo_frame::photo_frame_error_t google_drive_client::get_access_token()
{
    String jwt = create_jwt();
    if (jwt.isEmpty())
        return photo_frame::error_type::JwtCreationFailed;
    
    String body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" + urlEncode(jwt);
    
    // Apply rate limiting before making the request
    wait_for_rate_limit();
    
    uint8_t attempt = 0;
    while (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
        record_request();
        
        WiFiClientSecure client;
        HttpResponse response;
        bool networkError = false;
        
        // Set up SSL/TLS
#ifdef USE_INSECURE_TLS
        client.setInsecure();
#else
        if (g_google_root_ca.length() > 0) {
            client.setCACert(g_google_root_ca.c_str());
        } else {
            Serial.println(F("WARNING: No root CA certificate loaded, using insecure connection"));
            client.setInsecure();
        }
#endif
        
        // Attempt connection with timeout
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set socket timeout in seconds
        unsigned long connectStart = millis();
        if (!client.connect(TOKEN_HOST, 443)) {
            Serial.println(F("Failed to connect to token endpoint"));
            networkError = true;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("Token endpoint connection timeout"));
            client.stop();
            networkError = true;
        } else {
            // Build and send request
            String req;
            size_t reqLen = strlen(TOKEN_PATH) + strlen(TOKEN_HOST) + body.length() + 150;
            req.reserve(reqLen);
            req = "POST ";
            req += TOKEN_PATH;
            req += " HTTP/1.1\r\nHost: ";
            req += TOKEN_HOST;
            req += "\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ";
            req += String(body.length());
            req += "\r\nConnection: close\r\n\r\n";
            req += body;
            
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
            return photo_frame::error_type::HttpPostFailed;
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
                return photo_frame::error_type::JsonParseFailed;
            }
            
            const char* tok = doc["access_token"];
            if (!tok) {
                Serial.println(F("No access_token in response"));
                return photo_frame::error_type::TokenMissing;
            }
            
            g_access_token = tok;
            tokenExpirationTime = time(NULL) + 3600; // Token expires in 1 hour
            Serial.print(F("Access token obtained, length: "));
            Serial.println(g_access_token.length());
            Serial.print(F("Token expires at: "));
            Serial.println(tokenExpirationTime);
            return photo_frame::error_type::None;
        }
        
        // Handle transient failures with proper backoff
        if (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS - 1) {
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
    
    return photo_frame::error_type::HttpPostFailed;
}

photo_frame::photo_frame_error_t google_drive_client::list_files(const char* folderId, std::vector<google_drive_file>& outFiles, int pageSize)
{
    String nextPageToken;
    do {
        photo_frame::photo_frame_error_t err = list_files_in_folder(folderId, outFiles, pageSize, &nextPageToken, nextPageToken);
        if (err != photo_frame::error_type::None) {
            return err;
        }
    } while (nextPageToken.length() > 0);
    return photo_frame::error_type::None;
}

photo_frame::photo_frame_error_t google_drive_client::list_files_in_folder(const char* folderId, std::vector<google_drive_file>& outFiles, int pageSize, String* nextPageToken, const String& pageToken)
{
    // Check if token is expired and refresh if needed
    if (is_token_expired()) {
        Serial.println(F("Token expired, refreshing before API call"));
        photo_frame::photo_frame_error_t refreshError = refresh_token();
        if (refreshError != photo_frame::error_type::None) {
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
    path += "&fields=nextPageToken,files(id,name,mimeType,modifiedTime)&orderBy=modifiedTime&pageSize=";
    path += String(pageSize);
    if (pageToken.length() > 0) {
        String encodedToken = urlEncode(pageToken);
        path += "&pageToken=";
        path += encodedToken;
    }
    
    // Apply rate limiting before making the request
    wait_for_rate_limit();
    
    uint8_t attempt = 0;
    while (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
        record_request();
        
        HttpResponse response;
        bool networkError = false;
        
        WiFiClientSecure client;
#ifdef USE_INSECURE_TLS
        client.setInsecure();
#else
        if (g_google_root_ca.length() > 0) {
            client.setCACert(g_google_root_ca.c_str());
        } else {
            Serial.println(F("WARNING: No root CA certificate loaded, using insecure connection"));
            client.setInsecure();
        }
#endif
        
        // Attempt connection with timeout
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set socket timeout in seconds
        unsigned long connectStart = millis();
        if (!client.connect(DRIVE_HOST, 443)) {
            Serial.println(F("Failed to connect to Google Drive API"));
            networkError = true;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("Google Drive API connection timeout"));
            client.stop();
            networkError = true;
        } else {
            // Build and send request
            String req;
            size_t reqLen = path.length() + strlen(DRIVE_HOST) + g_access_token.length() + 100;
            req.reserve(reqLen);
            req = "GET ";
            req += path;
            req += " HTTP/1.1\r\nHost: ";
            req += DRIVE_HOST;
            req += "\r\nAuthorization: Bearer ";
            req += g_access_token;
            req += "\r\nConnection: close\r\n\r\n";
            
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
            Serial.println(F("), not retrying file listing"));
            return photo_frame::error_type::HttpGetFailed;
        }
        
        // Success case
        if (response.statusCode == 200 && response.hasContent) {
            Serial.print(F("Google Drive response size: "));
            Serial.println(response.body.length());
            
            // Parse the JSON response
            photo_frame::photo_frame_error_t parseError = photo_frame::error_type::None;
            if (response.body.length() > 6144) { // 6KB threshold
                Serial.println(F("Using streaming parser for large response"));
                parseError = parse_file_list_streaming(response.body, outFiles, nextPageToken);
            } else {
                // For smaller responses, use optimized static allocation
                StaticJsonDocument<6144> doc;
                auto err = deserializeJson(doc, response.body);
                if (err) {
                    Serial.print(F("JSON parse error: "));
                    Serial.println(err.c_str());
                    
                    // Fallback to streaming parser
                    Serial.println(F("Falling back to streaming parser"));
                    parseError = parse_file_list_streaming(response.body, outFiles, nextPageToken);
                } else {
                    if (nextPageToken) {
                        *nextPageToken = doc["nextPageToken"] | "";
                    }

                    JsonArray arr = doc["files"].as<JsonArray>();
                    outFiles.reserve(outFiles.size() + arr.size());
                    
                    for (JsonObject f : arr) {
                        outFiles.emplace_back(
                            (const char*)f["id"],
                            (const char*)f["name"],
                            (const char*)f["mimeType"],
                            (const char*)f["modifiedTime"]);
                        
                        // Yield every 10 files to prevent watchdog reset
                        if (outFiles.size() % 10 == 0) {
                            yield();
                        }
                    }
                    parseError = photo_frame::error_type::None;
                }
            }
            
            return parseError;
        }
        
        // Handle transient failures with proper backoff
        if (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS - 1) {
            if (failureType == failure_type::RateLimit) {
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
            } else if (failureType == failure_type::TokenExpired) {
                Serial.println(F("Token expired, refreshing..."));
                photo_frame::photo_frame_error_t refreshError = refresh_token();
                if (refreshError != photo_frame::error_type::None) {
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
    
    return photo_frame::error_type::HttpGetFailed;
}

photo_frame::photo_frame_error_t google_drive_client::parse_file_list_streaming(const String& jsonBody, std::vector<google_drive_file>& outFiles, String* nextPageToken) {
    Serial.println(F("Using streaming JSON parser for file list"));
    
    // Extract nextPageToken first using simple string search
    int tokenStart = jsonBody.indexOf("\"nextPageToken\":\"");
    if (tokenStart >= 0 && nextPageToken) {
        tokenStart += 17; // Length of "nextPageToken":""
        int tokenEnd = jsonBody.indexOf("\"", tokenStart);
        if (tokenEnd > tokenStart) {
            *nextPageToken = jsonBody.substring(tokenStart, tokenEnd);
        } else {
            *nextPageToken = "";
        }
    } else if (nextPageToken) {
        *nextPageToken = "";
    }
    
    // Find files array start
    int filesStart = jsonBody.indexOf("\"files\":[");
    if (filesStart < 0) {
        Serial.println(F("No files array found in response"));
        return photo_frame::error_type::JsonParseFailed;
    }
    
    filesStart += 9; // Move past "files":["
    
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
                    outFiles.emplace_back(
                        (const char*)fileDoc["id"],
                        (const char*)fileDoc["name"], 
                        (const char*)fileDoc["mimeType"],
                        (const char*)fileDoc["modifiedTime"]);
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
    
    return photo_frame::error_type::None;
}

photo_frame::photo_frame_error_t google_drive_client::download_file(const String& fileId, fs::File* outFile)
{
    // Check if token is expired and refresh if needed
    if (is_token_expired()) {
        Serial.println(F("Token expired, refreshing before download"));
        photo_frame::photo_frame_error_t refreshError = refresh_token();
        if (refreshError != photo_frame::error_type::None) {
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
    wait_for_rate_limit();
    
    uint8_t attempt = 0;
    while (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
        record_request();
        
        WiFiClientSecure client;

#ifdef USE_INSECURE_TLS
        client.setInsecure();
#else
        if (g_google_root_ca.length() > 0) {
            client.setCACert(g_google_root_ca.c_str());
        } else {
            Serial.println(F("WARNING: No root CA certificate loaded, connection may fail"));
            client.setInsecure(); // Fallback to insecure connection
        }
#endif
        client.setTimeout(HTTP_REQUEST_TIMEOUT / 1000); // Set socket timeout in seconds
        unsigned long connectStart = millis();
        if (!client.connect(DRIVE_HOST, 443)) {
            if (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS - 1) {
                Serial.println(F("Download connection failed, retrying..."));
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return photo_frame::error_type::HttpConnectFailed;
        } else if (millis() - connectStart > HTTP_CONNECT_TIMEOUT) {
            Serial.println(F("Download connection timeout"));
            client.stop();
            if (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS - 1) {
                Serial.println(F("Retrying after timeout..."));
                if (!handle_rate_limit_response(attempt)) {
                    break;
                }
                attempt++;
                continue;
            }
            return photo_frame::error_type::HttpConnectFailed;
        }
        
        // Build request with optimized String concatenation
        String req;
        size_t reqLen = path.length() + strlen(DRIVE_HOST) + g_access_token.length() + 100;
        req.reserve(reqLen);
        req = "GET ";
        req += path;
        req += " HTTP/1.1\r\nHost: ";
        req += DRIVE_HOST;
        req += "\r\nAuthorization: Bearer ";
        req += g_access_token;
        req += "\r\nConnection: close\r\n\r\n";
        client.print(req);
        
        // Skip headers
        while (true) {
            String line = client.readStringUntil('\n');
            if (line == "\r")
                break;
            if (!client.connected() && !client.available())
                break;
        }
        
        // Download file content
        uint8_t buf[2048];
        bool readAny = false;
        uint32_t bytesRead = 0;
        while (client.connected() || client.available()) {
            int n = client.read(buf, sizeof(buf));
            if (n > 0) {
                outFile->write(buf, n);
                readAny = true;
                bytesRead += n;
                
                // Yield every 8KB to prevent watchdog timeout on large downloads
                if (bytesRead % 8192 == 0) {
                    yield();
                }
            } else {
                // Yield when no data is available to prevent tight loops
                yield();
            }
        }
        client.stop();
        
        if (readAny) {
            return photo_frame::error_type::None; // Success
        }
        
        // If no data was read, consider it a failure
        if (attempt < GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS - 1) {
            Serial.println(F("Download failed (no data), retrying..."));
            if (!handle_rate_limit_response(attempt)) {
                break;
            }
        }
        attempt++;
    }
    
    return photo_frame::error_type::DownloadFailed;
}


String google_drive_client::get_access_token_value() const { return g_access_token; }

bool google_drive_client::parse_http_response(WiFiClientSecure& client, HttpResponse& response) {
    // Read the status line first
    String statusLine = client.readStringUntil('\n');
    if (statusLine.length() == 0) {
        Serial.println(F("Empty HTTP response"));
        return false;
    }
    
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
    
    // Read and skip headers
    uint16_t headerCount = 0;
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            break; // End of headers
        }
        if (!client.connected() && !client.available()) {
            break;
        }
        
        // Yield every 10 headers to prevent watchdog timeout
        if (++headerCount % 10 == 0) {
            yield();
        }
    }
    
    // Read response body
    response.body = "";
    uint32_t bodyBytesRead = 0;
    while (client.connected() || client.available()) {
        char c = client.read();
        if (c < 0) {
            yield(); // Yield when no data available
            break;
        }
        response.body += c;
        
        // Yield every 1KB to prevent watchdog timeout on large responses
        if (++bodyBytesRead % 1024 == 0) {
            yield();
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

bool google_drive_client::handle_transient_failure(uint8_t attempt, failure_type failureType) {
    if (attempt >= GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS) {
        Serial.println(F("Max retry attempts reached for transient failure"));
        return false;
    }
    
    // Calculate base delay based on failure type
    unsigned long baseDelay;
    const char* failureTypeName;
    
    switch (failureType) {
        case failure_type::RateLimit:
            baseDelay = GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS * 2; // Longer delay for rate limits
            failureTypeName = "rate limit";
            break;
        case failure_type::Transient:
            baseDelay = GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS;
            failureTypeName = "transient error";
            break;
        default:
            baseDelay = GOOGLE_DRIVE_BACKOFF_BASE_DELAY_MS;
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
    backoffDelay = addJitter(backoffDelay);
    
    Serial.print(F("Transient failure ("));
    Serial.print(failureTypeName);
    Serial.print(F("). Backing off for "));
    Serial.print(backoffDelay);
    Serial.print(F("ms (attempt "));
    Serial.print(attempt + 1);
    Serial.print(F("/"));
    Serial.print(GOOGLE_DRIVE_MAX_RETRY_ATTEMPTS);
    Serial.println(F(")"));
    
    delay(backoffDelay);
    yield();
    
    return true;
}

unsigned long google_drive_client::addJitter(unsigned long baseDelay) {
    // Add up to 25% random jitter to prevent thundering herd
    unsigned long maxJitter = baseDelay / 4;
    unsigned long jitter = random(0, maxJitter + 1);
    return baseDelay + jitter;
}

bool google_drive_client::is_token_expired(int marginSeconds) {
    if (tokenExpirationTime == 0) {
        // No token obtained yet
        return true;
    }
    
    time_t currentTime = time(NULL);
    return (currentTime + marginSeconds) >= tokenExpirationTime;
}

photo_frame::photo_frame_error_t google_drive_client::refresh_token() {
    Serial.println(F("Refreshing access token..."));
    
    // Clear the current token
    g_access_token = "";
    tokenExpirationTime = 0;
    
    // Get a new token
    return get_access_token();
}

#if !defined(USE_INSECURE_TLS)
void google_drive_client::set_root_ca_certificate(const String& rootCA) {
    g_google_root_ca = rootCA;
    Serial.println(F("Root CA certificate set for Google Drive client"));
}
#endif
