// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get helloWorld => 'Hello World!';

  @override
  String get connectionLostMessage => 'Connection to device lost';

  @override
  String get deviceNotConnectedMessage => 'Device not connected';

  @override
  String get uploadSuccessTitle => 'Upload Successful';

  @override
  String get uploadSuccessMessage => 'The file has been uploaded to the device successfully.';

  @override
  String get okAction => 'OK';

  @override
  String get newImageAction => 'New Image';

  @override
  String get uploadFailedTitle => 'Upload Failed';

  @override
  String uploadFailedMessage(Object error) {
    return 'Failed to upload file: $error';
  }

  @override
  String shareFailedMessage(Object error) {
    return 'Failed to share file: $error';
  }

  @override
  String get previewTitle => 'Preview';

  @override
  String get shareTooltip => 'Share';

  @override
  String get unableToRenderPreview => 'Unable to render .pfr1 preview';

  @override
  String dimensionsLabel(Object width, Object height) {
    return 'Dimensions: ${width}x$height';
  }

  @override
  String orientationLabel(Object degrees) {
    return 'Orientation: $degrees°';
  }

  @override
  String get retryAction => 'Retry';

  @override
  String uploadingProgress(Object percent) {
    return 'Uploading: $percent%';
  }

  @override
  String get rotateAction => 'Rotate';

  @override
  String get uploadAction => 'Upload';

  @override
  String get photoFrameBinaryShareText => 'Photo Frame Binary';

  @override
  String get imageSelectTitle => 'Image Select';

  @override
  String get deviceInfoTitle => 'Device Information';

  @override
  String get deviceLabel => 'Device';

  @override
  String get displayLabel => 'Display';

  @override
  String get typeLabel => 'Type';

  @override
  String get rotationLabel => 'Rotation';

  @override
  String get batteryLabel => 'Battery';

  @override
  String get pickImageFromGallery => 'Pick Image from Gallery';

  @override
  String get continueAction => 'Continue';

  @override
  String get selectImageFirstMessage => 'Please select an image first';

  @override
  String pickImageFailedMessage(Object error) {
    return 'Failed to pick image: $error';
  }

  @override
  String get rotationUnknown => 'Unknown';

  @override
  String get homeAppBarTitle => 'ESP32 Photo Frame';

  @override
  String get homeTitle => 'Photo Frame App';

  @override
  String get chooseConnectionMethod => 'Choose connection method';

  @override
  String get scanQrTitle => 'Scan QR Code';

  @override
  String get scanQrSubtitle => 'Scan the QR code from your ESP32 display';

  @override
  String get enterManuallyTitle => 'Enter Manually';

  @override
  String get enterManuallySubtitle => 'Enter IP address and port manually';

  @override
  String get testCropTitle => 'Test Crop Screen';

  @override
  String get testCropSubtitle => '[DEV] Skip to crop screen with mock board';

  @override
  String errorMessageWithDetails(Object error) {
    return 'Error: $error';
  }

  @override
  String get permissionRequiredLabel => 'Permission required';

  @override
  String get locationPermissionRequiredMessage => 'Location permission is required to verify WiFi network';

  @override
  String get wifiInfoErrorLabel => 'Error getting WiFi info';

  @override
  String get wifiConnectCorrectNetworkMessage => 'Please connect to the correct WiFi network first';

  @override
  String get connectedReadyMessage => 'Connected successfully! Ready to upload';

  @override
  String connectionFailedMessage(Object error) {
    return 'Connection failed: $error';
  }

  @override
  String get ePaperConnectionTitle => 'E-Paper Connection';

  @override
  String get wifiConnectionTitle => 'WiFi Connection';

  @override
  String get checkingWifiMessage => 'Checking WiFi...';

  @override
  String currentNetworkLabel(Object ssid) {
    return 'Current Network: $ssid';
  }

  @override
  String requiredNetworkLabel(Object ssid) {
    return 'Required Network: $ssid';
  }

  @override
  String networkPrefixRequirement(Object prefix) {
    return 'Network must start with: $prefix';
  }

  @override
  String get wifiSettingsInstruction => 'Open the WiFi settings and connect to the network displayed on your ESP32 screen';

  @override
  String get wifiSettingsAction => 'WiFi Settings';

  @override
  String get refreshAction => 'Refresh';

  @override
  String get connectionSettingsTitle => 'Connection Settings';

  @override
  String get ipAddressLabel => 'IP Address';

  @override
  String get ipAddressRequiredMessage => 'Please enter an IP address';

  @override
  String get ipAddressFormatInvalidMessage => 'Invalid IP address format';

  @override
  String get ipAddressInvalidMessage => 'Invalid IP address';

  @override
  String get portLabel => 'Port';

  @override
  String get portRequiredMessage => 'Please enter a port number';

  @override
  String get portInvalidMessage => 'Invalid port number (1-65535)';

  @override
  String get saveConnectAction => 'Save & Connect';

  @override
  String get notConnectedLabel => 'Not connected';

  @override
  String get cropDataNotReadyMessage => 'Crop data not ready yet';

  @override
  String saveCroppedImageFailedMessage(Object error) {
    return 'Failed to save cropped image: $error';
  }

  @override
  String get cropRotateTitle => 'Crop & Rotate';

  @override
  String get resetAction => 'Reset';

  @override
  String ditheringFailedMessage(Object error) {
    return 'Failed to process image: $error';
  }

  @override
  String get ditheringTitle => 'Dithering & Effects';

  @override
  String get effectsLabel => 'Effects';

  @override
  String get brightnessLabel => 'Brightness';

  @override
  String get contrastLabel => 'Contrast';

  @override
  String get saturationLabel => 'Saturation';

  @override
  String get cancelAction => 'Cancel';

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
  String get reviewTitle => 'Review';

  @override
  String wizardStepTitle(Object title, Object step, Object total) {
    return '$title ($step of $total)';
  }

  @override
  String get generatingPfrMessage => 'Generating .pfr1 file...';

  @override
  String get preparingPfrMessage => 'Preparing .pfr1 file...';

  @override
  String get unableToGeneratePfrMessage => 'Unable to generate .pfr1 file';

  @override
  String fileSavedToGalleryMessage(Object path) {
    return 'File saved to gallery: $path';
  }

  @override
  String get errorSavingFileMessage => 'Error saving file';

  @override
  String get photoframePfrReadyShareText => 'PhotoFrame .pfr1 ready for desktop test';

  @override
  String get sharePfrTooltip => 'Share .pfr1';

  @override
  String get backAction => 'Back';

  @override
  String get nextAction => 'Next';

  @override
  String get finishAction => 'Finish';

  @override
  String get invalidQrFormatMessage => 'Invalid QR code format';

  @override
  String qrProcessFailedMessage(Object error) {
    return 'Failed to process QR code: $error';
  }

  @override
  String get qrScannerInstructionTitle => 'Point your camera at the QR code';

  @override
  String get qrScannerInstructionSubtitle => 'The QR code will be scanned automatically';
}
