package it.sephiroth.android.photoframe.processor

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.ColorMatrix
import android.graphics.ColorMatrixColorFilter
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.RectF
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import androidx.core.graphics.createBitmap
import androidx.core.graphics.scale
import androidx.exifinterface.media.ExifInterface
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.text.SimpleDateFormat
import java.util.Locale

class ImageProcessor(private val context: Context) {
    
    companion object {
        const val TARGET_WIDTH = 800

        const val TARGET_HEIGHT = 480

        @Suppress("unused")
        const val BYTES_PER_PIXEL = 1 // 8-bit color format (RRRGGGBB)

        const val TAG = "ImageProcessor"
    }
    
    private val outputDir = File(context.filesDir, "processed_images")
    private val personDetector = MobileNetPersonDetector(context)
    
    init {
        if (!outputDir.exists()) {
            outputDir.mkdirs()
        }
    }
    
    private fun saveBitmapAsBmp(bitmap: Bitmap, file: File) {
        val width = bitmap.width
        val height = bitmap.height
        val bytesPerPixel = 3 // RGB format
        val rowPadding = (4 - (width * bytesPerPixel) % 4) % 4
        val imageSize = (width * bytesPerPixel + rowPadding) * height
        val fileSize = 54 + imageSize // 54 bytes header + image data
        
        FileOutputStream(file).use { fos ->
            // BMP File Header (14 bytes)
            val fileHeader = ByteBuffer.allocate(14).order(ByteOrder.LITTLE_ENDIAN)
            fileHeader.put("BM".toByteArray()) // Signature
            fileHeader.putInt(fileSize) // File size
            fileHeader.putInt(0) // Reserved
            fileHeader.putInt(54) // Offset to pixel data
            fos.write(fileHeader.array())
            
            // DIB Header (40 bytes - BITMAPINFOHEADER)
            val dibHeader = ByteBuffer.allocate(40).order(ByteOrder.LITTLE_ENDIAN)
            dibHeader.putInt(40) // Header size
            dibHeader.putInt(width) // Width
            dibHeader.putInt(height) // Height
            dibHeader.putShort(1) // Planes
            dibHeader.putShort(24) // Bits per pixel
            dibHeader.putInt(0) // Compression (BI_RGB)
            dibHeader.putInt(imageSize) // Image size
            dibHeader.putInt(2835) // X pixels per meter (72 DPI)
            dibHeader.putInt(2835) // Y pixels per meter (72 DPI)  
            dibHeader.putInt(0) // Colors used
            dibHeader.putInt(0) // Important colors
            fos.write(dibHeader.array())
            
            // Pixel data (bottom-up, BGR format)
            val pixels = IntArray(width * height)
            bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
            
            for (y in height - 1 downTo 0) { // BMP stores rows bottom-up
                for (x in 0 until width) {
                    val pixel = pixels[y * width + x]
                    fos.write(Color.blue(pixel)) // Blue
                    fos.write(Color.green(pixel)) // Green
                    fos.write(Color.red(pixel)) // Red
                }
                // Add row padding
                for (i in 0 until rowPadding) {
                    fos.write(0)
                }
            }
        }
    }
    
    fun processAnalyzedImages(
        analyzedImages: List<ProcessedImage>,
        progressCallback: (Int, Int, File?) -> Unit
    ): List<File> {
        Log.i(TAG, "processAnalyzedImages: Starting processing of ${analyzedImages.size} pre-analyzed images")
        val startTime = System.currentTimeMillis()
        
        // Use the already-analyzed images directly (no need to re-analyze)
        val processedFiles = mutableListOf<File>()
        val validImages = analyzedImages.filter { !it.isInvalid }
        val portraitPairs = validImages.count { it.isPortrait && it.pairedWith != null }
        val landscapeImages = validImages.count { !it.isPortrait }
        
        Log.d(TAG, "processAnalyzedImages: Processing breakdown - ${validImages.size} valid (${portraitPairs} portrait pairs, ${landscapeImages} landscape), ${analyzedImages.size - validImages.size} invalid")
        
        analyzedImages.forEachIndexed { index, image ->
            val imageStartTime = System.currentTimeMillis()
            Log.d(TAG, "processAnalyzedImages: Processing image ${index + 1}/${analyzedImages.size} - ${if (image.isInvalid) "INVALID" else if (image.isPortrait) "PORTRAIT" else "LANDSCAPE"}")
            
            try {
                if (image.isInvalid) {
                    Log.d(TAG, "processAnalyzedImages: Skipping invalid image ${index + 1}")
                    // Skip invalid images
                    progressCallback(index + 1, analyzedImages.size, null)
                } else if (image.isPortrait && image.pairedWith != null) {
                    Log.d(TAG, "processAnalyzedImages: Processing portrait pair ${index + 1}")
                    // Process combined portrait pair
                    val result = processCombinedPortraitImages(image, image.pairedWith, index)
                    val processingTime = System.currentTimeMillis() - imageStartTime
                    
                    if (result != null) {
                        processedFiles.add(result.binaryFile)
                        Log.i(TAG, "processAnalyzedImages: Portrait pair ${index + 1} processed successfully in ${processingTime}ms - Binary: ${result.binaryFile.name}, Preview: ${result.previewFile.name}")
                        progressCallback(index + 1, analyzedImages.size, result.previewFile)
                    } else {
                        Log.w(TAG, "processAnalyzedImages: Portrait pair ${index + 1} processing failed after ${processingTime}ms")
                        progressCallback(index + 1, analyzedImages.size, null)
                    }
                } else if (!image.isPortrait) {
                    Log.d(TAG, "processAnalyzedImages: Processing landscape image ${index + 1}")
                    // Process landscape image with smart cropping if person detected
                    val result = processImageWithPersonDetection(image, index)
                    val processingTime = System.currentTimeMillis() - imageStartTime
                    
                    if (result != null) {
                        processedFiles.add(result.binaryFile)
                        Log.i(TAG, "processAnalyzedImages: Landscape image ${index + 1} processed successfully in ${processingTime}ms - Binary: ${result.binaryFile.name}, Preview: ${result.previewFile.name}")
                        progressCallback(index + 1, analyzedImages.size, result.previewFile)
                    } else {
                        Log.w(TAG, "processAnalyzedImages: Landscape image ${index + 1} processing failed after ${processingTime}ms")
                        progressCallback(index + 1, analyzedImages.size, null)
                    }
                } else {
                    // This should not happen if pairing logic is correct
                    Log.w(TAG, "processAnalyzedImages: Unexpected case for image ${index + 1} - portrait without pair")
                    progressCallback(index + 1, analyzedImages.size, null)
                }
            } catch (e: Exception) {
                val processingTime = System.currentTimeMillis() - imageStartTime
                Log.e(TAG, "processAnalyzedImages: Error processing image ${index + 1} after ${processingTime}ms", e)
                progressCallback(index + 1, analyzedImages.size, null)
            }
        }
        
        val totalTime = System.currentTimeMillis() - startTime
        Log.i(TAG, "processAnalyzedImages: Completed processing in ${totalTime}ms - ${processedFiles.size} files generated from ${validImages.size} valid images")
        
        return processedFiles
    }
    
    // Keep the old method for backward compatibility with URIs
    @Suppress("unused")
    fun processImages(
        imageUris: List<Uri>,
        progressCallback: (Int, Int, File?) -> Unit
    ): List<File> {
        // First, analyze all images to determine orientation and pair portraits
        val analyzedImages = analyzeAndPairImages(imageUris)
        return processAnalyzedImages(analyzedImages, progressCallback)
    }
    
    fun analyzeImagesForPreview(imageUris: List<Uri>): List<ProcessedImage> {
        Log.d(TAG, "analyzeImagesForPreview: Analyzing ${imageUris.size} images for preview")
        val result = analyzeAndPairImages(imageUris)
        
        // Log the analysis results
        Log.d(TAG, "analyzeImagesForPreview: Analysis results:")
        result.forEachIndexed { index, image ->
            Log.d(TAG, "  [$index] URI: ${image.originalUri}, Portrait: ${image.isPortrait}, Invalid: ${image.isInvalid}, PairedWith: ${image.pairedWith?.originalUri}")
        }
        
        return result
    }
    
    private fun analyzeAndPairImages(imageUris: List<Uri>): List<ProcessedImage> {
        Log.d(TAG, "analyzeAndPairImages: Starting analysis and pairing of ${imageUris.size} images")
        
        // First pass: analyze all images to determine orientation and detect persons
        val analyzedImages = imageUris.map { uri ->
            val imageInfo = getImageInfo(uri)
            val personDetection = detectPersonInImage(uri)
            
            ProcessedImage(
                originalUri = uri,
                isPortrait = imageInfo.isPortrait,
                width = imageInfo.width,
                height = imageInfo.height,
                personDetectionResult = personDetection,
                hasMainPerson = personDetection?.mainPerson != null
            )
        }
        
        Log.d(TAG, "analyzeAndPairImages: First pass analysis complete:")
        analyzedImages.forEachIndexed { index, image ->
            Log.d(TAG, "  [$index] URI: ${image.originalUri}, Portrait: ${image.isPortrait}")
        }
        
        // Second pass: pair portrait images
        val results = mutableListOf<ProcessedImage>()
        val portraitImages = analyzedImages.filter { it.isPortrait }.toMutableList()
        val landscapeImages = analyzedImages.filter { !it.isPortrait }
        
        Log.d(TAG, "analyzeAndPairImages: Found ${landscapeImages.size} landscape and ${portraitImages.size} portrait images")
        
        // Process landscape images - they go through as-is
        results.addAll(landscapeImages)
        Log.d(TAG, "analyzeAndPairImages: Added ${landscapeImages.size} landscape images to results")
        
        // Pair portrait images
        var pairCount = 0
        while (portraitImages.size >= 2) {
            val first = portraitImages.removeAt(0)
            val second = portraitImages.removeAt(0)
            pairCount++
            
            Log.d(TAG, "analyzeAndPairImages: Creating pair $pairCount: ${first.originalUri} <-> ${second.originalUri}")
            
            // Create paired images
            val pairedFirst = first.copy(pairedWith = second)
            @Suppress("UnusedVariable") val pairedSecond = second.copy(pairedWith = first)
            
            // Add only the first image to results (it represents the pair)
            results.add(pairedFirst)
            Log.d(TAG, "analyzeAndPairImages: Added paired portrait ${first.originalUri} to results (represents pair)")
        }
        
        // Handle leftover portrait images - mark as invalid
        if (portraitImages.isNotEmpty()) {
            Log.d(TAG, "analyzeAndPairImages: ${portraitImages.size} portrait images cannot be paired")
            portraitImages.forEach { leftoverImage ->
                Log.d(TAG, "analyzeAndPairImages: Marking ${leftoverImage.originalUri} as invalid (unpaired)")
                results.add(leftoverImage.copy(
                    isInvalid = true,
                    invalidReason = "Portrait image cannot be paired - requires even number of portrait images"
                ))
            }
        }
        
        Log.d(TAG, "analyzeAndPairImages: Final results - ${results.size} items (representing ${imageUris.size} original images)")
        return results
    }
    
    private fun detectPersonInImage(uri: Uri): PersonDetectionResult? {
        return try {
            Log.d(TAG, "detectPersonInImage: Starting detection for $uri")
            
            // Load and apply EXIF rotation to get the actual final dimensions
            val inputStream = context.contentResolver.openInputStream(uri) ?: return null
            val originalBitmap = BitmapFactory.decodeStream(inputStream)
            inputStream.close()
            
            if (originalBitmap == null) return null
            
            // Apply EXIF rotation to get the final orientation
            val rotatedBitmap = applyExifRotation(uri, originalBitmap)
            val finalWidth = rotatedBitmap.width
            val finalHeight = rotatedBitmap.height
            
            Log.d(TAG, "detectPersonInImage: Final rotated dimensions: ${finalWidth}x${finalHeight}")
            
            // Create preview from the already-rotated image
            val previewBitmap = createPreviewFromBitmap(rotatedBitmap, 400)
            val previewWidth = previewBitmap.width
            val previewHeight = previewBitmap.height
            
            Log.d(TAG, "detectPersonInImage: Preview dimensions: ${previewWidth}x${previewHeight}")
            Log.d(TAG, "detectPersonInImage: Scale factors will be: ${finalWidth.toFloat() / previewWidth}x, ${finalHeight.toFloat() / previewHeight}y")
            
            val result = personDetector.detectPersons(previewBitmap)
            
            // Clean up bitmaps
            previewBitmap.recycle()
            if (rotatedBitmap != originalBitmap) {
                rotatedBitmap.recycle()
            }
            if (originalBitmap != rotatedBitmap) {
                originalBitmap.recycle()
            }
            
            if (result != null && result.detections.isNotEmpty()) {
                Log.d(TAG, "detectPersonInImage: Raw detection results (preview coordinates):")
                result.detections.forEachIndexed { index, detection ->
                    Log.d(TAG, "detectPersonInImage: Detection $index: ${detection.boundingBox} (confidence: ${detection.confidence})")
                }
            }
            
            // Scale bounding boxes from preview coordinates to final rotated image coordinates
            val scaledResult = result?.let { scaleDetectionResult(it, previewWidth, previewHeight, finalWidth, finalHeight) }
            
            if (scaledResult != null && scaledResult.detections.isNotEmpty()) {
                Log.d(TAG, "detectPersonInImage: Scaled detection results (final rotated coordinates):")
                scaledResult.detections.forEachIndexed { index, detection ->
                    Log.d(TAG, "detectPersonInImage: Scaled detection $index: ${detection.boundingBox} (confidence: ${detection.confidence})")
                }
            }
            
            Log.d(TAG, "detectPersonInImage: Completed for $uri: ${scaledResult?.detections?.size ?: 0} persons found")
            scaledResult
        } catch (e: Exception) {
            Log.e(TAG, "detectPersonInImage: Error: ${e.message}")
            null
        }
    }
    
    @Suppress("SameParameterValue")
    private fun createPreviewFromBitmap(bitmap: Bitmap, maxDimension: Int): Bitmap {
        val maxOriginalDimension = maxOf(bitmap.width, bitmap.height)
        
        return if (maxOriginalDimension > maxDimension) {
            val scale = maxDimension.toFloat() / maxOriginalDimension
            val newWidth = (bitmap.width * scale).toInt()
            val newHeight = (bitmap.height * scale).toInt()
            
            bitmap.scale(newWidth, newHeight)
        } else {
            // Return a copy if no scaling needed
            bitmap.copy(bitmap.config ?: Bitmap.Config.ARGB_8888, false)
        }
    }
    
    private fun scaleDetectionResult(
        result: PersonDetectionResult,
        previewWidth: Int,
        previewHeight: Int,
        originalWidth: Int,
        originalHeight: Int
    ): PersonDetectionResult {
        
        val scaleX = originalWidth.toFloat() / previewWidth.toFloat()
        val scaleY = originalHeight.toFloat() / previewHeight.toFloat()
        
        Log.d(TAG, "scaleDetectionResult: Scaling coordinates by ${scaleX}x, ${scaleY}y")
        
        // Scale all detections
        val scaledDetections = result.detections.map { detection ->
            val originalBox = detection.boundingBox
            val scaledBox = RectF(
                originalBox.left * scaleX,
                originalBox.top * scaleY,
                originalBox.right * scaleX,
                originalBox.bottom * scaleY
            )
            
            Log.v(TAG, "scaleDetectionResult: Scaled box from $originalBox to $scaledBox")
            
            Detection(scaledBox, detection.confidence, detection.classId)
        }
        
        // Scale the main person detection if it exists
        val scaledMainPerson = result.mainPerson?.let { mainPerson ->
            val originalBox = mainPerson.boundingBox
            val scaledBox = RectF(
                originalBox.left * scaleX,
                originalBox.top * scaleY,
                originalBox.right * scaleX,
                originalBox.bottom * scaleY
            )
            
            Log.d(TAG, "scaleDetectionResult: Main person scaled from $originalBox to $scaledBox")
            
            Detection(scaledBox, mainPerson.confidence, mainPerson.classId)
        }
        
        return PersonDetectionResult(scaledDetections, scaledMainPerson, result.processingTimeMs)
    }
    
    @Suppress("unused")
    private fun loadBitmapFromUri(uri: Uri): Bitmap? {
        return try {
            val inputStream = context.contentResolver.openInputStream(uri) ?: return null
            val bitmap = BitmapFactory.decodeStream(inputStream)
            inputStream.close()
            bitmap
        } catch (e: Exception) {
            Log.e(TAG, "Error loading bitmap from URI: ${e.message}")
            null
        }
    }
    
    @Suppress("unused")
    private fun loadBitmapFromUri(uri: Uri, maxDimension: Int): Bitmap? {
        return try {
            // First, get image dimensions without loading the full bitmap
            val options = BitmapFactory.Options().apply {
                inJustDecodeBounds = true
            }
            
            val inputStream1 = context.contentResolver.openInputStream(uri) ?: return null
            BitmapFactory.decodeStream(inputStream1, null, options)
            inputStream1.close()
            
            val width = options.outWidth
            val height = options.outHeight
            
            // Calculate sample size to get image close to maxDimension
            val maxOriginalDimension = maxOf(width, height)
            var sampleSize = 1
            if (maxOriginalDimension > maxDimension) {
                sampleSize = maxOriginalDimension / maxDimension
            }
            
            // Load the sampled bitmap
            val loadOptions = BitmapFactory.Options().apply {
                inSampleSize = sampleSize
            }
            
            val inputStream2 = context.contentResolver.openInputStream(uri) ?: return null
            val bitmap = BitmapFactory.decodeStream(inputStream2, null, loadOptions)
            inputStream2.close()
            
            // Fine-tune the size to exactly maxDimension if needed
            if (bitmap != null) {
                val currentMaxDimension = maxOf(bitmap.width, bitmap.height)
                if (currentMaxDimension > maxDimension) {
                    val scale = maxDimension.toFloat() / currentMaxDimension
                    val newWidth = (bitmap.width * scale).toInt()
                    val newHeight = (bitmap.height * scale).toInt()
                    
                    val scaledBitmap = bitmap.scale(newWidth, newHeight)
                    if (scaledBitmap != bitmap) {
                        bitmap.recycle()
                    }
                    return scaledBitmap
                }
            }
            
            bitmap
        } catch (e: Exception) {
            Log.e(TAG, "Error loading bitmap from URI with max dimension: ${e.message}")
            null
        }
    }
    
    private data class ImageInfo(
        val width: Int,
        val height: Int,
        val isPortrait: Boolean
    )
    
    private fun getImageInfo(uri: Uri): ImageInfo {
        try {
            val inputStream = context.contentResolver.openInputStream(uri) ?: return ImageInfo(0, 0, false)
            val options = BitmapFactory.Options().apply {
                inJustDecodeBounds = true
            }
            BitmapFactory.decodeStream(inputStream, null, options)
            inputStream.close()
            
            var width = options.outWidth
            var height = options.outHeight
            
            // Check EXIF orientation to determine if dimensions need swapping
            val exifInputStream = context.contentResolver.openInputStream(uri)
            if (exifInputStream != null) {
                val exif = ExifInterface(exifInputStream)
                val orientation = exif.getAttributeInt(
                    ExifInterface.TAG_ORIENTATION,
                    ExifInterface.ORIENTATION_NORMAL
                )
                exifInputStream.close()
                
                // If image is rotated 90 or 270 degrees, swap dimensions
                if (orientation == ExifInterface.ORIENTATION_ROTATE_90 || 
                    orientation == ExifInterface.ORIENTATION_ROTATE_270) {
                    val temp = width
                    width = height
                    height = temp
                }
            }
            
            val isPortrait = height > width
            return ImageInfo(width, height, isPortrait)
            
        } catch (e: Exception) {
            e.printStackTrace()
            return ImageInfo(0, 0, false)
        }
    }
    
    private fun generateSafeFileName(uri: Uri): String {
        // Extract filename from URI or generate one from URI hash
        val fileName = try {
            uri.lastPathSegment?.substringAfterLast('/')?.substringBeforeLast('.') 
                ?: uri.toString().hashCode().toString()
        } catch (_: Exception) {
            uri.toString().hashCode().toString()
        }
        
        // Sanitize filename - remove special characters and limit length
        return fileName.replace(Regex("[^a-zA-Z0-9_-]"), "_")
            .take(50) // Limit length to avoid filesystem issues
    }
    
    private fun generateCombinedFileName(firstUri: Uri, secondUri: Uri): String {
        val firstName = generateSafeFileName(firstUri)
        val secondName = generateSafeFileName(secondUri)
        return "${firstName}_${secondName}".take(100) // Combined name with length limit
    }
    
    private fun processCombinedPortraitImages(
        first: ProcessedImage,
        second: ProcessedImage,
        @Suppress("unused") index: Int
    ): ProcessingResult? {
        try {
            // Extract labels (dates or filenames) from both images
            val firstLabelText = extractImageLabel(first.originalUri)
            val secondLabelText = extractImageLabel(second.originalUri)
            
            if (firstLabelText != null) {
                Log.d(TAG, "processCombinedPortraitImages: First image label: $firstLabelText")
            } else {
                Log.d(TAG, "processCombinedPortraitImages: No label available for first image")
            }
            
            if (secondLabelText != null) {
                Log.d(TAG, "processCombinedPortraitImages: Second image label: $secondLabelText")
            } else {
                Log.d(TAG, "processCombinedPortraitImages: No label available for second image")
            }
            
            // Load both bitmaps
            val firstBitmap = loadAndRotateBitmap(first.originalUri) ?: return null
            val secondBitmap = loadAndRotateBitmap(second.originalUri) ?: return null
            
            // Resize both portraits to half-width to fit side by side, using smart cropping if persons detected
            val targetPortraitWidth = TARGET_WIDTH / 2
            val resizedFirst = resizeBitmapWithPersonDetection(
                firstBitmap, targetPortraitWidth, TARGET_HEIGHT, first.personDetectionResult
            )
            val resizedSecond = resizeBitmapWithPersonDetection(
                secondBitmap, targetPortraitWidth, TARGET_HEIGHT, second.personDetectionResult
            )
            
            // Add labels to individual portraits if available
            val firstWithLabel = if (firstLabelText != null) {
                Log.d(TAG, "processCombinedPortraitImages: Adding label to first image: $firstLabelText")
                drawDateLabel(resizedFirst, firstLabelText, DateLabelPosition.BOTTOM_RIGHT)
            } else {
                resizedFirst
            }
            
            val secondWithLabel = if (secondLabelText != null) {
                Log.d(TAG, "processCombinedPortraitImages: Adding label to second image: $secondLabelText")
                drawDateLabel(resizedSecond, secondLabelText, DateLabelPosition.BOTTOM_RIGHT)
            } else {
                resizedSecond
            }
            
            // Combine images horizontally
            val combinedBitmap = combineImagesHorizontally(firstWithLabel, secondWithLabel)
            
            // Convert to grayscale and apply Floyd-Steinberg dithering
            val grayscaleBitmap = convertToGrayscale(combinedBitmap)
            val ditheredBitmap = applyFloydSteinbergDithering(grayscaleBitmap)
            
            // Convert to 8-bit color format
            val binaryData = convertTo8BitColor(ditheredBitmap)
            
            // Generate unique filename for combined images
            val combinedFileName = generateCombinedFileName(first.originalUri, second.originalUri)
            
            // Save as .bin file
            val outputFile = File(outputDir, "combined_${combinedFileName}.bin")
            FileOutputStream(outputFile).use { fos ->
                fos.write(binaryData)
            }
            
            // Save preview as BMP file
            val previewFile = File(outputDir, "preview_combined_${combinedFileName}.bmp")
            saveBitmapAsBmp(ditheredBitmap, previewFile)
            
            // Cleanup bitmaps
            firstBitmap.recycle()
            secondBitmap.recycle()
            resizedFirst.recycle()
            resizedSecond.recycle()
            if (firstWithLabel != resizedFirst) firstWithLabel.recycle()
            if (secondWithLabel != resizedSecond) secondWithLabel.recycle()
            combinedBitmap.recycle()
            grayscaleBitmap.recycle()
            ditheredBitmap.recycle()
            
            return ProcessingResult(outputFile, previewFile)
            
        } catch (e: Exception) {
            e.printStackTrace()
            return null
        }
    }
    
    private fun loadAndRotateBitmap(uri: Uri): Bitmap? {
        try {
            val inputStream = context.contentResolver.openInputStream(uri) ?: return null
            val originalBitmap = BitmapFactory.decodeStream(inputStream)
            inputStream.close()
            
            if (originalBitmap == null) return null
            
            return applyExifRotation(uri, originalBitmap)
        } catch (_: Exception) {
            return null
        }
    }
    
    private fun combineImagesHorizontally(leftBitmap: Bitmap, rightBitmap: Bitmap): Bitmap {
        val combinedWidth = leftBitmap.width + rightBitmap.width
        val combinedHeight = maxOf(leftBitmap.height, rightBitmap.height)
        
        val combinedBitmap = createBitmap(combinedWidth, combinedHeight)
        val canvas = Canvas(combinedBitmap)
        
        // Fill background with black
        canvas.drawColor(Color.BLACK)
        
        // Draw left image
        canvas.drawBitmap(leftBitmap, 0f, 0f, null)
        
        // Draw right image
        canvas.drawBitmap(rightBitmap, leftBitmap.width.toFloat(), 0f, null)
        
        // Draw dividing line (matching the shell script behavior)
        val paint = Paint().apply {
            color = Color.WHITE
            strokeWidth = 3f
        }
        val centerX = leftBitmap.width.toFloat()
        canvas.drawLine(centerX, 0f, centerX, combinedHeight.toFloat(), paint)
        
        return combinedBitmap
    }
    
    private fun processImageWithPersonDetection(processedImage: ProcessedImage, index: Int): ProcessingResult? {
        return processImage(processedImage.originalUri, index, processedImage.personDetectionResult)
    }
    
    private fun processImage(uri: Uri, @Suppress("unused") index: Int, personDetectionResult: PersonDetectionResult? = null): ProcessingResult? {
        try {
            Log.d(TAG, "processImage: Starting processing for $uri")
            
            // Extract label (date or filename) before processing
            val labelText = extractImageLabel(uri)
            if (labelText != null) {
                Log.d(TAG, "processImage: Extracted label: $labelText")
            } else {
                Log.d(TAG, "processImage: No label available (no date or filename)")
            }
            
            // Load bitmap from URI
            val inputStream = context.contentResolver.openInputStream(uri) ?: return null
            val originalBitmap = BitmapFactory.decodeStream(inputStream)
            inputStream.close()
            
            if (originalBitmap == null) return null
            
            Log.d(TAG, "processImage: Loaded original bitmap: ${originalBitmap.width}x${originalBitmap.height}")
            
            // Apply EXIF rotation
            val rotatedBitmap = applyExifRotation(uri, originalBitmap)
            
            Log.d(TAG, "processImage: After EXIF rotation: ${rotatedBitmap.width}x${rotatedBitmap.height}")
            
            if (personDetectionResult?.mainPerson != null) {
                Log.d(TAG, "processImage: Using person detection for smart cropping")
                Log.d(TAG, "processImage: Person bounding box: ${personDetectionResult.mainPerson.boundingBox}")
            } else {
                Log.d(TAG, "processImage: No person detection, using center crop")
            }
            
            // Resize bitmap to target dimensions using smart cropping if person detected
            val resizedBitmap = resizeBitmapWithPersonDetection(
                rotatedBitmap, TARGET_WIDTH, TARGET_HEIGHT, personDetectionResult
            )
            
            // Add label if date or filename was found
            val bitmapWithLabel = if (labelText != null) {
                Log.d(TAG, "processImage: Adding label: $labelText")
                drawDateLabel(resizedBitmap, labelText, DateLabelPosition.BOTTOM_RIGHT)
            } else {
                resizedBitmap
            }
            
            // Convert to grayscale and apply Floyd-Steinberg dithering (matching convert.sh --type grey)
            val grayscaleBitmap = convertToGrayscale(bitmapWithLabel)
            val ditheredBitmap = applyFloydSteinbergDithering(grayscaleBitmap)
            
            // Convert to 8-bit color format (RRRGGGBB) matching bmp2cpp
            val binaryData = convertTo8BitColor(ditheredBitmap)
            
            // Generate unique filename based on URI
            val safeFileName = generateSafeFileName(uri)
            
            // Save as .bin file
            val outputFile = File(outputDir, "image_${safeFileName}.bin")
            FileOutputStream(outputFile).use { fos ->
                fos.write(binaryData)
            }
            
            // Save preview as BMP file
            val previewFile = File(outputDir, "preview_${safeFileName}.bmp")
            saveBitmapAsBmp(ditheredBitmap, previewFile)
            
            // Cleanup bitmaps
            originalBitmap.recycle()
            if (rotatedBitmap != originalBitmap) rotatedBitmap.recycle()
            resizedBitmap.recycle()
            if (bitmapWithLabel != resizedBitmap) bitmapWithLabel.recycle()
            grayscaleBitmap.recycle()
            ditheredBitmap.recycle()
            
            return ProcessingResult(outputFile, previewFile)
            
        } catch (e: Exception) {
            e.printStackTrace()
            return null
        }
    }
    
    private fun applyExifRotation(uri: Uri, bitmap: Bitmap): Bitmap {
        try {
            val inputStream = context.contentResolver.openInputStream(uri) ?: return bitmap
            val exif = ExifInterface(inputStream)
            inputStream.close()
            
            val orientation = exif.getAttributeInt(
                ExifInterface.TAG_ORIENTATION,
                ExifInterface.ORIENTATION_NORMAL
            )
            
            val matrix = Matrix()
            when (orientation) {
                ExifInterface.ORIENTATION_ROTATE_90 -> matrix.postRotate(90f)
                ExifInterface.ORIENTATION_ROTATE_180 -> matrix.postRotate(180f)
                ExifInterface.ORIENTATION_ROTATE_270 -> matrix.postRotate(270f)
                else -> return bitmap
            }
            
            return Bitmap.createBitmap(bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true)
        } catch (_: Exception) {
            return bitmap
        }
    }
    
    private fun extractExifDate(uri: Uri): String? {
        try {
            val inputStream = context.contentResolver.openInputStream(uri) ?: return null
            val exif = ExifInterface(inputStream)
            inputStream.close()
            
            // Try multiple EXIF date tags in order of preference
            val dateString = exif.getAttribute(ExifInterface.TAG_DATETIME_ORIGINAL)
                ?: exif.getAttribute(ExifInterface.TAG_DATETIME)
                ?: exif.getAttribute(ExifInterface.TAG_DATETIME_DIGITIZED)
            
            if (dateString != null) {
                try {
                    // EXIF date format: "YYYY:MM:DD HH:MM:SS"
                    val exifFormat = SimpleDateFormat("yyyy:MM:dd HH:mm:ss", Locale.US)
                    val date = exifFormat.parse(dateString)
                    
                    // Convert to display format: "YYYY/MM/DD"
                    val displayFormat = SimpleDateFormat("yyyy/MM/dd", Locale.US)
                    return date?.let { displayFormat.format(it) }
                } catch (e: Exception) {
                    Log.w(TAG, "extractExifDate: Error parsing date string '$dateString': ${e.message}")
                }
            }
            
            return null
        } catch (e: Exception) {
            Log.w(TAG, "extractExifDate: Error extracting EXIF date from $uri: ${e.message}")
            return null
        }
    }
    
    private fun extractImageLabel(uri: Uri): String? {
        // First try to get the EXIF date
        val dateText = extractExifDate(uri)
        if (dateText != null) {
            return dateText
        }
        
        // Fallback to filename if no date is available
        return try {
            val fileName = when (uri.scheme) {
                "content" -> {
                    // For content URIs, try to get the display name
                    context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                        val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (nameIndex != -1 && cursor.moveToFirst()) {
                            cursor.getString(nameIndex)
                        } else null
                    } ?: uri.lastPathSegment
                }
                "file" -> {
                    // For file URIs, extract filename from path
                    uri.lastPathSegment
                }
                else -> uri.lastPathSegment
            }
            
            // Clean up the filename - remove extension and sanitize
            fileName?.let { name ->
                val nameWithoutExtension = name.substringBeforeLast('.')
                // Keep only alphanumeric, spaces, hyphens, and underscores
                nameWithoutExtension.replace(Regex("[^a-zA-Z0-9\\s_-]"), "")
                    .trim()
                    .take(20) // Limit length for display
                    .takeIf { it.isNotBlank() }
            }
        } catch (e: Exception) {
            Log.w(TAG, "extractImageLabel: Error extracting filename from $uri: ${e.message}")
            null
        }
    }
    
    @Suppress("SameParameterValue")
    private fun drawDateLabel(bitmap: Bitmap, dateText: String, position: DateLabelPosition = DateLabelPosition.BOTTOM_RIGHT): Bitmap {
        val mutableBitmap = bitmap.copy(bitmap.config ?: Bitmap.Config.ARGB_8888, true)
        val canvas = Canvas(mutableBitmap)
        
        // Calculate font size based on image dimensions (similar to your reference images)
        val baseFontSize = minOf(bitmap.width, bitmap.height) * 0.04f // 4% of smallest dimension
        val fontSize = baseFontSize.coerceAtLeast(16f).coerceAtMost(32f)
        
        // Create paint for text background (dark rectangle)
        val backgroundPaint = Paint().apply {
            color = Color.BLACK
            alpha = 180 // Semi-transparent
            isAntiAlias = true
        }
        
        // Create paint for text
        val textPaint = Paint().apply {
            color = Color.WHITE
            textSize = fontSize
            isAntiAlias = true
            typeface = android.graphics.Typeface.DEFAULT_BOLD
        }
        
        // Measure text dimensions
        val textBounds = android.graphics.Rect()
        textPaint.getTextBounds(dateText, 0, dateText.length, textBounds)
        val textWidth = textBounds.width()
        val textHeight = textBounds.height()
        
        // Add padding around text
        val padding = fontSize * 0.3f
        val backgroundWidth = textWidth + (padding * 2)
        val backgroundHeight = textHeight + (padding * 2)
        
        // Calculate position based on DateLabelPosition
        val (backgroundLeft, backgroundTop) = when (position) {
            DateLabelPosition.BOTTOM_RIGHT -> {
                val left = bitmap.width - backgroundWidth - padding
                val top = bitmap.height - backgroundHeight - padding
                Pair(left, top)
            }
            DateLabelPosition.BOTTOM_LEFT -> {
                val left = padding
                val top = bitmap.height - backgroundHeight - padding
                Pair(left, top)
            }
        }
        
        // Draw background rectangle
        canvas.drawRoundRect(
            backgroundLeft,
            backgroundTop,
            backgroundLeft + backgroundWidth,
            backgroundTop + backgroundHeight,
            padding / 2,
            padding / 2,
            backgroundPaint
        )
        
        // Draw text
        val textX = backgroundLeft + padding
        val textY = backgroundTop + backgroundHeight - padding - textBounds.bottom
        canvas.drawText(dateText, textX, textY, textPaint)
        
        return mutableBitmap
    }
    
    enum class DateLabelPosition {
        BOTTOM_RIGHT,
        BOTTOM_LEFT
    }
    
    @Suppress("SameParameterValue")
    private fun resizeBitmapWithPersonDetection(
        bitmap: Bitmap,
        targetWidth: Int,
        targetHeight: Int,
        personDetectionResult: PersonDetectionResult?
    ): Bitmap {
        return if (personDetectionResult?.mainPerson != null) {
            resizeBitmapSmartCrop(bitmap, targetWidth, targetHeight,
                personDetectionResult.mainPerson
            )
        } else {
            resizeBitmap(bitmap, targetWidth, targetHeight)
        }
    }
    
    private fun resizeBitmapSmartCrop(
        bitmap: Bitmap, 
        targetWidth: Int, 
        targetHeight: Int, 
        mainPerson: Detection
    ): Bitmap {
        val targetAspectRatio = targetWidth.toFloat() / targetHeight
        val bitmapAspectRatio = bitmap.width.toFloat() / bitmap.height
        
        Log.d(TAG, "resizeBitmapSmartCrop: Input bitmap: ${bitmap.width}x${bitmap.height}, target: ${targetWidth}x${targetHeight}")
        Log.d(TAG, "resizeBitmapSmartCrop: Person bounding box: ${mainPerson.boundingBox}")
        Log.d(TAG, "resizeBitmapSmartCrop: Target aspect ratio: $targetAspectRatio, bitmap aspect ratio: $bitmapAspectRatio")
        
        // Validate person bounding box is within bitmap bounds
        val personBox = mainPerson.boundingBox
        val isValid = personBox.left >= 0 && personBox.top >= 0 && 
                     personBox.right <= bitmap.width && personBox.bottom <= bitmap.height &&
                     personBox.width() > 0 && personBox.height() > 0
        
        if (!isValid) {
            Log.w(TAG, "resizeBitmapSmartCrop: Invalid person bounding box ${personBox} for bitmap ${bitmap.width}x${bitmap.height}, falling back to center crop")
            return resizeBitmap(bitmap, targetWidth, targetHeight)
        }
        
        // Calculate crop region that includes the main person and maintains target aspect ratio
        val cropRegion = calculateOptimalCropRegion(
            bitmap.width, bitmap.height, 
            mainPerson.boundingBox, 
            targetAspectRatio
        )
        
        Log.d(TAG, "resizeBitmapSmartCrop: Calculated crop region: $cropRegion")
        
        // Validate crop region
        val cropValid = cropRegion.left >= 0 && cropRegion.top >= 0 && 
                       cropRegion.right <= bitmap.width && cropRegion.bottom <= bitmap.height &&
                       cropRegion.width() > 0 && cropRegion.height() > 0
        
        if (!cropValid) {
            Log.w(TAG, "resizeBitmapSmartCrop: Invalid crop region $cropRegion for bitmap ${bitmap.width}x${bitmap.height}, falling back to center crop")
            return resizeBitmap(bitmap, targetWidth, targetHeight)
        }
        
        // Crop the bitmap to focus on the person
        val croppedBitmap = Bitmap.createBitmap(
            bitmap,
            cropRegion.left.toInt(),
            cropRegion.top.toInt(),
            (cropRegion.right - cropRegion.left).toInt(),
            (cropRegion.bottom - cropRegion.top).toInt()
        )
        
        Log.d(TAG, "resizeBitmapSmartCrop: Cropped bitmap: ${croppedBitmap.width}x${croppedBitmap.height}")
        
        // Scale the cropped bitmap to target size
        val finalBitmap = croppedBitmap.scale(targetWidth, targetHeight)
        
        Log.d(TAG, "resizeBitmapSmartCrop: Final bitmap: ${finalBitmap.width}x${finalBitmap.height}")
        
        if (croppedBitmap != bitmap) {
            croppedBitmap.recycle()
        }
        
        return finalBitmap
    }
    
    private fun calculateOptimalCropRegion(
        imageWidth: Int,
        imageHeight: Int,
        personBounds: RectF,
        targetAspectRatio: Float
    ): RectF {
        
        // Calculate person center and dimensions
        val personCenterX = (personBounds.left + personBounds.right) / 2
        val personCenterY = (personBounds.top + personBounds.bottom) / 2
        val personWidth = personBounds.right - personBounds.left
        val personHeight = personBounds.bottom - personBounds.top
        
        // Check if person detection covers too much of the image (>80% in either dimension)
        // If so, fall back to center cropping as smart cropping won't be effective
        val personWidthRatio = personWidth / imageWidth
        val personHeightRatio = personHeight / imageHeight
        
        if (personWidthRatio > 0.8f || personHeightRatio > 0.8f) {
            Log.d(TAG, "calculateOptimalCropRegion: Person detection too large (${personWidthRatio * 100}% width, ${personHeightRatio * 100}% height), using center crop")
            
            // Calculate center crop region with target aspect ratio
            val cropWidth: Float
            val cropHeight: Float
            
            val imageAspectRatio = imageWidth.toFloat() / imageHeight
            if (imageAspectRatio > targetAspectRatio) {
                // Image is wider than target, crop horizontally
                cropHeight = imageHeight.toFloat()
                cropWidth = cropHeight * targetAspectRatio
            } else {
                // Image is taller than target, crop vertically  
                cropWidth = imageWidth.toFloat()
                cropHeight = cropWidth / targetAspectRatio
            }
            
            val cropLeft = (imageWidth - cropWidth) / 2
            val cropTop = (imageHeight - cropHeight) / 2
            val cropRight = cropLeft + cropWidth
            val cropBottom = cropTop + cropHeight
            
            return RectF(cropLeft, cropTop, cropRight, cropBottom)
        }
        
        // Calculate minimum crop size to include the entire person with some padding
        val paddingFactor = 1.3f // 30% padding around person
        val minCropWidth = personWidth * paddingFactor
        val minCropHeight = personHeight * paddingFactor
        
        // Calculate crop dimensions that maintain target aspect ratio
        val cropWidth: Float
        val cropHeight: Float
        
        if (minCropWidth / minCropHeight > targetAspectRatio) {
            // Width is the limiting factor
            cropWidth = minCropWidth
            cropHeight = cropWidth / targetAspectRatio
        } else {
            // Height is the limiting factor
            cropHeight = minCropHeight
            cropWidth = cropHeight * targetAspectRatio
        }
        
        // Calculate crop position centered on person
        var cropLeft = personCenterX - cropWidth / 2
        var cropTop = personCenterY - cropHeight / 2
        var cropRight = cropLeft + cropWidth
        var cropBottom = cropTop + cropHeight
        
        // Adjust if crop extends beyond image boundaries
        if (cropLeft < 0) {
            cropRight -= cropLeft
            cropLeft = 0f
        }
        if (cropTop < 0) {
            cropBottom -= cropTop
            cropTop = 0f
        }
        if (cropRight > imageWidth) {
            cropLeft -= (cropRight - imageWidth)
            cropRight = imageWidth.toFloat()
        }
        if (cropBottom > imageHeight) {
            cropTop -= (cropBottom - imageHeight)
            cropBottom = imageHeight.toFloat()
        }
        
        // Ensure crop region is within image bounds
        cropLeft = cropLeft.coerceAtLeast(0f)
        cropTop = cropTop.coerceAtLeast(0f)
        cropRight = cropRight.coerceAtMost(imageWidth.toFloat())
        cropBottom = cropBottom.coerceAtMost(imageHeight.toFloat())
        
        return RectF(cropLeft, cropTop, cropRight, cropBottom)
    }
    
    private fun resizeBitmap(bitmap: Bitmap, targetWidth: Int, targetHeight: Int): Bitmap {
        // Calculate scaling to fill the entire target area (crop-to-fill)
        val scaleX = targetWidth.toFloat() / bitmap.width
        val scaleY = targetHeight.toFloat() / bitmap.height
        val scale = maxOf(scaleX, scaleY) // Use maxOf instead of minOf to fill completely
        
        val scaledWidth = (bitmap.width * scale).toInt()
        val scaledHeight = (bitmap.height * scale).toInt()
        
        val scaledBitmap = bitmap.scale(scaledWidth, scaledHeight)
        
        // Calculate crop region to get center portion of scaled image
        val cropX = (scaledWidth - targetWidth) / 2
        val cropY = (scaledHeight - targetHeight) / 2
        
        // Create final bitmap by cropping the center portion
        val finalBitmap = Bitmap.createBitmap(
            scaledBitmap,
            cropX.coerceAtLeast(0),
            cropY.coerceAtLeast(0),
            targetWidth,
            targetHeight
        )
        
        scaledBitmap.recycle()
        return finalBitmap
    }
    
    private fun convertToGrayscale(bitmap: Bitmap): Bitmap {
        val grayscaleBitmap = createBitmap(bitmap.width, bitmap.height)
        
        val canvas = Canvas(grayscaleBitmap)
        val paint = Paint()
        val colorMatrix = ColorMatrix()
        colorMatrix.setSaturation(0f) // Convert to grayscale
        paint.colorFilter = ColorMatrixColorFilter(colorMatrix)
        canvas.drawBitmap(bitmap, 0f, 0f, paint)
        
        return grayscaleBitmap
    }
    
    private fun applyFloydSteinbergDithering(bitmap: Bitmap): Bitmap {
        val width = bitmap.width
        val height = bitmap.height
        
        // Create a mutable copy
        val mutableBitmap = bitmap.copy(Bitmap.Config.ARGB_8888, true)
        
        val pixels = IntArray(width * height)
        mutableBitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        
        for (y in 0 until height) {
            for (x in 0 until width) {
                val index = y * width + x
                val oldPixel = pixels[index]
                val gray = Color.red(oldPixel) // Already grayscale, so R=G=B
                
                // Quantize to black or white (matching the monochrome conversion)
                val newPixel = if (gray > 127) Color.WHITE else Color.BLACK
                pixels[index] = newPixel
                
                val error = gray - (if (newPixel == Color.WHITE) 255 else 0)
                
                // Distribute error to neighboring pixels (Floyd-Steinberg)
                if (x + 1 < width) {
                    val rightIndex = y * width + (x + 1)
                    pixels[rightIndex] = adjustPixel(pixels[rightIndex], (error * 7 / 16))
                }
                if (y + 1 < height) {
                    if (x > 0) {
                        val bottomLeftIndex = (y + 1) * width + (x - 1)
                        pixels[bottomLeftIndex] = adjustPixel(pixels[bottomLeftIndex], (error * 3 / 16))
                    }
                    val bottomIndex = (y + 1) * width + x
                    pixels[bottomIndex] = adjustPixel(pixels[bottomIndex], (error * 5 / 16))
                    if (x + 1 < width) {
                        val bottomRightIndex = (y + 1) * width + (x + 1)
                        pixels[bottomRightIndex] = adjustPixel(pixels[bottomRightIndex], (error * 1 / 16))
                    }
                }
            }
        }
        
        mutableBitmap.setPixels(pixels, 0, width, 0, 0, width, height)
        return mutableBitmap
    }
    
    private fun adjustPixel(pixel: Int, error: Int): Int {
        val red = ((Color.red(pixel) + error).coerceIn(0, 255))
        val green = ((Color.green(pixel) + error).coerceIn(0, 255))
        val blue = ((Color.blue(pixel) + error).coerceIn(0, 255))
        return Color.rgb(red, green, blue)
    }
    
    private fun convertTo8BitColor(bitmap: Bitmap): ByteArray {
        val width = bitmap.width
        val height = bitmap.height
        val buffer = ByteArray(width * height)
        
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        
        for (i in pixels.indices) {
            val pixel = pixels[i]
            // Match Rust implementation: ((color.r() / 32) << 5) + ((color.g() / 32) << 2) + (color.b() / 64)
            val r = Color.red(pixel) / 32   // 3 bits (0-7)
            val g = Color.green(pixel) / 32 // 3 bits (0-7)
            val b = Color.blue(pixel) / 64  // 2 bits (0-3)
            
            val color8 = (r shl 5) + (g shl 2) + b
            buffer[i] = color8.toByte()
        }
        
        return buffer
    }
    
    @Suppress("unused")
    fun clearProcessedFiles() {
        outputDir.listFiles()?.forEach { file ->
            if (file.extension == "bin" || file.extension == "bmp") {
                file.delete()
            }
        }
    }
    
    fun cleanup() {
        personDetector.close()
    }
}