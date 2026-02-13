// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Italian (`it`).
class AppLocalizationsIt extends AppLocalizations {
  AppLocalizationsIt([String locale = 'it']) : super(locale);

  @override
  String get helloWorld => 'Ciao mondo!';

  @override
  String get connectionLostMessage => 'Connessione al dispositivo persa';

  @override
  String get deviceNotConnectedMessage => 'Dispositivo non connesso';

  @override
  String get uploadSuccessTitle => 'Upload completato';

  @override
  String get uploadSuccessMessage => 'Il file e stato caricato correttamente sul dispositivo.';

  @override
  String get okAction => 'OK';

  @override
  String get newImageAction => 'Nuova immagine';

  @override
  String get uploadFailedTitle => 'Upload fallito';

  @override
  String uploadFailedMessage(Object error) {
    return 'Caricamento non riuscito: $error';
  }

  @override
  String shareFailedMessage(Object error) {
    return 'Condivisione fallita: $error';
  }

  @override
  String get previewTitle => 'Anteprima';

  @override
  String get shareTooltip => 'Condividi';

  @override
  String get unableToRenderPreview => 'Impossibile visualizzare l\'anteprima .pfr1';

  @override
  String dimensionsLabel(Object width, Object height) {
    return 'Dimensioni: ${width}x$height';
  }

  @override
  String orientationLabel(Object degrees) {
    return 'Orientamento: $degrees°';
  }

  @override
  String get retryAction => 'Riprova';

  @override
  String uploadingProgress(Object percent) {
    return 'Caricamento: $percent%';
  }

  @override
  String get rotateAction => 'Ruota';

  @override
  String get uploadAction => 'Carica';

  @override
  String get photoFrameBinaryShareText => 'File binario Photo Frame';

  @override
  String get imageSelectTitle => 'Selezione immagine';

  @override
  String get deviceInfoTitle => 'Informazioni dispositivo';

  @override
  String get deviceLabel => 'Dispositivo';

  @override
  String get displayLabel => 'Display';

  @override
  String get typeLabel => 'Tipo';

  @override
  String get rotationLabel => 'Rotazione';

  @override
  String get batteryLabel => 'Batteria';

  @override
  String get pickImageFromGallery => 'Scegli immagine dalla galleria';

  @override
  String get continueAction => 'Continua';

  @override
  String get selectImageFirstMessage => 'Seleziona prima un\'immagine';

  @override
  String pickImageFailedMessage(Object error) {
    return 'Selezione immagine fallita: $error';
  }

  @override
  String get rotationUnknown => 'Sconosciuto';

  @override
  String get homeAppBarTitle => 'ESP32 Photo Frame';

  @override
  String get homeTitle => 'App Photo Frame';

  @override
  String get chooseConnectionMethod => 'Scegli il metodo di connessione';

  @override
  String get scanQrTitle => 'Scansiona QR Code';

  @override
  String get scanQrSubtitle => 'Scansiona il QR code dal display ESP32';

  @override
  String get enterManuallyTitle => 'Inserisci manualmente';

  @override
  String get enterManuallySubtitle => 'Inserisci indirizzo IP e porta manualmente';

  @override
  String get testCropTitle => 'Test schermata crop';

  @override
  String get testCropSubtitle => '[DEV] Salta al crop con board fittizia';

  @override
  String errorMessageWithDetails(Object error) {
    return 'Errore: $error';
  }

  @override
  String get permissionRequiredLabel => 'Permesso richiesto';

  @override
  String get locationPermissionRequiredMessage => 'Il permesso posizione e richiesto per verificare la rete WiFi';

  @override
  String get wifiInfoErrorLabel => 'Errore nel leggere info WiFi';

  @override
  String get wifiConnectCorrectNetworkMessage => 'Connettiti prima alla rete WiFi corretta';

  @override
  String get connectedReadyMessage => 'Connesso con successo! Pronto per caricare';

  @override
  String connectionFailedMessage(Object error) {
    return 'Connessione fallita: $error';
  }

  @override
  String get ePaperConnectionTitle => 'Connessione E-Paper';

  @override
  String get wifiConnectionTitle => 'Connessione WiFi';

  @override
  String get checkingWifiMessage => 'Verifica WiFi...';

  @override
  String currentNetworkLabel(Object ssid) {
    return 'Rete attuale: $ssid';
  }

  @override
  String requiredNetworkLabel(Object ssid) {
    return 'Rete richiesta: $ssid';
  }

  @override
  String networkPrefixRequirement(Object prefix) {
    return 'La rete deve iniziare con: $prefix';
  }

  @override
  String get wifiSettingsInstruction => 'Apri le impostazioni WiFi e connettiti alla rete mostrata sullo schermo ESP32';

  @override
  String get wifiSettingsAction => 'Impostazioni WiFi';

  @override
  String get refreshAction => 'Aggiorna';

  @override
  String get connectionSettingsTitle => 'Impostazioni connessione';

  @override
  String get ipAddressLabel => 'Indirizzo IP';

  @override
  String get ipAddressRequiredMessage => 'Inserisci un indirizzo IP';

  @override
  String get ipAddressFormatInvalidMessage => 'Formato indirizzo IP non valido';

  @override
  String get ipAddressInvalidMessage => 'Indirizzo IP non valido';

  @override
  String get portLabel => 'Porta';

  @override
  String get portRequiredMessage => 'Inserisci un numero di porta';

  @override
  String get portInvalidMessage => 'Numero di porta non valido (1-65535)';

  @override
  String get saveConnectAction => 'Salva e connetti';

  @override
  String get notConnectedLabel => 'Non connesso';

  @override
  String get cropDataNotReadyMessage => 'Dati di crop non pronti';

  @override
  String saveCroppedImageFailedMessage(Object error) {
    return 'Salvataggio immagine ritagliata fallito: $error';
  }

  @override
  String get cropRotateTitle => 'Ritaglia e ruota';

  @override
  String get resetAction => 'Reimposta';

  @override
  String ditheringFailedMessage(Object error) {
    return 'Elaborazione immagine fallita: $error';
  }

  @override
  String get ditheringTitle => 'Dithering e effetti';

  @override
  String get effectsLabel => 'Effetti';

  @override
  String get brightnessLabel => 'Luminosita';

  @override
  String get contrastLabel => 'Contrasto';

  @override
  String get saturationLabel => 'Saturazione';

  @override
  String get cancelAction => 'Annulla';

  @override
  String get ditheringMethodFloydSteinberg => 'Floyd-Steinberg';

  @override
  String get ditheringMethodAtkinson => 'Atkinson';

  @override
  String get ditheringMethodStucki => 'Stucki';

  @override
  String get ditheringMethodJarvisJudice => 'Jarvis-Judice';

  @override
  String get ditheringMethodOrdered => 'Ordered';

  @override
  String get colorModeSixColorsLabel => '6C';

  @override
  String get colorModeBlackWhiteLabel => 'BW';

  @override
  String adjustmentValueLabel(Object label, Object value) {
    return '$label: $value';
  }

  @override
  String get reviewTitle => 'Riepilogo';

  @override
  String wizardStepTitle(Object title, Object step, Object total) {
    return '$title ($step di $total)';
  }

  @override
  String get generatingPfrMessage => 'Generazione file .pfr1...';

  @override
  String get preparingPfrMessage => 'Preparazione file .pfr1...';

  @override
  String get unableToGeneratePfrMessage => 'Impossibile generare file .pfr1';

  @override
  String fileSavedToGalleryMessage(Object path) {
    return 'File salvato in galleria: $path';
  }

  @override
  String get errorSavingFileMessage => 'Errore nel salvataggio del file';

  @override
  String get photoframePfrReadyShareText => 'PhotoFrame .pfr1 pronto per test desktop';

  @override
  String get sharePfrTooltip => 'Condividi .pfr1';

  @override
  String get backAction => 'Indietro';

  @override
  String get nextAction => 'Avanti';

  @override
  String get finishAction => 'Fine';

  @override
  String get invalidQrFormatMessage => 'Formato QR code non valido';

  @override
  String qrProcessFailedMessage(Object error) {
    return 'Elaborazione QR code fallita: $error';
  }

  @override
  String get qrScannerInstructionTitle => 'Inquadra il QR code con la fotocamera';

  @override
  String get qrScannerInstructionSubtitle => 'Il QR code verra scansionato automaticamente';

  @override
  String get uploadTitle => 'Carica';
}
