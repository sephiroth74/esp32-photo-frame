import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_en.dart';
import 'app_localizations_it.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale) : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations? of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations);
  }

  static const LocalizationsDelegate<AppLocalizations> delegate = _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates = <LocalizationsDelegate<dynamic>>[
    delegate,
    GlobalMaterialLocalizations.delegate,
    GlobalCupertinoLocalizations.delegate,
    GlobalWidgetsLocalizations.delegate,
  ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[Locale('en'), Locale('it')];

  /// The conventional newborn programmer greeting
  ///
  /// In en, this message translates to:
  /// **'Hello World!'**
  String get helloWorld;

  /// No description provided for @connectionLostMessage.
  ///
  /// In en, this message translates to:
  /// **'Connection to device lost'**
  String get connectionLostMessage;

  /// No description provided for @deviceNotConnectedMessage.
  ///
  /// In en, this message translates to:
  /// **'Device not connected'**
  String get deviceNotConnectedMessage;

  /// No description provided for @uploadSuccessTitle.
  ///
  /// In en, this message translates to:
  /// **'Upload Successful'**
  String get uploadSuccessTitle;

  /// No description provided for @uploadSuccessMessage.
  ///
  /// In en, this message translates to:
  /// **'The file has been uploaded to the device successfully. Press OK to complete the process and turn off the device.'**
  String get uploadSuccessMessage;

  /// No description provided for @okAction.
  ///
  /// In en, this message translates to:
  /// **'OK'**
  String get okAction;

  /// No description provided for @newImageAction.
  ///
  /// In en, this message translates to:
  /// **'New Image'**
  String get newImageAction;

  /// No description provided for @uploadFailedTitle.
  ///
  /// In en, this message translates to:
  /// **'Upload Failed'**
  String get uploadFailedTitle;

  /// Upload failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to upload file: {error}'**
  String uploadFailedMessage(Object error);

  /// Share failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to share file: {error}'**
  String shareFailedMessage(Object error);

  /// No description provided for @previewTitle.
  ///
  /// In en, this message translates to:
  /// **'Preview'**
  String get previewTitle;

  /// No description provided for @shareTooltip.
  ///
  /// In en, this message translates to:
  /// **'Share'**
  String get shareTooltip;

  /// No description provided for @unableToRenderPreview.
  ///
  /// In en, this message translates to:
  /// **'Unable to render .pfr1 preview'**
  String get unableToRenderPreview;

  /// Image dimensions label
  ///
  /// In en, this message translates to:
  /// **'Dimensions: {width}x{height}'**
  String dimensionsLabel(Object width, Object height);

  /// Image orientation label
  ///
  /// In en, this message translates to:
  /// **'Orientation: {degrees}°'**
  String orientationLabel(Object degrees);

  /// No description provided for @retryAction.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get retryAction;

  /// Upload progress label
  ///
  /// In en, this message translates to:
  /// **'Uploading: {percent}%'**
  String uploadingProgress(Object percent);

  /// No description provided for @rotateAction.
  ///
  /// In en, this message translates to:
  /// **'Rotate'**
  String get rotateAction;

  /// No description provided for @uploadAction.
  ///
  /// In en, this message translates to:
  /// **'Upload'**
  String get uploadAction;

  /// No description provided for @photoFrameBinaryShareText.
  ///
  /// In en, this message translates to:
  /// **'Photo Frame Binary'**
  String get photoFrameBinaryShareText;

  /// No description provided for @imageSelectTitle.
  ///
  /// In en, this message translates to:
  /// **'Image Select'**
  String get imageSelectTitle;

  /// No description provided for @deviceInfoTitle.
  ///
  /// In en, this message translates to:
  /// **'Device Information'**
  String get deviceInfoTitle;

  /// No description provided for @deviceLabel.
  ///
  /// In en, this message translates to:
  /// **'Device'**
  String get deviceLabel;

  /// No description provided for @displayLabel.
  ///
  /// In en, this message translates to:
  /// **'Display'**
  String get displayLabel;

  /// No description provided for @typeLabel.
  ///
  /// In en, this message translates to:
  /// **'Type'**
  String get typeLabel;

  /// No description provided for @rotationLabel.
  ///
  /// In en, this message translates to:
  /// **'Rotation'**
  String get rotationLabel;

  /// No description provided for @batteryLabel.
  ///
  /// In en, this message translates to:
  /// **'Battery'**
  String get batteryLabel;

  /// No description provided for @pickImageFromGallery.
  ///
  /// In en, this message translates to:
  /// **'Pick Image'**
  String get pickImageFromGallery;

  /// No description provided for @continueAction.
  ///
  /// In en, this message translates to:
  /// **'Continue'**
  String get continueAction;

  /// No description provided for @selectImageFirstMessage.
  ///
  /// In en, this message translates to:
  /// **'Please select an image first'**
  String get selectImageFirstMessage;

  /// Image pick failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to pick image: {error}'**
  String pickImageFailedMessage(Object error);

  /// No description provided for @rotationUnknown.
  ///
  /// In en, this message translates to:
  /// **'Unknown'**
  String get rotationUnknown;

  /// No description provided for @homeAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'ESP32 Photo Frame'**
  String get homeAppBarTitle;

  /// No description provided for @homeTitle.
  ///
  /// In en, this message translates to:
  /// **'Photo Frame App'**
  String get homeTitle;

  /// No description provided for @chooseConnectionMethod.
  ///
  /// In en, this message translates to:
  /// **'Connect to your ESP32 Photo Frame by choosing one of the options below'**
  String get chooseConnectionMethod;

  /// No description provided for @scanQrTitle.
  ///
  /// In en, this message translates to:
  /// **'Scan QR Code'**
  String get scanQrTitle;

  /// No description provided for @scanQrSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Scan the QR code from your ESP32 display'**
  String get scanQrSubtitle;

  /// No description provided for @enterManuallyTitle.
  ///
  /// In en, this message translates to:
  /// **'Enter Manually'**
  String get enterManuallyTitle;

  /// No description provided for @enterManuallySubtitle.
  ///
  /// In en, this message translates to:
  /// **'Enter IP address and port manually'**
  String get enterManuallySubtitle;

  /// No description provided for @testCropTitle.
  ///
  /// In en, this message translates to:
  /// **'Test Crop Screen'**
  String get testCropTitle;

  /// No description provided for @testCropSubtitle.
  ///
  /// In en, this message translates to:
  /// **'[DEV] Skip to crop screen with mock board'**
  String get testCropSubtitle;

  /// Error message with details
  ///
  /// In en, this message translates to:
  /// **'Error: {error}'**
  String errorMessageWithDetails(Object error);

  /// No description provided for @permissionRequiredLabel.
  ///
  /// In en, this message translates to:
  /// **'Permission required'**
  String get permissionRequiredLabel;

  /// No description provided for @locationPermissionRequiredMessage.
  ///
  /// In en, this message translates to:
  /// **'Location permission is required to verify WiFi network'**
  String get locationPermissionRequiredMessage;

  /// No description provided for @wifiInfoErrorLabel.
  ///
  /// In en, this message translates to:
  /// **'Error getting WiFi info'**
  String get wifiInfoErrorLabel;

  /// No description provided for @wifiConnectCorrectNetworkMessage.
  ///
  /// In en, this message translates to:
  /// **'Please connect to the correct WiFi network first'**
  String get wifiConnectCorrectNetworkMessage;

  /// No description provided for @connectedReadyMessage.
  ///
  /// In en, this message translates to:
  /// **'Connected successfully! Ready to upload'**
  String get connectedReadyMessage;

  /// Connection failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Connection failed: {error}'**
  String connectionFailedMessage(Object error);

  /// No description provided for @ePaperConnectionTitle.
  ///
  /// In en, this message translates to:
  /// **'E-Paper Connection'**
  String get ePaperConnectionTitle;

  /// No description provided for @wifiConnectionTitle.
  ///
  /// In en, this message translates to:
  /// **'WiFi Connection'**
  String get wifiConnectionTitle;

  /// No description provided for @checkingWifiMessage.
  ///
  /// In en, this message translates to:
  /// **'Checking WiFi...'**
  String get checkingWifiMessage;

  /// Current WiFi network label
  ///
  /// In en, this message translates to:
  /// **'Current Network: {ssid}'**
  String currentNetworkLabel(Object ssid);

  /// Required WiFi network label
  ///
  /// In en, this message translates to:
  /// **'Required Network: {ssid}'**
  String requiredNetworkLabel(Object ssid);

  /// Network prefix requirement
  ///
  /// In en, this message translates to:
  /// **'Network must start with: {prefix}'**
  String networkPrefixRequirement(Object prefix);

  /// No description provided for @wifiSettingsInstruction.
  ///
  /// In en, this message translates to:
  /// **'Open the WiFi settings and connect to the network displayed on your ESP32 screen'**
  String get wifiSettingsInstruction;

  /// No description provided for @wifiSettingsAction.
  ///
  /// In en, this message translates to:
  /// **'WiFi Settings'**
  String get wifiSettingsAction;

  /// No description provided for @refreshAction.
  ///
  /// In en, this message translates to:
  /// **'Refresh'**
  String get refreshAction;

  /// No description provided for @connectionSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Connection Settings'**
  String get connectionSettingsTitle;

  /// No description provided for @ipAddressLabel.
  ///
  /// In en, this message translates to:
  /// **'IP Address'**
  String get ipAddressLabel;

  /// No description provided for @ipAddressRequiredMessage.
  ///
  /// In en, this message translates to:
  /// **'Please enter an IP address'**
  String get ipAddressRequiredMessage;

  /// No description provided for @ipAddressFormatInvalidMessage.
  ///
  /// In en, this message translates to:
  /// **'Invalid IP address format'**
  String get ipAddressFormatInvalidMessage;

  /// No description provided for @ipAddressInvalidMessage.
  ///
  /// In en, this message translates to:
  /// **'Invalid IP address'**
  String get ipAddressInvalidMessage;

  /// No description provided for @portLabel.
  ///
  /// In en, this message translates to:
  /// **'Port'**
  String get portLabel;

  /// No description provided for @portRequiredMessage.
  ///
  /// In en, this message translates to:
  /// **'Please enter a port number'**
  String get portRequiredMessage;

  /// No description provided for @portInvalidMessage.
  ///
  /// In en, this message translates to:
  /// **'Invalid port number (1-65535)'**
  String get portInvalidMessage;

  /// No description provided for @saveConnectAction.
  ///
  /// In en, this message translates to:
  /// **'Save & Connect'**
  String get saveConnectAction;

  /// No description provided for @notConnectedLabel.
  ///
  /// In en, this message translates to:
  /// **'Not connected'**
  String get notConnectedLabel;

  /// No description provided for @cropDataNotReadyMessage.
  ///
  /// In en, this message translates to:
  /// **'Crop data not ready yet'**
  String get cropDataNotReadyMessage;

  /// Cropped image save failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to save cropped image: {error}'**
  String saveCroppedImageFailedMessage(Object error);

  /// No description provided for @cropRotateTitle.
  ///
  /// In en, this message translates to:
  /// **'Crop & Rotate'**
  String get cropRotateTitle;

  /// No description provided for @resetAction.
  ///
  /// In en, this message translates to:
  /// **'Reset'**
  String get resetAction;

  /// Dithering failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to process image: {error}'**
  String ditheringFailedMessage(Object error);

  /// No description provided for @ditheringTitle.
  ///
  /// In en, this message translates to:
  /// **'Dithering & Effects'**
  String get ditheringTitle;

  /// No description provided for @effectsLabel.
  ///
  /// In en, this message translates to:
  /// **'Effects'**
  String get effectsLabel;

  /// No description provided for @brightnessLabel.
  ///
  /// In en, this message translates to:
  /// **'Brightness'**
  String get brightnessLabel;

  /// No description provided for @contrastLabel.
  ///
  /// In en, this message translates to:
  /// **'Contrast'**
  String get contrastLabel;

  /// No description provided for @saturationLabel.
  ///
  /// In en, this message translates to:
  /// **'Saturation'**
  String get saturationLabel;

  /// No description provided for @cancelAction.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get cancelAction;

  /// No description provided for @ditheringMethodFloydSteinberg.
  ///
  /// In en, this message translates to:
  /// **'Floyd-Steinberg'**
  String get ditheringMethodFloydSteinberg;

  /// No description provided for @ditheringMethodAtkinson.
  ///
  /// In en, this message translates to:
  /// **'Atkinson'**
  String get ditheringMethodAtkinson;

  /// No description provided for @ditheringMethodStucki.
  ///
  /// In en, this message translates to:
  /// **'Stucki'**
  String get ditheringMethodStucki;

  /// No description provided for @ditheringMethodJarvisJudice.
  ///
  /// In en, this message translates to:
  /// **'Jarvis-Judice'**
  String get ditheringMethodJarvisJudice;

  /// No description provided for @ditheringMethodOrdered.
  ///
  /// In en, this message translates to:
  /// **'Ordered'**
  String get ditheringMethodOrdered;

  /// No description provided for @colorModeSixColorsLabel.
  ///
  /// In en, this message translates to:
  /// **'6C'**
  String get colorModeSixColorsLabel;

  /// No description provided for @colorModeBlackWhiteLabel.
  ///
  /// In en, this message translates to:
  /// **'BW'**
  String get colorModeBlackWhiteLabel;

  /// Adjustment label with current value
  ///
  /// In en, this message translates to:
  /// **'{label}: {value}'**
  String adjustmentValueLabel(Object label, Object value);

  /// No description provided for @reviewTitle.
  ///
  /// In en, this message translates to:
  /// **'Review'**
  String get reviewTitle;

  /// Wizard step title with progress
  ///
  /// In en, this message translates to:
  /// **'{title} ({step} of {total})'**
  String wizardStepTitle(Object title, Object step, Object total);

  /// No description provided for @generatingPfrMessage.
  ///
  /// In en, this message translates to:
  /// **'Generating .pfr1 file...'**
  String get generatingPfrMessage;

  /// No description provided for @preparingPfrMessage.
  ///
  /// In en, this message translates to:
  /// **'Preparing .pfr1 file...'**
  String get preparingPfrMessage;

  /// No description provided for @unableToGeneratePfrMessage.
  ///
  /// In en, this message translates to:
  /// **'Unable to generate .pfr1 file'**
  String get unableToGeneratePfrMessage;

  /// File saved message with path
  ///
  /// In en, this message translates to:
  /// **'File saved to gallery: {path}'**
  String fileSavedToGalleryMessage(Object path);

  /// No description provided for @errorSavingFileMessage.
  ///
  /// In en, this message translates to:
  /// **'Error saving file'**
  String get errorSavingFileMessage;

  /// No description provided for @photoframePfrReadyShareText.
  ///
  /// In en, this message translates to:
  /// **'PhotoFrame .pfr1 ready for desktop test'**
  String get photoframePfrReadyShareText;

  /// No description provided for @sharePfrTooltip.
  ///
  /// In en, this message translates to:
  /// **'Share .pfr1'**
  String get sharePfrTooltip;

  /// No description provided for @backAction.
  ///
  /// In en, this message translates to:
  /// **'Back'**
  String get backAction;

  /// No description provided for @nextAction.
  ///
  /// In en, this message translates to:
  /// **'Next'**
  String get nextAction;

  /// No description provided for @finishAction.
  ///
  /// In en, this message translates to:
  /// **'Finish'**
  String get finishAction;

  /// No description provided for @invalidQrFormatMessage.
  ///
  /// In en, this message translates to:
  /// **'Invalid QR code format'**
  String get invalidQrFormatMessage;

  /// QR processing failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to process QR code: {error}'**
  String qrProcessFailedMessage(Object error);

  /// No description provided for @qrScannerInstructionTitle.
  ///
  /// In en, this message translates to:
  /// **'Point your camera at the QR code'**
  String get qrScannerInstructionTitle;

  /// No description provided for @qrScannerInstructionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'The QR code will be scanned automatically'**
  String get qrScannerInstructionSubtitle;

  /// Upload screen title
  ///
  /// In en, this message translates to:
  /// **'Upload'**
  String get uploadTitle;

  /// No description provided for @reconnectionFailedTitle.
  ///
  /// In en, this message translates to:
  /// **'Reconnection Failed'**
  String get reconnectionFailedTitle;

  /// Reconnection failed message with error details
  ///
  /// In en, this message translates to:
  /// **'Failed to reconnect: {error}'**
  String reconnectionFailedMessage(Object error);

  /// No description provided for @connectionErrorTitle.
  ///
  /// In en, this message translates to:
  /// **'Connection Error'**
  String get connectionErrorTitle;

  /// No description provided for @shutdownDevice.
  ///
  /// In en, this message translates to:
  /// **'Shutdown Device'**
  String get shutdownDevice;

  /// No description provided for @shutdownDeviceConfirmationMessage.
  ///
  /// In en, this message translates to:
  /// **'Are you sure you want to shutdown the device?'**
  String get shutdownDeviceConfirmationMessage;

  /// No description provided for @confirmAction.
  ///
  /// In en, this message translates to:
  /// **'Confirm'**
  String get confirmAction;
}

class _AppLocalizationsDelegate extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) => <String>['en', 'it'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'en':
      return AppLocalizationsEn();
    case 'it':
      return AppLocalizationsIt();
  }

  throw FlutterError(
    'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
