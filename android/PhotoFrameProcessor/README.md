# Photo Frame Processor Android App

This Android app replicates the functionality of the existing shell scripts (`auth.sh`, `auto.sh`, etc.) for processing images for the ESP32 photo frame. The app allows users to:

- Pick images from their phone (single or multiple)
- Receive shared images from other apps
- Process images to 800x480 resolution with grayscale conversion and dithering
- Convert processed images to binary format (.bin files)
- Upload the results to Google Drive

## Features

### Image Processing
- **Automatic resizing**: Images are resized to 800x480 pixels while maintaining aspect ratio
- **Grayscale conversion**: Images are converted to grayscale (matching the 'bw' mode from the original scripts)
- **Floyd-Steinberg dithering**: Applied to reduce color banding and improve display quality
- **EXIF rotation**: Automatically handles image orientation based on EXIF data
- **Binary format**: Converts processed images to RGB565 binary format for the ESP32 display

### User Interface
- Clean, Material Design interface
- Progress indicators for processing and upload
- Image preview with remove functionality
- Real-time status updates

### Google Drive Integration
- OAuth2 authentication with Google Drive
- Automatic folder creation ("PhotoFrameProcessor")
- Batch file upload with progress tracking
- Secure file sharing

### Intent Filters
- Handles single image sharing from other apps
- Handles multiple image sharing
- Can be used as a "Share to" target from gallery apps

## Setup Instructions

### Prerequisites
1. Android Studio Arctic Fox or later
2. Android SDK API 24+ (Android 7.0+)
3. Google Cloud Console project with Drive API enabled

### Google Drive API Setup
1. Go to the [Google Cloud Console](https://console.cloud.google.com/)
2. Create a new project or select an existing one
3. Enable the Google Drive API:
   - Navigate to "APIs & Services" > "Library"
   - Search for "Google Drive API" and enable it
4. Create credentials:
   - Go to "APIs & Services" > "Credentials"
   - Click "Create Credentials" > "OAuth 2.0 Client IDs"
   - Select "Android" as the application type
   - Enter your package name: `it.sephiroth.android.photoframe.processor`
   - Get your SHA-1 certificate fingerprint:
     ```bash
     keytool -list -v -keystore ~/.android/debug.keystore -alias androiddebugkey -storepass android -keypass android
     ```
   - Enter the SHA-1 fingerprint
   - Download the `google-services.json` file (clientId: 912321923458-k29dhnld1png8c7bdpa6cgrek11mkjt6.apps.googleusercontent.com)
5. Place the `google-services.json` file in the `app/` directory

### Building the App
1. Clone this repository
2. Open the `PhotoFrameProcessor` folder in Android Studio
3. Add your `google-services.json` file to the `app/` directory
4. The project uses Gradle Version Catalogs (`libs.versions.toml`) for dependency management
5. Sync the project with Gradle files
6. Build and run the app on your device or emulator

### Permissions
The app requires the following permissions:
- `READ_EXTERNAL_STORAGE`: To access images from the device
- `WRITE_EXTERNAL_STORAGE`: To save processed images (Android 9 and below)
- `INTERNET`: For Google Drive API communication
- `ACCESS_NETWORK_STATE`: To check network connectivity

## Usage

### Processing Images
1. Open the app
2. Tap "Pick Images" to select images from your gallery
3. Or share images to the app from other applications
4. Selected images will appear in the list
5. Tap "Process Images" to start processing
6. Wait for processing to complete

### Uploading to Google Drive
1. After processing is complete, tap "Upload to Drive"
2. Sign in to your Google account when prompted
3. Grant permissions for Drive access
4. Files will be uploaded to a "PhotoFrameProcessor" folder in your Drive
5. Upload progress will be shown in real-time

## Technical Details

### Image Processing Pipeline
1. **Load and decode** the original image from URI
2. **Apply EXIF rotation** to handle device orientation
3. **Resize** to 800x480 with letterboxing (black bars) to maintain aspect ratio
4. **Convert to grayscale** using standard luminance formula
5. **Apply Floyd-Steinberg dithering** for better quality at reduced bit depth
6. **Convert to RGB565 binary format** for ESP32 compatibility
7. **Save as .bin file** ready for upload

### File Format
- Output files are in RGB565 format (2 bytes per pixel)
- Little-endian byte order
- Total file size: 800 × 480 × 2 = 768,000 bytes per image
- Files are named sequentially: `image_0.bin`, `image_1.bin`, etc.

### Architecture
- **MVVM pattern** with Android Architecture Components
- **Kotlin Coroutines** for asynchronous operations
- **ViewBinding** for type-safe view access
- **LiveData** for reactive UI updates
- **Material Design Components** for consistent UI

## Dependencies

The project uses Gradle Version Catalogs (`gradle/libs.versions.toml`) for centralized dependency management. This provides:
- Centralized version management
- Type-safe accessors for dependencies
- Easy version updates across modules
- Better IDE support

### Main Dependencies
- **AndroidX**: Core, AppCompat, Material Design, ConstraintLayout, Activity/Fragment KTX
- **Kotlin Coroutines**: For asynchronous operations
- **Google Play Services Auth**: OAuth2 authentication
- **Google Drive API Client**: Drive API integration
- **Glide**: Image loading and caching
- **ExifInterface**: EXIF data handling for image orientation

### Updating Dependencies
To update dependency versions, modify the `gradle/libs.versions.toml` file in the `[versions]` section.

## Comparison with Shell Scripts

This Android app replicates the functionality of the original shell scripts:

| Shell Script Feature | Android App Equivalent |
|---------------------|------------------------|
| ImageMagick resize & convert | Custom bitmap processing |
| Floyd-Steinberg dithering | Custom dithering algorithm |
| BMP to binary conversion | Direct RGB565 binary output |
| Batch processing | Kotlin coroutines with progress |
| File I/O | Android file system APIs |
| Google Drive upload | Google Drive API v3 |

## Troubleshooting

### Common Issues
1. **Google Sign-in fails**: Ensure SHA-1 fingerprint is correct in Google Cloud Console
2. **Upload fails**: Check internet connection and Google Drive API quotas
3. **Processing fails**: Ensure sufficient storage space and valid image files
4. **Permissions denied**: Grant storage permissions in Android settings

### Debug Information
- Check Android Studio Logcat for detailed error messages
- Ensure Google Services JSON file is properly configured
- Verify Google Drive API is enabled in your Google Cloud project

## License

This project is part of the ESP32 Photo Frame project. Please refer to the main project license.