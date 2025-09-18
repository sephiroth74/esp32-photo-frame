
#include "_locale.h"

const char TXT_NO_ERROR[] PROGMEM = "Nessun errore";
const char TXT_CARD_MOUNT_FAILED[] PROGMEM = "Scheda SD non trovata";
const char TXT_NO_SD_CARD_ATTACHED[] PROGMEM = "Nessuna scheda SD collegata";
const char TXT_UNKNOWN_SD_CARD_TYPE[] PROGMEM = "Tipo di scheda SD sconosciuto";
const char TXT_CARD_OPEN_FILE_FAILED[] PROGMEM = "Apertura del file nella scheda SD fallita";
const char TXT_SD_CARD_FILE_CREATE_FAILED[] PROGMEM = "Creazione del file nella scheda SD fallita";
const char TXT_SD_CARD_DIR_CREATE_FAILED[] PROGMEM = "Creazione della directory nella scheda SD fallita";
const char TXT_SD_CARD_FILE_NOT_FOUND[] PROGMEM = "File non trovato";
const char TXT_SD_CARD_FILE_OPEN_FAILED[] PROGMEM = "Apertura del file della scheda SD fallita";
const char TXT_IMAGE_FORMAT_NOT_SUPPORTED[] PROGMEM = "Formato immagine non supportato";
const char TXT_ERROR_CODE[] PROGMEM = "Codice errore: ";
const char TXT_BATTERY_LEVEL_CRITICAL[] PROGMEM = "Livello batteria critico";
const char TXT_CARD_TOC_OPEN_FILE_FAILED[] PROGMEM = "Apertura del file TOC della scheda fallita";
const char TXT_PREFERENCES_OPEN_FAILED[] PROGMEM = "Apertura delle preferenze fallita";
const char TXT_NO_IMAGES_FOUND[] PROGMEM = "Nessuna immagine trovata";
const char TXT_BATTERY_EMPTY[] PROGMEM = "Batteria scarica";
const char TXT_RTC_MODULE_NOT_FOUND[] PROGMEM = "Modulo RTC non trovato";
const char TXT_JWT_CREATION_FAILED[] PROGMEM = "Creazione JWT fallita";
const char TXT_HTTP_POST_FAILED[] PROGMEM = "Richiesta HTTP POST fallita";
const char TXT_JSON_PARSE_FAILED[] PROGMEM = "Parsing JSON fallito";
const char TXT_TOKEN_MISSING[] PROGMEM = "Token mancante";
const char TXT_FILE_OPEN_FAILED[] PROGMEM = "Apertura del file fallita";
const char TXT_HTTP_CONNECT_FAILED[] PROGMEM = "Connessione HTTP fallita";
const char TXT_HTTP_GET_FAILED[] PROGMEM = "Richiesta HTTP GET fallita";
const char TXT_DOWNLOAD_FAILED[] PROGMEM = "Download fallito";
const char TXT_SSL_CERTIFICATE_LOAD_FAILED[] PROGMEM = "Caricamento certificato SSL fallito";
const char TXT_RATE_LIMIT_TIMEOUT_EXCEEDED[] PROGMEM = "Timeout superato";
const char TXT_WIFI_CREDENTIALS_NOT_FOUND[] PROGMEM = "Credenziali WiFi non trovate";
const char TXT_WIFI_CONNECTION_FAILED[] PROGMEM = "Connessione WiFi fallita";

// Google Drive configuration validation error messages
const char TXT_CONFIG_MISSING_SECTION[] PROGMEM = "Sezione di configurazione mancante";
const char TXT_CONFIG_MISSING_FIELD[] PROGMEM = "Campo obbligatorio mancante";
const char TXT_CONFIG_INVALID_EMAIL[] PROGMEM = "Formato email non valido";
const char TXT_CONFIG_INVALID_PEM_KEY[] PROGMEM = "Formato chiave PEM non valido";
const char TXT_CONFIG_INVALID_PATH[] PROGMEM = "Percorso file non valido";
const char TXT_CONFIG_INVALID_FILENAME[] PROGMEM = "Nome file non valido";
const char TXT_CONFIG_VALUE_OUT_OF_RANGE[] PROGMEM = "Valore fuori dal range consentito";

// ========================================
// COMPREHENSIVE ERROR SYSTEM - ITALIANO
// ========================================

// LittleFS Specific Errors (110-119)  
const char TXT_LITTLEFS_INIT_FAILED[] PROGMEM = "Inizializzazione LittleFS fallita";
const char TXT_LITTLEFS_FILE_CREATE_FAILED[] PROGMEM = "Creazione file LittleFS fallita";
const char TXT_LITTLEFS_FILE_OPEN_FAILED[] PROGMEM = "Apertura file LittleFS fallita";



// OAuth/Authentication Specific Errors (40-49)
const char TXT_OAUTH_TOKEN_EXPIRED[] PROGMEM = "Token di accesso OAuth scaduto";
const char TXT_OAUTH_TOKEN_INVALID[] PROGMEM = "Token di accesso OAuth non valido o malformato";
const char TXT_OAUTH_REFRESH_TOKEN_INVALID[] PROGMEM = "Token di aggiornamento OAuth non valido";
const char TXT_OAUTH_SCOPE_INSUFFICIENT[] PROGMEM = "Token OAuth con permessi insufficienti";
const char TXT_OAUTH_TOKEN_REQUEST_FAILED[] PROGMEM = "Richiesta token OAuth fallita";

// Google Drive API Specific Errors (50-69)
const char TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED[] PROGMEM = "Quota API Google Drive superata";
const char TXT_GOOGLE_DRIVE_API_RATE_LIMITED[] PROGMEM = "Limite velocità API Google Drive superato";
const char TXT_GOOGLE_DRIVE_FILE_NOT_FOUND[] PROGMEM = "File non trovato in Google Drive";
const char TXT_GOOGLE_DRIVE_PERMISSION_DENIED[] PROGMEM = "Permesso negato per risorsa Google Drive";
const char TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED[] PROGMEM = "Quota archiviazione Google Drive superata";
const char TXT_GOOGLE_DRIVE_API_DISABLED[] PROGMEM = "API Google Drive disabilitata per questo progetto";
const char TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR[] PROGMEM = "Errore interno server API Google Drive";
const char TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT[] PROGMEM = "Timeout rete contattando Google Drive";

// HTTP Specific Errors (70-79)
const char TXT_HTTP_UNAUTHORIZED[] PROGMEM = "HTTP 401: Accesso non autorizzato";
const char TXT_HTTP_FORBIDDEN[] PROGMEM = "HTTP 403: Accesso vietato";
const char TXT_HTTP_NOT_FOUND[] PROGMEM = "HTTP 404: Risorsa non trovata";
const char TXT_HTTP_TOO_MANY_REQUESTS[] PROGMEM = "HTTP 429: Troppe richieste";
const char TXT_HTTP_INTERNAL_SERVER_ERROR[] PROGMEM = "HTTP 500: Errore interno server";
const char TXT_HTTP_BAD_GATEWAY[] PROGMEM = "HTTP 502: Gateway non valido";
const char TXT_HTTP_SERVICE_UNAVAILABLE[] PROGMEM = "HTTP 503: Servizio non disponibile";
const char TXT_HTTP_GATEWAY_TIMEOUT[] PROGMEM = "HTTP 504: Timeout gateway";
const char TXT_HTTP_BAD_REQUEST[] PROGMEM = "HTTP 400: Richiesta non valida";

// Image Processing Specific Errors (80-99)
const char TXT_IMAGE_FILE_CORRUPTED[] PROGMEM = "File immagine corrotto o non valido";
const char TXT_IMAGE_FILE_TOO_LARGE[] PROGMEM = "File immagine troppo grande per l'elaborazione";
const char TXT_IMAGE_DIMENSIONS_INVALID[] PROGMEM = "Dimensioni immagine non valide o non supportate";
const char TXT_IMAGE_DIMENSIONS_MISMATCH[] PROGMEM = "Dimensioni immagine non corrispondono alle attese";
const char TXT_IMAGE_COLOR_DEPTH_UNSUPPORTED[] PROGMEM = "Profondità colore immagine non supportata";
const char TXT_IMAGE_PIXEL_DATA_CORRUPTED[] PROGMEM = "Dati pixel corrotti o illeggibili";
const char TXT_IMAGE_FILE_SEEK_FAILED[] PROGMEM = "Posizionamento nel file immagine fallito";
const char TXT_IMAGE_FILE_READ_FAILED[] PROGMEM = "Lettura dati da file immagine fallita";
const char TXT_IMAGE_BUFFER_OVERFLOW[] PROGMEM = "Overflow buffer elaborazione immagine";
const char TXT_IMAGE_DISPLAY_WRITE_FAILED[] PROGMEM = "Scrittura dati pixel su display fallita";
const char TXT_IMAGE_FILE_HEADER_INVALID[] PROGMEM = "Header file immagine non valido o mancante";
const char TXT_IMAGE_FILE_EMPTY[] PROGMEM = "File immagine vuoto o senza contenuto";
const char TXT_IMAGE_FILE_TRUNCATED[] PROGMEM = "File immagine sembra troncato";
const char TXT_IMAGE_MEMORY_ALLOCATION_FAILED[] PROGMEM = "Allocazione memoria per elaborazione immagine fallita";
const char TXT_IMAGE_CONVERSION_FAILED[] PROGMEM = "Conversione dati immagine per display fallita";
const char TXT_IMAGE_RESOLUTION_TOO_HIGH[] PROGMEM = "Risoluzione immagine supera capacità display";
const char TXT_IMAGE_PROCESSING_ABORTED[] PROGMEM = "Elaborazione immagine interrotta per errore";
const char TXT_IMAGE_DIMENSIONS_NOT_PROVIDED[] PROGMEM = "Dimensioni immagine non fornite";

// Battery/Power Errors (160-199)
const char TXT_BATTERY_VOLTAGE_LOW[] PROGMEM = "Tensione batteria sotto soglia sicurezza";
const char TXT_BATTERY_VOLTAGE_UNSTABLE[] PROGMEM = "Tensione batteria instabile o fluttuante";
const char TXT_BATTERY_TEMPERATURE_HIGH[] PROGMEM = "Temperatura batteria troppo alta";
const char TXT_BATTERY_TEMPERATURE_LOW[] PROGMEM = "Temperatura batteria troppo bassa";
const char TXT_BATTERY_AGING_DETECTED[] PROGMEM = "Batteria mostra segni di invecchiamento o degrado";
const char TXT_BATTERY_CALIBRATION_NEEDED[] PROGMEM = "Indicatore batteria necessita calibrazione";
const char TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH[] PROGMEM = "Velocità scarica batteria anormalmente alta";
const char TXT_BATTERY_CHARGE_RATE_TOO_SLOW[] PROGMEM = "Velocità carica batteria troppo lenta";
const char TXT_BATTERY_CAPACITY_REDUCED[] PROGMEM = "Capacità batteria significativamente ridotta";
const char TXT_BATTERY_HEALTH_POOR[] PROGMEM = "Stato salute batteria scadente";

// Charging Specific Errors (170-179)
const char TXT_CHARGING_FAILED[] PROGMEM = "Carica batteria fallita";
const char TXT_CHARGER_NOT_CONNECTED[] PROGMEM = "Caricabatterie non collegato";
const char TXT_CHARGER_INCOMPATIBLE[] PROGMEM = "Caricabatterie incompatibile o insufficiente";
const char TXT_CHARGING_OVERHEAT[] PROGMEM = "Carica interrotta per surriscaldamento";
const char TXT_CHARGING_TIMEOUT[] PROGMEM = "Processo carica scaduto";
const char TXT_CHARGE_CURRENT_TOO_HIGH[] PROGMEM = "Corrente carica pericolosamente alta";
const char TXT_CHARGE_CURRENT_TOO_LOW[] PROGMEM = "Corrente carica troppo bassa per efficienza";
const char TXT_CHARGING_CIRCUIT_FAULT[] PROGMEM = "Guasto circuito carica";
const char TXT_BATTERY_NOT_DETECTED[] PROGMEM = "Batteria non rilevata dal sistema";
const char TXT_BATTERY_AUTHENTICATION_FAILED[] PROGMEM = "Autenticazione batteria fallita";






