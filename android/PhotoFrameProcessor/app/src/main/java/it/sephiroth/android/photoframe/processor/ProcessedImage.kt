package it.sephiroth.android.photoframe.processor

import android.net.Uri
import java.io.File

enum class ProcessingState {
    IDLE,           // Not started processing
    IN_QUEUE,       // Waiting to be processed
    PROCESSING,     // Currently being processed
    COMPLETED       // Processing completed successfully
}

data class ProcessedImage(
    val originalUri: Uri,
    val previewFile: File? = null,
    val isProcessed: Boolean = false,
    val isPortrait: Boolean = false,
    val width: Int = 0,
    val height: Int = 0,
    val pairedWith: ProcessedImage? = null,
    val isInvalid: Boolean = false,
    val invalidReason: String? = null,
    val personDetectionResult: PersonDetectionResult? = null,
    val hasMainPerson: Boolean = personDetectionResult?.mainPerson != null,
    val processingState: ProcessingState = ProcessingState.IDLE
)