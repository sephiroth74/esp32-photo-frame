
#include "_locale.h"

const char* TXT_NO_ERROR = "Nessun errore";
const char* TXT_CARD_MOUNT_FAILED = "Scheda SD non trovata";
const char* TXT_NO_SD_CARD_ATTACHED = "Nessuna scheda SD collegata";
const char* TXT_UNKNOWN_SD_CARD_TYPE = "Tipo di scheda SD sconosciuto";
const char* TXT_CARD_OPEN_FILE_FAILED = "Apertura del file nella scheda SD fallita";
const char* TXT_SD_CARD_FILE_CREATE_FAILED = "Creazione del file nella scheda SD fallita";
const char* TXT_SD_CARD_DIR_CREATE_FAILED = "Creazione della directory nella scheda SD fallita";
const char* TXT_SD_CARD_FILE_NOT_FOUND = "File non trovato";
const char* TXT_SD_CARD_FILE_OPEN_FAILED = "Apertura del file della scheda SD fallita";
const char* TXT_IMAGE_FORMAT_NOT_SUPPORTED = "Formato immagine non supportato";
const char* TXT_ERROR_CODE = "Codice errore: ";
const char* TXT_BATTERY_LEVEL_CRITICAL = "Livello batteria critico";
const char* TXT_CARD_TOC_OPEN_FILE_FAILED = "Apertura del file TOC della scheda fallita";
const char* TXT_PREFERENCES_OPEN_FAILED = "Apertura delle preferenze fallita";
const char* TXT_NO_IMAGES_FOUND = "Nessuna immagine trovata";
const char* TXT_BATTERY_EMPTY = "Batteria scarica";
const char* TXT_RTC_MODULE_NOT_FOUND = "Modulo RTC non trovato";
const char* TXT_JWT_CREATION_FAILED = "Creazione JWT fallita";
const char* TXT_HTTP_POST_FAILED = "Richiesta HTTP POST fallita";
const char* TXT_JSON_PARSE_FAILED = "Parsing JSON fallito";
const char* TXT_TOKEN_MISSING = "Token mancante";
const char* TXT_FILE_OPEN_FAILED = "Apertura del file fallita";
const char* TXT_HTTP_CONNECT_FAILED = "Connessione HTTP fallita";
const char* TXT_HTTP_GET_FAILED = "Richiesta HTTP GET fallita";
const char* TXT_DOWNLOAD_FAILED = "Download fallito";
const char* TXT_SSL_CERTIFICATE_LOAD_FAILED = "Caricamento certificato SSL fallito";
const char* TXT_RATE_LIMIT_TIMEOUT_EXCEEDED = "Timeout superato";
const char* TXT_WIFI_CREDENTIALS_NOT_FOUND = "Credenziali WiFi non trovate";
const char* TXT_WIFI_CONNECTION_FAILED = "Connessione WiFi fallita";

// Google Drive configuration validation error messages
const char* TXT_CONFIG_MISSING_SECTION = "Sezione di configurazione mancante";
const char* TXT_CONFIG_MISSING_FIELD = "Campo obbligatorio mancante";
const char* TXT_CONFIG_INVALID_EMAIL = "Formato email non valido";
const char* TXT_CONFIG_INVALID_PEM_KEY = "Formato chiave PEM non valido";
const char* TXT_CONFIG_INVALID_PATH = "Percorso file non valido";
const char* TXT_CONFIG_INVALID_FILENAME = "Nome file non valido";
const char* TXT_CONFIG_VALUE_OUT_OF_RANGE = "Valore fuori dal range consentito";

// ========================================
// COMPREHENSIVE ERROR SYSTEM - ITALIANO
// ========================================

// SD Card Specific Errors (100-119)
const char* TXT_SD_CARD_WRITE_PROTECTED = "Scheda SD protetta da scrittura";
const char* TXT_SD_CARD_CORRUPTED = "File system della scheda SD corrotto";
const char* TXT_SD_CARD_INSUFFICIENT_SPACE = "Spazio insufficiente sulla scheda SD";
const char* TXT_SD_CARD_SLOW_RESPONSE = "Scheda SD con risposta lenta";
const char* TXT_SD_CARD_READ_ONLY = "Scheda SD in modalità sola lettura";
const char* TXT_SD_CARD_BAD_SECTOR = "Scheda SD con settori danneggiati";
const char* TXT_SD_CARD_SIZE_INVALID = "Dimensione scheda SD non valida";
const char* TXT_SD_CARD_INIT_FAILED = "Inizializzazione scheda SD fallita";
const char* TXT_SD_CARD_VERSION_UNSUPPORTED = "Versione scheda SD non supportata";
const char* TXT_SD_CARD_FILESYSTEM_UNSUPPORTED = "File system scheda SD non supportato";

// Network/WiFi Specific Errors (120-139)
const char* TXT_WIFI_SIGNAL_TOO_WEAK = "Segnale WiFi troppo debole";
const char* TXT_WIFI_AUTHENTICATION_FAILED = "Autenticazione WiFi fallita";
const char* TXT_WIFI_DHCP_FAILED = "Assegnazione DHCP WiFi fallita";
const char* TXT_WIFI_DNS_RESOLUTION_FAILED = "Risoluzione DNS WiFi fallita";
const char* TXT_WIFI_NETWORK_NOT_FOUND = "Rete WiFi non trovata";
const char* TXT_WIFI_PASSWORD_INCORRECT = "Password WiFi non corretta";
const char* TXT_WIFI_CHANNEL_BUSY = "Canale WiFi occupato o congestionato";
const char* TXT_WIFI_FREQUENCY_NOT_SUPPORTED = "Frequenza WiFi non supportata";
const char* TXT_HTTP_REQUEST_TIMEOUT = "Timeout richiesta HTTP";
const char* TXT_HTTP_INVALID_RESPONSE = "Risposta HTTP non valida o malformata";
const char* TXT_SSL_HANDSHAKE_FAILED = "Handshake SSL/TLS fallito";
const char* TXT_NETWORK_INTERFACE_DOWN = "Interfaccia di rete non attiva";
const char* TXT_NETWORK_CONFIG_INVALID = "Configurazione di rete non valida";
const char* TXT_PROXY_CONNECTION_FAILED = "Connessione proxy fallita";
const char* TXT_NETWORK_TIMEOUT_EXCEEDED = "Timeout operazione di rete superato";

// Configuration Specific Errors (140-159)
const char* TXT_CONFIG_FILE_CORRUPTED = "File di configurazione corrotto";
const char* TXT_CONFIG_JSON_SYNTAX_ERROR = "Errore sintassi JSON configurazione";
const char* TXT_CONFIG_VERSION_MISMATCH = "Versione configurazione non compatibile";
const char* TXT_CONFIG_FIELD_TYPE_MISMATCH = "Tipo campo configurazione non corretto";
const char* TXT_CONFIG_ENCRYPTION_KEY_INVALID = "Chiave crittografia configurazione non valida";
const char* TXT_CONFIG_BACKUP_FAILED = "Backup configurazione fallito";
const char* TXT_CONFIG_RESTORE_FAILED = "Ripristino configurazione fallito";
const char* TXT_CONFIG_VALIDATION_FAILED = "Validazione configurazione fallita";
const char* TXT_CONFIG_DEFAULTS_MISSING = "Configurazione predefinita mancante";
const char* TXT_CONFIG_SCHEMA_INVALID = "Schema configurazione non valido";
const char* TXT_CONFIG_ACCESS_DENIED = "Accesso file configurazione negato";
const char* TXT_CONFIG_FORMAT_UNSUPPORTED = "Formato configurazione non supportato";
const char* TXT_CONFIG_SIZE_LIMIT_EXCEEDED = "Limite dimensione configurazione superato";
const char* TXT_CONFIG_DEPENDENCY_MISSING = "Dipendenza configurazione mancante";
const char* TXT_CONFIG_ENVIRONMENT_MISMATCH = "Ambiente configurazione non compatibile";

// OAuth/Authentication Specific Errors (40-49)
const char* TXT_OAUTH_TOKEN_EXPIRED = "Token di accesso OAuth scaduto";
const char* TXT_OAUTH_TOKEN_INVALID = "Token di accesso OAuth non valido o malformato";
const char* TXT_OAUTH_REFRESH_TOKEN_MISSING = "Token di aggiornamento OAuth mancante";
const char* TXT_OAUTH_REFRESH_TOKEN_INVALID = "Token di aggiornamento OAuth non valido";
const char* TXT_OAUTH_SCOPE_INSUFFICIENT = "Token OAuth con permessi insufficienti";
const char* TXT_OAUTH_JWT_SIGNING_FAILED = "Firma JWT per account servizio fallita";
const char* TXT_OAUTH_SERVICE_ACCOUNT_KEY_INVALID = "Chiave privata account servizio non valida";
const char* TXT_OAUTH_TOKEN_REQUEST_FAILED = "Richiesta token OAuth fallita";
const char* TXT_OAUTH_TOKEN_REFRESH_FAILED = "Aggiornamento token OAuth fallito";

// Google Drive API Specific Errors (50-69)
const char* TXT_GOOGLE_DRIVE_API_QUOTA_EXCEEDED = "Quota API Google Drive superata";
const char* TXT_GOOGLE_DRIVE_API_RATE_LIMITED = "Limite velocità API Google Drive superato";
const char* TXT_GOOGLE_DRIVE_FILE_NOT_FOUND = "File non trovato in Google Drive";
const char* TXT_GOOGLE_DRIVE_FOLDER_NOT_FOUND = "Cartella non trovata in Google Drive";
const char* TXT_GOOGLE_DRIVE_PERMISSION_DENIED = "Permesso negato per risorsa Google Drive";
const char* TXT_GOOGLE_DRIVE_STORAGE_QUOTA_EXCEEDED = "Quota archiviazione Google Drive superata";
const char* TXT_GOOGLE_DRIVE_API_DISABLED = "API Google Drive disabilitata per questo progetto";
const char* TXT_GOOGLE_DRIVE_FILE_TOO_BIG = "File troppo grande per il download";
const char* TXT_GOOGLE_DRIVE_FILE_CORRUPTED = "File scaricato sembra corrotto";
const char* TXT_GOOGLE_DRIVE_INVALID_QUERY = "Sintassi query Google Drive non valida";
const char* TXT_GOOGLE_DRIVE_API_INTERNAL_ERROR = "Errore interno server API Google Drive";
const char* TXT_GOOGLE_DRIVE_NETWORK_TIMEOUT = "Timeout rete contattando Google Drive";
const char* TXT_GOOGLE_DRIVE_RESPONSE_MALFORMED = "Risposta malformata da API Google Drive";
const char* TXT_GOOGLE_DRIVE_API_UNAVAILABLE = "API Google Drive temporaneamente non disponibile";
const char* TXT_GOOGLE_DRIVE_DOWNLOAD_INTERRUPTED = "Download file interrotto";
const char* TXT_GOOGLE_DRIVE_METADATA_INVALID = "Metadati file non validi o mancanti";
const char* TXT_GOOGLE_DRIVE_FOLDER_EMPTY = "Cartella Google Drive vuota";
const char* TXT_GOOGLE_DRIVE_API_VERSION_UNSUPPORTED = "Versione API Google Drive non supportata";

// HTTP Specific Errors (70-79)
const char* TXT_HTTP_UNAUTHORIZED = "HTTP 401: Accesso non autorizzato";
const char* TXT_HTTP_FORBIDDEN = "HTTP 403: Accesso vietato";
const char* TXT_HTTP_NOT_FOUND = "HTTP 404: Risorsa non trovata";
const char* TXT_HTTP_TOO_MANY_REQUESTS = "HTTP 429: Troppe richieste";
const char* TXT_HTTP_INTERNAL_SERVER_ERROR = "HTTP 500: Errore interno server";
const char* TXT_HTTP_BAD_GATEWAY = "HTTP 502: Gateway non valido";
const char* TXT_HTTP_SERVICE_UNAVAILABLE = "HTTP 503: Servizio non disponibile";
const char* TXT_HTTP_GATEWAY_TIMEOUT = "HTTP 504: Timeout gateway";
const char* TXT_HTTP_BAD_REQUEST = "HTTP 400: Richiesta non valida";

// Image Processing Specific Errors (80-99)
const char* TXT_IMAGE_FILE_CORRUPTED = "File immagine corrotto o non valido";
const char* TXT_IMAGE_FILE_TOO_LARGE = "File immagine troppo grande per l'elaborazione";
const char* TXT_IMAGE_DIMENSIONS_INVALID = "Dimensioni immagine non valide o non supportate";
const char* TXT_IMAGE_DIMENSIONS_MISMATCH = "Dimensioni immagine non corrispondono alle attese";
const char* TXT_IMAGE_COLOR_DEPTH_UNSUPPORTED = "Profondità colore immagine non supportata";
const char* TXT_IMAGE_PIXEL_DATA_CORRUPTED = "Dati pixel corrotti o illeggibili";
const char* TXT_IMAGE_FILE_SEEK_FAILED = "Posizionamento nel file immagine fallito";
const char* TXT_IMAGE_FILE_READ_FAILED = "Lettura dati da file immagine fallita";
const char* TXT_IMAGE_BUFFER_OVERFLOW = "Overflow buffer elaborazione immagine";
const char* TXT_IMAGE_BUFFER_UNDERFLOW = "Underflow buffer elaborazione immagine";
const char* TXT_IMAGE_RENDER_TIMEOUT = "Timeout rendering immagine";
const char* TXT_IMAGE_DISPLAY_WRITE_FAILED = "Scrittura dati pixel su display fallita";
const char* TXT_IMAGE_FILE_HEADER_INVALID = "Header file immagine non valido o mancante";
const char* TXT_IMAGE_FILE_EMPTY = "File immagine vuoto o senza contenuto";
const char* TXT_IMAGE_FILE_TRUNCATED = "File immagine sembra troncato";
const char* TXT_IMAGE_MEMORY_ALLOCATION_FAILED = "Allocazione memoria per elaborazione immagine fallita";
const char* TXT_IMAGE_PALETTE_INVALID = "Dati palette immagine non validi";
const char* TXT_IMAGE_CONVERSION_FAILED = "Conversione dati immagine per display fallita";
const char* TXT_IMAGE_RESOLUTION_TOO_HIGH = "Risoluzione immagine supera capacità display";
const char* TXT_IMAGE_PROCESSING_ABORTED = "Elaborazione immagine interrotta per errore";

// Battery/Power Errors (160-199)
const char* TXT_BATTERY_VOLTAGE_LOW = "Tensione batteria sotto soglia sicurezza";
const char* TXT_BATTERY_VOLTAGE_UNSTABLE = "Tensione batteria instabile o fluttuante";
const char* TXT_BATTERY_TEMPERATURE_HIGH = "Temperatura batteria troppo alta";
const char* TXT_BATTERY_TEMPERATURE_LOW = "Temperatura batteria troppo bassa";
const char* TXT_BATTERY_AGING_DETECTED = "Batteria mostra segni di invecchiamento o degrado";
const char* TXT_BATTERY_CALIBRATION_NEEDED = "Indicatore batteria necessita calibrazione";
const char* TXT_BATTERY_DISCHARGE_RATE_TOO_HIGH = "Velocità scarica batteria anormalmente alta";
const char* TXT_BATTERY_CHARGE_RATE_TOO_SLOW = "Velocità carica batteria troppo lenta";
const char* TXT_BATTERY_CAPACITY_REDUCED = "Capacità batteria significativamente ridotta";
const char* TXT_BATTERY_HEALTH_POOR = "Stato salute batteria scadente";

// Charging Specific Errors (170-179)
const char* TXT_CHARGING_FAILED = "Carica batteria fallita";
const char* TXT_CHARGER_NOT_CONNECTED = "Caricabatterie non collegato";
const char* TXT_CHARGER_INCOMPATIBLE = "Caricabatterie incompatibile o insufficiente";
const char* TXT_CHARGING_OVERHEAT = "Carica interrotta per surriscaldamento";
const char* TXT_CHARGING_TIMEOUT = "Processo carica scaduto";
const char* TXT_CHARGE_CURRENT_TOO_HIGH = "Corrente carica pericolosamente alta";
const char* TXT_CHARGE_CURRENT_TOO_LOW = "Corrente carica troppo bassa per efficienza";
const char* TXT_CHARGING_CIRCUIT_FAULT = "Guasto circuito carica";
const char* TXT_BATTERY_NOT_DETECTED = "Batteria non rilevata dal sistema";
const char* TXT_BATTERY_AUTHENTICATION_FAILED = "Autenticazione batteria fallita";

// Power Supply Errors (180-189)
const char* TXT_POWER_SUPPLY_INSUFFICIENT = "Alimentazione insufficiente per funzionamento";
const char* TXT_POWER_SUPPLY_UNSTABLE = "Tensione alimentazione instabile";
const char* TXT_POWER_SUPPLY_OVERVOLTAGE = "Tensione alimentazione troppo alta";
const char* TXT_POWER_SUPPLY_UNDERVOLTAGE = "Tensione alimentazione troppo bassa";
const char* TXT_POWER_REGULATOR_FAILED = "Regolatore tensione guasto";
const char* TXT_POWER_SUPPLY_NOISE = "Alimentazione con rumore eccessivo o interferenze";
const char* TXT_POWER_SUPPLY_EFFICIENCY_LOW = "Efficienza alimentazione sotto soglia";
const char* TXT_POWER_SUPPLY_OVERCURRENT = "Protezione sovracorrente alimentazione attivata";
const char* TXT_POWER_SUPPLY_SHORT_CIRCUIT = "Cortocircuito alimentazione rilevato";
const char* TXT_POWER_SUPPLY_DISCONNECTED = "Alimentazione disconnessa";

// Power Management Errors (190-199)
const char* TXT_POWER_SAVING_MODE_ENTER_FAILED = "Ingresso modalità risparmio energetico fallito";
const char* TXT_POWER_SAVING_MODE_EXIT_FAILED = "Uscita modalità risparmio energetico fallita";
const char* TXT_SLEEP_MODE_ACTIVATION_FAILED = "Attivazione modalità sleep fallita";
const char* TXT_WAKEUP_SOURCE_INVALID = "Configurazione sorgente risveglio non valida";
const char* TXT_POWER_CONSUMPTION_TOO_HIGH = "Consumo energetico supera limiti previsti";
const char* TXT_POWER_MANAGER_INIT_FAILED = "Inizializzazione sistema gestione energia fallita";
const char* TXT_CLOCK_FREQUENCY_ERROR = "Regolazione frequenza clock CPU fallita";
const char* TXT_VOLTAGE_SCALING_FAILED = "Scaling dinamico tensione fallito";
const char* TXT_POWER_DOMAIN_ERROR = "Errore controllo dominio alimentazione";
const char* TXT_THERMAL_THROTTLING_ACTIVE = "Throttling termico attivo per surriscaldamento";

// Display Hardware Errors (200-219)
const char* TXT_DISPLAY_INITIALIZATION_FAILED = "Inizializzazione display fallita";
const char* TXT_DISPLAY_DRIVER_ERROR = "Errore comunicazione driver display";
const char* TXT_DISPLAY_SPI_COMM_ERROR = "Errore comunicazione SPI display";
const char* TXT_DISPLAY_BUSY_TIMEOUT = "Timeout segnale busy display";
const char* TXT_DISPLAY_RESET_FAILED = "Sequenza reset display fallita";
const char* TXT_DISPLAY_POWER_ON_FAILED = "Sequenza accensione display fallita";
const char* TXT_DISPLAY_POWER_OFF_FAILED = "Sequenza spegnimento display fallita";
const char* TXT_DISPLAY_WAKEUP_FAILED = "Risveglio display da sleep fallito";
const char* TXT_DISPLAY_COMMAND_ERROR = "Errore esecuzione comando display";
const char* TXT_DISPLAY_HARDWARE_FAULT = "Guasto hardware display rilevato";

// E-Paper Specific Errors (210-229)
const char* TXT_EPAPER_REFRESH_FAILED = "Aggiornamento display e-paper fallito";
const char* TXT_EPAPER_PARTIAL_REFRESH_NOT_SUPPORTED = "Aggiornamento parziale e-paper non supportato";
const char* TXT_EPAPER_GHOSTING_DETECTED = "Artefatti ghosting e-paper rilevati";
const char* TXT_EPAPER_TEMPERATURE_COMPENSATION_FAILED = "Compensazione temperatura e-paper fallita";
const char* TXT_EPAPER_WAVEFORM_ERROR = "Errore selezione waveform e-paper";
const char* TXT_EPAPER_VOLTAGE_REGULATION_ERROR = "Errore regolazione tensione e-paper";
const char* TXT_EPAPER_PIXEL_STUCK_ERROR = "Pixel e-paper permanentemente bloccati";
const char* TXT_EPAPER_CONTRAST_POOR = "Contrasto e-paper scadente o sbiadito";
const char* TXT_EPAPER_REFRESH_TOO_FREQUENT = "Frequenza aggiornamento e-paper troppo alta";
const char* TXT_EPAPER_LIFETIME_EXCEEDED = "Conteggio aggiornamenti e-paper oltre vita utile";

// Display Rendering Errors (230-249)
const char* TXT_DISPLAY_BUFFER_OVERFLOW = "Overflow buffer rendering display";
const char* TXT_DISPLAY_BUFFER_UNDERFLOW = "Underflow buffer rendering display";
const char* TXT_DISPLAY_MEMORY_ALLOCATION_FAILED = "Allocazione memoria display fallita";
const char* TXT_DISPLAY_FRAMEBUFFER_CORRUPTED = "Framebuffer display corrotto";
const char* TXT_DISPLAY_PIXEL_FORMAT_ERROR = "Errore conversione formato pixel display";
const char* TXT_DISPLAY_COLOR_SPACE_ERROR = "Errore conversione spazio colore display";
const char* TXT_DISPLAY_SCALING_ERROR = "Errore scaling immagine display";
const char* TXT_DISPLAY_ROTATION_ERROR = "Errore operazione rotazione display";
const char* TXT_DISPLAY_CLIPPING_ERROR = "Errore clipping contenuto display";
const char* TXT_DISPLAY_RENDERING_TIMEOUT = "Timeout operazione rendering display";

// Display Configuration Errors (250-259)
const char* TXT_DISPLAY_RESOLUTION_MISMATCH = "Configurazione risoluzione display non compatibile";
const char* TXT_DISPLAY_COLOR_DEPTH_UNSUPPORTED = "Profondità colore display non supportata";
const char* TXT_DISPLAY_ORIENTATION_INVALID = "Impostazione orientamento display non valida";
const char* TXT_DISPLAY_REFRESH_RATE_INVALID = "Impostazione frequenza aggiornamento display non valida";
const char* TXT_DISPLAY_GAMMA_CONFIG_ERROR = "Errore configurazione correzione gamma display";
const char* TXT_DISPLAY_BRIGHTNESS_CONTROL_ERROR = "Errore controllo luminosità display";
const char* TXT_DISPLAY_CONTRAST_CONTROL_ERROR = "Errore controllo contrasto display";
const char* TXT_DISPLAY_TIMING_CONFIG_ERROR = "Errore configurazione timing display";
const char* TXT_DISPLAY_MODE_NOT_SUPPORTED = "Modalità display non supportata dall'hardware";
const char* TXT_DISPLAY_CALIBRATION_REQUIRED = "Calibrazione display richiesta";
