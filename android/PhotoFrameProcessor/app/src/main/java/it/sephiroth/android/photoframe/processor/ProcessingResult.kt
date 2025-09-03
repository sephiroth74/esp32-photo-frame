package it.sephiroth.android.photoframe.processor

import java.io.File

data class ProcessingResult(
    val binaryFile: File,
    val previewFile: File
)