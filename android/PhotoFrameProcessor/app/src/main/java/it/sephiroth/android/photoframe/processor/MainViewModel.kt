package it.sephiroth.android.photoframe.processor

import android.app.Application
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.core.content.FileProvider
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.bumptech.glide.Glide
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class MainViewModel(application: Application) : AndroidViewModel(application) {
    
    companion object {
        private const val TAG = "MainViewModel"
    }
    
    private val _selectedImages = MutableLiveData<List<ProcessedImage>>(emptyList())
    val selectedImages: LiveData<List<ProcessedImage>> = _selectedImages
    
    private val _isProcessing = MutableLiveData<Boolean>(false)
    val isProcessing: LiveData<Boolean> = _isProcessing
    
    private val _isImporting = MutableLiveData<Boolean>(false)
    val isImporting: LiveData<Boolean> = _isImporting
    
    private val _processedFiles = MutableLiveData<List<File>>(emptyList())
    val processedFiles: LiveData<List<File>> = _processedFiles
    
    private val _statusMessage = MutableLiveData<String>("Ready")
    val statusMessage: LiveData<String> = _statusMessage
    
    private val _allImagesValid = MutableLiveData<Boolean>(true)
    val allImagesValid: LiveData<Boolean> = _allImagesValid
    
    private val _hasValidImages = MutableLiveData<Boolean>(false)
    val hasValidImages: LiveData<Boolean> = _hasValidImages
    
    private val _shareStatisticsMessage = MutableLiveData<String>()
    val shareStatisticsMessage: LiveData<String> = _shareStatisticsMessage
    
    private val imageProcessor = ImageProcessor(getApplication())
    
    fun addImages(uris: List<Uri>) {
        Log.d(TAG, "addImages: Adding ${uris.size} images")
        
        val currentImages = _selectedImages.value?.toMutableList() ?: mutableListOf()
        val currentUris = currentImages.map { it.originalUri }
        val newUris = uris.filter { it !in currentUris }
        Log.d(TAG, "addImages: ${newUris.size} new images (${uris.size - newUris.size} duplicates filtered)")
        
        if (newUris.isEmpty()) {
            Log.d(TAG, "addImages: No new images to add")
            return
        }
        
        viewModelScope.launch {
            _isImporting.value = true
            _statusMessage.value = "Analyzing ${newUris.size} image${if (newUris.size != 1) "s" else ""}..."
            
            try {
                // Analyze only the NEW images
                Log.d(TAG, "addImages: Starting analysis of ${newUris.size} new images")
                val startTime = System.currentTimeMillis()
                val newAnalyzedImages = withContext(Dispatchers.IO) {
                    imageProcessor.analyzeImagesForPreview(newUris)
                }
                val analysisTime = System.currentTimeMillis() - startTime
                Log.d(TAG, "addImages: New image analysis completed in ${analysisTime}ms")
                
                // Preserve existing images and add new ones at the END
                val allImages = currentImages + newAnalyzedImages
                Log.d(TAG, "addImages: Combined ${currentImages.size} existing + ${newAnalyzedImages.size} new = ${allImages.size} total images")
                
                // Handle pairing for any unpaired portraits
                val finalImages = handlePortraitPairing(allImages)
                
                val validCount = finalImages.count { !it.isInvalid }
                val invalidCount = finalImages.count { it.isInvalid }
                val portraitCount = finalImages.count { it.isPortrait && !it.isInvalid }
                val landscapeCount = finalImages.count { !it.isPortrait && !it.isInvalid }
                
                Log.i(TAG, "addImages: Final results - Valid: $validCount, Invalid: $invalidCount, Portrait: $portraitCount, Landscape: $landscapeCount")
                
                val allValid = invalidCount == 0
                val hasValidImages = validCount > 0
                _allImagesValid.value = allValid
                _hasValidImages.value = hasValidImages
                
                _statusMessage.value = when {
                    invalidCount > 0 -> "$validCount valid images, $invalidCount invalid (unpaired portraits)"
                    else -> "$validCount images selected"
                }
                
                // Set importing to false BEFORE updating selectedImages to ensure proper button state
                _isImporting.value = false
                _selectedImages.value = finalImages
                
            } catch (e: Exception) {
                Log.e(TAG, "addImages: Error analyzing images", e)
                _statusMessage.value = "Error analyzing images: ${e.message}"
                _isImporting.value = false
            }
        }
    }
    
    private fun handlePortraitPairing(images: List<ProcessedImage>): List<ProcessedImage> {
        Log.d(TAG, "handlePortraitPairing: Starting pairing logic for ${images.size} images")
        
        val result = images.toMutableList()
        
        // Find unpaired portrait images (including previously invalid ones that could now be paired)
        val unpairedPortraits = result.filter { 
            it.isPortrait && it.pairedWith == null
        }.toMutableList()
        
        Log.d(TAG, "handlePortraitPairing: Found ${unpairedPortraits.size} unpaired portrait images")
        
        // Keep track of URIs to remove (the second image in each pair)
        val urisToRemove = mutableSetOf<Uri>()
        
        // Pair up unpaired portraits
        var pairCount = 0
        while (unpairedPortraits.size >= 2) {
            val first = unpairedPortraits.removeAt(0)
            val second = unpairedPortraits.removeAt(0)
            
            Log.d(TAG, "handlePortraitPairing: Pairing ${first.originalUri} with ${second.originalUri}")
            
            // Find indices
            val firstIndex = result.indexOfFirst { it.originalUri == first.originalUri }
            val secondIndex = result.indexOfFirst { it.originalUri == second.originalUri }
            
            if (firstIndex >= 0 && secondIndex >= 0) {
                // Update the first image to be paired with the second and mark as valid
                result[firstIndex] = result[firstIndex].copy(
                    pairedWith = result[secondIndex], 
                    isInvalid = false
                )
                
                // Mark the second image for removal (to avoid duplicates)
                urisToRemove.add(second.originalUri)
                pairCount++
            }
        }
        
        // Remove the second images from pairs to avoid duplicates
        val filteredResult = result.filterNot { it.originalUri in urisToRemove }
        
        // Mark remaining unpaired portraits as invalid in the filtered result
        val finalResult = filteredResult.toMutableList()
        unpairedPortraits.forEach { unpairedPortrait ->
            val index = finalResult.indexOfFirst { it.originalUri == unpairedPortrait.originalUri }
            if (index >= 0) {
                Log.d(TAG, "handlePortraitPairing: Marking unpaired portrait ${unpairedPortrait.originalUri} as invalid")
                finalResult[index] = finalResult[index].copy(isInvalid = true)
            }
        }
        
        Log.d(TAG, "handlePortraitPairing: Created $pairCount new pairs, ${unpairedPortraits.size} portraits remain unpaired, removed ${urisToRemove.size} duplicate thumbnails")
        
        return finalResult
    }
    
    fun removeImage(uri: Uri) {
        Log.d(TAG, "removeImage: Removing image $uri")
        val currentImages = _selectedImages.value?.toMutableList() ?: return
        
        // Log current state before removal
        Log.d(TAG, "removeImage: Current state before removal:")
        currentImages.forEachIndexed { index, image ->
            Log.d(TAG, "  [$index] URI: ${image.originalUri}, Portrait: ${image.isPortrait}, Invalid: ${image.isInvalid}, PairedWith: ${image.pairedWith?.originalUri}")
        }
        
        val imageToRemove = currentImages.find { it.originalUri == uri }
        
        if (imageToRemove != null) {
            Log.d(TAG, "removeImage: Found image to remove - Portrait: ${imageToRemove.isPortrait}, Invalid: ${imageToRemove.isInvalid}, Paired: ${imageToRemove.pairedWith != null}")
            if (imageToRemove.pairedWith != null) {
                Log.d(TAG, "removeImage: PairedWith URI: ${imageToRemove.pairedWith!!.originalUri}")
            }
            
            // Clean up preview file if it exists
            if (imageToRemove.previewFile?.delete() == true) {
                Log.d(TAG, "removeImage: Cleaned up preview file: ${imageToRemove.previewFile?.name}")
            }
            
            // Determine which URIs to remove based on image type
            val urisToRemove = if (imageToRemove.isPortrait && imageToRemove.pairedWith != null) {
                // If removing a paired portrait, remove both images in the pair
                Log.d(TAG, "removeImage: Removing paired portrait images")
                val uris = listOf(imageToRemove.originalUri, imageToRemove.pairedWith!!.originalUri)
                Log.d(TAG, "removeImage: URIs to remove: $uris")
                uris
            } else {
                // If removing a landscape image or invalid image, remove just that one
                Log.d(TAG, "removeImage: Removing single image")
                val uris = listOf(imageToRemove.originalUri)
                Log.d(TAG, "removeImage: URIs to remove: $uris")
                uris
            }
            
            // Clean up paired image's preview file if it exists
            if (imageToRemove.pairedWith != null) {
                if (imageToRemove.pairedWith!!.previewFile?.delete() == true) {
                    Log.d(TAG, "removeImage: Cleaned up paired image preview file")
                }
            }
            
            // Remove corresponding processed files if the image was processed
            if (imageToRemove.isProcessed) {
                removeCorrespondingProcessedFiles(imageToRemove)
            }
            
            // Simply filter out the items to remove, preserving existing pairings
            val remainingImages = currentImages.filter { it.originalUri !in urisToRemove }
            
            Log.d(TAG, "removeImage: ${urisToRemove.size} images removed, ${remainingImages.size} remaining items")
            
            if (remainingImages.isEmpty()) {
                _selectedImages.value = emptyList()
                _statusMessage.value = "No images selected"
                _allImagesValid.value = true // No images means no invalid images
                _hasValidImages.value = false
                
                // Clear all processed files when all images are removed
                Log.d(TAG, "removeImage: All images removed, clearing all processed files")
                val currentProcessedFiles = _processedFiles.value
                if (!currentProcessedFiles.isNullOrEmpty()) {
                    // Delete all processed files from disk in background
                    viewModelScope.launch(Dispatchers.IO) {
                        currentProcessedFiles.forEach { file ->
                            try {
                                if (file.delete()) {
                                    Log.d(TAG, "removeImage: Deleted processed file: ${file.name}")
                                }
                            } catch (e: Exception) {
                                Log.w(TAG, "removeImage: Failed to delete processed file: ${file.name}", e)
                            }
                        }
                    }
                    // Clear the processed files list immediately
                    _processedFiles.value = emptyList()
                    Log.d(TAG, "removeImage: Cleared all ${currentProcessedFiles.size} processed files")
                }
                
                Log.d(TAG, "removeImage: All images removed")
            } else {
                // Keep existing analysis and pairings, just update the list
                Log.d(TAG, "removeImage: Preserving existing pairings for ${remainingImages.size} items")
                
                // Log the preserved state
                Log.d(TAG, "removeImage: Preserved state:")
                remainingImages.forEachIndexed { index, image ->
                    Log.d(TAG, "  [$index] URI: ${image.originalUri}, Portrait: ${image.isPortrait}, Invalid: ${image.isInvalid}, PairedWith: ${image.pairedWith?.originalUri}")
                }
                
                _selectedImages.value = remainingImages
                
                val validCount = remainingImages.count { !it.isInvalid }
                val invalidCount = remainingImages.count { it.isInvalid }
                
                _allImagesValid.value = invalidCount == 0
                _hasValidImages.value = validCount > 0
                Log.d(TAG, "removeImage: After removal - Valid: $validCount, Invalid: $invalidCount")
                
                _statusMessage.value = when {
                    invalidCount > 0 -> "$validCount valid images, $invalidCount invalid (unpaired portraits)"
                    else -> "$validCount images selected"
                }
                
                // Clean up any orphaned processed files that no longer correspond to existing images
                cleanupOrphanedProcessedFiles(remainingImages)
            }
        } else {
            Log.w(TAG, "removeImage: Image not found for removal: $uri")
        }
    }
    
    fun processImages() {
        Log.i(TAG, "processImages: Starting image processing")
        val images = _selectedImages.value
        if (images.isNullOrEmpty()) {
            Log.w(TAG, "processImages: No images to process")
            _statusMessage.value = "No images to process"
            return
        }
        
        // Check if there are any valid images to process
        val validImages = images.filter { !it.isInvalid }
        if (validImages.isEmpty()) {
            Log.w(TAG, "processImages: No valid images to process (${images.size} total images, all invalid)")
            _statusMessage.value = "No valid images to process"
            return
        }
        
        // Filter out already processed images
        val unprocessedImages = validImages.filter { !it.isProcessed }
        if (unprocessedImages.isEmpty()) {
            Log.i(TAG, "processImages: All valid images (${validImages.size}) are already processed")
            _statusMessage.value = "All images are already processed"
            return
        }
        
        Log.i(TAG, "processImages: Processing ${unprocessedImages.size} unprocessed images out of ${validImages.size} valid images (${images.size} total)")
        
        viewModelScope.launch {
            val processingStartTime = System.currentTimeMillis()
            _isProcessing.value = true
            _statusMessage.value = "Processing images..."
            Log.d(TAG, "processImages: Set processing state to true")
            
            // Set only unprocessed images to IN_QUEUE state - do this efficiently
            Log.d(TAG, "processImages: Setting unprocessed images to IN_QUEUE state")
            val unprocessedUris = unprocessedImages.map { it.originalUri }.toSet()
            val currentImages = withContext(Dispatchers.Default) {
                _selectedImages.value?.map { image ->
                    if (image.originalUri in unprocessedUris) {
                        image.copy(processingState = ProcessingState.IN_QUEUE)
                    } else {
                        image
                    }
                } ?: emptyList()
            }
            _selectedImages.value = currentImages
            Log.d(TAG, "processImages: All unprocessed images set to IN_QUEUE state")
            
            try {
                // Clear Glide cache and processed files to ensure fresh processing
                Log.d(TAG, "processImages: Clearing Glide cache and processed files before processing")
                val appContext: Application = getApplication()
                withContext<Unit>(Dispatchers.Main) {
                    Glide.get(appContext).clearMemory()
                }
                withContext<Unit>(Dispatchers.IO) {
                    Glide.get(appContext).clearDiskCache()
                    // imageProcessor.clearProcessedFiles() // Commented out to preserve files across processing sessions
                }
                
                val processedFiles = withContext(Dispatchers.IO) {
                    var lastUpdateTime = 0L
                    val updateInterval = 100L // Throttle UI updates to every 100ms
                    Log.d(TAG, "processImages: Starting actual processing with ${unprocessedImages.size} images")
                    
                    // Pass the already-analyzed ProcessedImage list directly
                    imageProcessor.processAnalyzedImages(unprocessedImages) { progress, total, previewFile ->
                        val currentTime = System.currentTimeMillis()
                        val shouldUpdateUI = currentTime - lastUpdateTime > updateInterval || progress == total
                        
                        Log.v(TAG, "processImages: Progress callback - $progress/$total, preview: ${previewFile != null}")
                        
                        if (shouldUpdateUI) {
                            lastUpdateTime = currentTime
                            
                            // Update status message
                            launch(Dispatchers.Main) {
                                _statusMessage.value = "Processing image $progress of $total"
                            }
                            Log.d(TAG, "processImages: Updated status to: Processing image $progress of $total")
                        }
                        
                        // Update image state (always do this, but optimize the operations)
                        launch(Dispatchers.Main) {
                            updateImageProcessingState(unprocessedImages, progress - 1, previewFile)
                        }
                    }
                }
                
                val processingTime = System.currentTimeMillis() - processingStartTime
                Log.i(TAG, "processImages: Processing completed successfully in ${processingTime}ms. Generated ${processedFiles.size} files")
                
                _processedFiles.value = processedFiles
                _statusMessage.value = "Complete! ${processedFiles.size} files processed"
                
            } catch (e: Exception) {
                val processingTime = System.currentTimeMillis() - processingStartTime
                Log.e(TAG, "processImages: Processing failed after ${processingTime}ms", e)
                _statusMessage.value = "Error processing images: ${e.message}"
                
                // Reset processing states on error - do this efficiently
                Log.d(TAG, "processImages: Resetting processing states due to error")
                val resetImages = withContext(Dispatchers.Default) {
                    _selectedImages.value?.map { image ->
                        if (image.processingState == ProcessingState.PROCESSING || 
                            image.processingState == ProcessingState.IN_QUEUE) {
                            image.copy(processingState = ProcessingState.IDLE)
                        } else {
                            image
                        }
                    } ?: emptyList()
                }
                _selectedImages.value = resetImages
                
            } finally {
                _isProcessing.value = false
                Log.d(TAG, "processImages: Set processing state to false")
            }
        }
    }
    
    fun shareProcessedFiles(shareType: String = "preview") {
        Log.i(TAG, "shareProcessedFiles: Starting file sharing - type: $shareType")
        
        val files = when (shareType) {
            "binary" -> {
                // Share binary .bin files
                _processedFiles.value
            }
            "preview" -> {
                // Share preview .jpg files
                _selectedImages.value?.filter { it.isProcessed && it.previewFile?.exists() == true }
                    ?.mapNotNull { it.previewFile }
            }
            "both" -> {
                // Share both binary .bin files and preview .jpg files
                val binaryFiles = _processedFiles.value ?: emptyList()
                val previewFiles = _selectedImages.value?.filter { it.isProcessed && it.previewFile?.exists() == true }
                    ?.mapNotNull { it.previewFile } ?: emptyList()
                binaryFiles + previewFiles
            }
            else -> {
                Log.w(TAG, "shareProcessedFiles: Unknown share type: $shareType, defaulting to preview")
                _selectedImages.value?.filter { it.isProcessed && it.previewFile?.exists() == true }
                    ?.mapNotNull { it.previewFile }
            }
        }
        
        if (files.isNullOrEmpty()) {
            Log.w(TAG, "shareProcessedFiles: No files to share for type: $shareType")
            _statusMessage.value = "No processed files to share"
            return
        }
        
        Log.d(TAG, "shareProcessedFiles: Sharing ${files.size} $shareType files")
        
        try {
            val context = getApplication<Application>()
            val mimeType = when (shareType) {
                "binary" -> "application/octet-stream"
                "preview" -> "image/bmp"
                "both" -> "*/*"
                else -> "image/bmp"
            }
            
            val shareIntent = Intent().apply {
                if (files.size == 1) {
                    Log.d(TAG, "shareProcessedFiles: Setting up single file share")
                    // Single file share
                    action = Intent.ACTION_SEND
                    type = mimeType
                    val fileUri = FileProvider.getUriForFile(
                        context,
                        "${context.packageName}.fileprovider",
                        files.first()
                    )
                    putExtra(Intent.EXTRA_STREAM, fileUri)
                    Log.d(TAG, "shareProcessedFiles: Single file URI: $fileUri")
                } else {
                    Log.d(TAG, "shareProcessedFiles: Setting up multiple file share")
                    // Multiple files share
                    action = Intent.ACTION_SEND_MULTIPLE
                    type = mimeType
                    val fileUris = files.map { file ->
                        FileProvider.getUriForFile(
                            context,
                            "${context.packageName}.fileprovider",
                            file
                        )
                    }
                    putParcelableArrayListExtra(Intent.EXTRA_STREAM, ArrayList(fileUris))
                    Log.d(TAG, "shareProcessedFiles: Multiple file URIs created: ${fileUris.size}")
                }
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            
            val shareTitle = when (shareType) {
                "binary" -> "Share binary files"
                "preview" -> "Share BMP images"
                "both" -> "Share binary files and BMP images"
                else -> "Share processed files"
            }
            val chooserIntent = Intent.createChooser(shareIntent, shareTitle)
            chooserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(chooserIntent)
            
            Log.i(TAG, "shareProcessedFiles: Share intent launched successfully")
            _statusMessage.value = "Files shared successfully!"
            
            // Calculate and show statistics toast
            showShareStatisticsToast(shareType, files.size)
            
        } catch (e: Exception) {
            Log.e(TAG, "shareProcessedFiles: Error sharing files", e)
            _statusMessage.value = "Error sharing files: ${e.message}"
        }
    }
    
    private fun showShareStatisticsToast(shareType: String, fileCount: Int) {
        val currentImages = _selectedImages.value ?: return
        
        // Count landscapes and portrait pairs (not individual portraits)
        var landscapeCount = 0
        var portraitPairCount = 0
        
        currentImages.filter { it.isProcessed && !it.isInvalid }.forEach { image ->
            when {
                !image.isPortrait -> {
                    // Landscape image
                    landscapeCount++
                }
                image.isPortrait && image.pairedWith != null -> {
                    // Portrait pair (only count once per pair)
                    portraitPairCount++
                }
                // Skip individual unpaired portraits as they shouldn't be processed
            }
        }
        
        Log.d(TAG, "showShareStatisticsToast: Calculated stats - $landscapeCount landscapes, $portraitPairCount portrait pairs, $fileCount total files")
        
        // Format message based on share type
        val itemType = when (shareType) {
            "binary" -> "files"
            "both" -> "files"
            else -> "images"
        }
        val message = buildString {
            append("Exported $fileCount $itemType")
            
            val parts = mutableListOf<String>()
            if (landscapeCount > 0) {
                parts.add("$landscapeCount landscape${if (landscapeCount != 1) "s" else ""}")
            }
            if (portraitPairCount > 0) {
                parts.add("$portraitPairCount portrait${if (portraitPairCount != 1) "s" else ""}")
            }
            
            if (parts.isNotEmpty()) {
                append(". ")
                append(parts.joinToString(" and "))
            }
        }
        
        Log.d(TAG, "showShareStatisticsToast: Statistics message: $message")
        
        // Post message to LiveData for MainActivity to show as Snackbar
        _shareStatisticsMessage.value = message
    }
    
    private fun removeCorrespondingProcessedFiles(imageToRemove: ProcessedImage) {
        Log.d(TAG, "removeCorrespondingProcessedFiles: Removing processed files for image ${imageToRemove.originalUri}")
        
        val currentProcessedFiles = _processedFiles.value?.toMutableList() ?: return
        
        // Generate expected filename patterns based on image type
        val expectedFileNames = if (imageToRemove.isPortrait && imageToRemove.pairedWith != null) {
            // For portrait pairs, generate combined filename
            val firstName = generateSafeFileName(imageToRemove.originalUri)
            val secondName = generateSafeFileName(imageToRemove.pairedWith!!.originalUri)
            val combinedName = "${firstName}_${secondName}".take(100)
            listOf("combined_${combinedName}.bin")
        } else if (!imageToRemove.isPortrait) {
            // For landscape images
            val safeFileName = generateSafeFileName(imageToRemove.originalUri)
            listOf("image_${safeFileName}.bin")
        } else {
            // Invalid images shouldn't have processed files
            emptyList()
        }
        
        Log.d(TAG, "removeCorrespondingProcessedFiles: Looking for files: $expectedFileNames")
        
        // Find and remove matching processed files
        val filesToRemove = currentProcessedFiles.filter { file ->
            expectedFileNames.any { expectedName -> file.name == expectedName }
        }
        
        if (filesToRemove.isNotEmpty()) {
            Log.d(TAG, "removeCorrespondingProcessedFiles: Found ${filesToRemove.size} processed files to remove")
            filesToRemove.forEach { file ->
                Log.d(TAG, "removeCorrespondingProcessedFiles: Removing processed file: ${file.name}")
                
                // Delete the actual file from disk in background
                viewModelScope.launch(Dispatchers.IO) {
                    try {
                        if (file.delete()) {
                            Log.d(TAG, "removeCorrespondingProcessedFiles: Successfully deleted file: ${file.name}")
                        } else {
                            Log.w(TAG, "removeCorrespondingProcessedFiles: Failed to delete file: ${file.name}")
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "removeCorrespondingProcessedFiles: Error deleting file: ${file.name}", e)
                    }
                }
            }
            
            // Remove from processed files list
            val remainingProcessedFiles = currentProcessedFiles.filter { file ->
                !expectedFileNames.any { expectedName -> file.name == expectedName }
            }
            _processedFiles.value = remainingProcessedFiles
            
            Log.d(TAG, "removeCorrespondingProcessedFiles: Removed ${filesToRemove.size} files, ${remainingProcessedFiles.size} processed files remaining")
        } else {
            Log.d(TAG, "removeCorrespondingProcessedFiles: No matching processed files found")
        }
    }
    
    private fun cleanupOrphanedProcessedFiles(remainingImages: List<ProcessedImage>) {
        val currentProcessedFiles = _processedFiles.value?.toMutableList() ?: return
        if (currentProcessedFiles.isEmpty()) return
        
        Log.d(TAG, "cleanupOrphanedProcessedFiles: Checking ${currentProcessedFiles.size} processed files against ${remainingImages.size} remaining images")
        
        // Generate expected filenames for all remaining processed images
        val expectedFileNames = mutableSetOf<String>()
        
        remainingImages.filter { it.isProcessed }.forEach { image ->
            when {
                image.isPortrait && image.pairedWith != null -> {
                    // Portrait pair
                    val firstName = generateSafeFileName(image.originalUri)
                    val secondName = generateSafeFileName(image.pairedWith!!.originalUri)
                    val combinedName = "${firstName}_${secondName}".take(100)
                    expectedFileNames.add("combined_${combinedName}.bin")
                }
                !image.isPortrait -> {
                    // Landscape image
                    val safeFileName = generateSafeFileName(image.originalUri)
                    expectedFileNames.add("image_${safeFileName}.bin")
                }
                // Invalid images don't have processed files
            }
        }
        
        Log.d(TAG, "cleanupOrphanedProcessedFiles: Expected files: $expectedFileNames")
        
        // Find orphaned files (processed files that don't match any remaining image)
        val orphanedFiles = currentProcessedFiles.filter { file ->
            !expectedFileNames.contains(file.name)
        }
        
        if (orphanedFiles.isNotEmpty()) {
            Log.d(TAG, "cleanupOrphanedProcessedFiles: Found ${orphanedFiles.size} orphaned files to remove")
            
            orphanedFiles.forEach { file ->
                Log.d(TAG, "cleanupOrphanedProcessedFiles: Removing orphaned file: ${file.name}")
                
                // Delete from disk in background
                viewModelScope.launch(Dispatchers.IO) {
                    try {
                        if (file.delete()) {
                            Log.d(TAG, "cleanupOrphanedProcessedFiles: Successfully deleted orphaned file: ${file.name}")
                        } else {
                            Log.w(TAG, "cleanupOrphanedProcessedFiles: Failed to delete orphaned file: ${file.name}")
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "cleanupOrphanedProcessedFiles: Error deleting orphaned file: ${file.name}", e)
                    }
                }
            }
            
            // Update processed files list
            val cleanedProcessedFiles = currentProcessedFiles.filter { file ->
                expectedFileNames.contains(file.name)
            }
            _processedFiles.value = cleanedProcessedFiles
            
            Log.d(TAG, "cleanupOrphanedProcessedFiles: Removed ${orphanedFiles.size} orphaned files, ${cleanedProcessedFiles.size} files remaining")
        } else {
            Log.d(TAG, "cleanupOrphanedProcessedFiles: No orphaned files found")
        }
    }
    
    fun clearAll() {
        Log.i(TAG, "clearAll: Clearing all images and processed files")
        
        viewModelScope.launch {
            try {
                val currentImages = _selectedImages.value
                val currentProcessedFiles = _processedFiles.value
                
                // Clean up preview files in background
                if (!currentImages.isNullOrEmpty()) {
                    withContext(Dispatchers.IO) {
                        currentImages.forEach { image ->
                            image.previewFile?.let { previewFile ->
                                try {
                                    if (previewFile.delete()) {
                                        Log.d(TAG, "clearAll: Deleted preview file: ${previewFile.name}")
                                    }
                                } catch (e: Exception) {
                                    Log.w(TAG, "clearAll: Failed to delete preview file: ${previewFile.name}", e)
                                }
                            }
                        }
                    }
                }
                
                // Clean up processed files in background
                if (!currentProcessedFiles.isNullOrEmpty()) {
                    withContext(Dispatchers.IO) {
                        currentProcessedFiles.forEach { file ->
                            try {
                                if (file.delete()) {
                                    Log.d(TAG, "clearAll: Deleted processed file: ${file.name}")
                                }
                            } catch (e: Exception) {
                                Log.w(TAG, "clearAll: Failed to delete processed file: ${file.name}", e)
                            }
                        }
                    }
                }
                
                // Clear all lists and reset state
                _selectedImages.value = emptyList()
                _processedFiles.value = emptyList()
                _allImagesValid.value = true
                _hasValidImages.value = false
                _statusMessage.value = "All images cleared"
                
                Log.i(TAG, "clearAll: Successfully cleared ${currentImages?.size ?: 0} images and ${currentProcessedFiles?.size ?: 0} processed files")
                
            } catch (e: Exception) {
                Log.e(TAG, "clearAll: Error clearing images", e)
                _statusMessage.value = "Error clearing images: ${e.message}"
            }
        }
    }
    
    private fun generateSafeFileName(uri: Uri): String {
        // Extract filename from URI or generate one from URI hash
        val fileName = try {
            uri.lastPathSegment?.substringAfterLast('/')?.substringBeforeLast('.') 
                ?: uri.toString().hashCode().toString()
        } catch (e: Exception) {
            uri.toString().hashCode().toString()
        }
        
        // Sanitize filename - remove special characters and limit length
        return fileName.replace(Regex("[^a-zA-Z0-9_-]"), "_")
            .take(50) // Limit length to avoid filesystem issues
    }

    private fun updateImageProcessingState(validImages: List<ProcessedImage>, currentIndex: Int, previewFile: File?) {
        if (currentIndex < 0 || currentIndex >= validImages.size) {
            Log.w(TAG, "updateImageProcessingState: Invalid index $currentIndex for ${validImages.size} images")
            return
        }
        
        val currentImages = _selectedImages.value?.toMutableList() ?: return
        val validImageUri = validImages[currentIndex].originalUri
        val imageIndex = currentImages.indexOfFirst { it.originalUri == validImageUri }
        
        if (imageIndex >= 0) {
            val imageToUpdate = currentImages[imageIndex]
            
            if (previewFile != null) {
                Log.d(TAG, "updateImageProcessingState: Image $currentIndex completed, preview: ${previewFile.name}")
                
                // Processing completed - clean up old preview file in background
                val oldPreviewFile = imageToUpdate.previewFile
                if (oldPreviewFile != null) {
                    viewModelScope.launch(Dispatchers.IO) {
                        try {
                            if (oldPreviewFile.delete()) {
                                Log.v(TAG, "updateImageProcessingState: Cleaned up old preview file: ${oldPreviewFile.name}")
                            }
                        } catch (e: Exception) {
                            Log.w(TAG, "updateImageProcessingState: Failed to delete old preview file", e)
                        }
                    }
                }
                
                // Update to completed state
                currentImages[imageIndex] = imageToUpdate.copy(
                    previewFile = previewFile,
                    isProcessed = true,
                    processingState = ProcessingState.COMPLETED
                )
                Log.d(TAG, "updateImageProcessingState: Set image $currentIndex to COMPLETED state")
            } else {
                // Set to processing state
                currentImages[imageIndex] = imageToUpdate.copy(
                    processingState = ProcessingState.PROCESSING
                )
                Log.d(TAG, "updateImageProcessingState: Set image $currentIndex to PROCESSING state")
            }
            
            // Use setValue instead of postValue for immediate update
            _selectedImages.value = currentImages
        } else {
            Log.w(TAG, "updateImageProcessingState: Could not find image index for URI: $validImageUri")
        }
    }

    override fun onCleared() {
        super.onCleared()
        imageProcessor.cleanup()
    }
}