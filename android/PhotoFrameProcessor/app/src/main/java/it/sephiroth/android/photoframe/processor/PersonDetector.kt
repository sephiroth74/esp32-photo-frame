package it.sephiroth.android.photoframe.processor

import android.content.Context
import android.graphics.Bitmap
import android.graphics.RectF
import android.util.Log
import org.tensorflow.lite.Interpreter
import java.io.FileInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel
import kotlin.math.max
import kotlin.math.min
import androidx.core.graphics.scale
import kotlin.math.exp

data class Detection(
    val boundingBox: RectF,
    val confidence: Float,
    val classId: Int
)

data class PersonDetectionResult(
    val detections: List<Detection>,
    val mainPerson: Detection? = detections.firstOrNull(),
    val processingTimeMs: Long = 0
)

class PersonDetector(private val context: Context) {
    
    companion object {
        private const val TAG = "PersonDetector"
        private const val MODEL_FILE = "models/yolov3_person.tflite"
        private const val INPUT_SIZE = 416 // YOLOv3 standard input size
        private const val PIXEL_SIZE = 3 // RGB
        private const val IMAGE_MEAN = 0f
        private const val IMAGE_STD = 255f
        private const val CONFIDENCE_THRESHOLD = 0.5f
        private const val IOU_THRESHOLD = 0.45f
        private const val PERSON_CLASS_ID = 0 // In COCO dataset, person is class 0
        
        // YOLOv3 has 3 output layers with different scales
        private const val OUTPUT_SIZE_1 = 13 * 13 * 3 * 85 // 13x13 grid
        private const val OUTPUT_SIZE_2 = 26 * 26 * 3 * 85 // 26x26 grid  
        private const val OUTPUT_SIZE_3 = 52 * 52 * 3 * 85 // 52x52 grid
    }
    
    private var interpreter: Interpreter? = null
    private var isGPU = false
    private val labels = listOf("person") // We only care about person detection
    
    init {
        try {
            val model = loadModelFile()
            val options = Interpreter.Options()
            
            interpreter = Interpreter(model, options)
            Log.i(TAG, "PersonDetector initialized successfully (CPU only)")
        } catch (e: java.io.IOException) {
            Log.w(TAG, "YOLOv3 model file not found: ${e.message}")
            Log.w(TAG, "Person detection will be disabled. See app/src/main/assets/models/README.md for instructions on adding the model file.")
            interpreter = null
        } catch (e: Exception) {
            Log.e(TAG, "Error initializing PersonDetector: ${e.message}")
            interpreter = null
        }
    }
    
    private fun loadModelFile(): MappedByteBuffer {
        val fileDescriptor = context.assets.openFd(MODEL_FILE)
        val inputStream = FileInputStream(fileDescriptor.fileDescriptor)
        val fileChannel = inputStream.channel
        val startOffset = fileDescriptor.startOffset
        val declaredLength = fileDescriptor.declaredLength
        return fileChannel.map(FileChannel.MapMode.READ_ONLY, startOffset, declaredLength)
    }
    
    fun detectPersons(bitmap: Bitmap): PersonDetectionResult? {
        val interpreter = this.interpreter ?: return null
        
        val startTime = System.currentTimeMillis()
        
        try {
            // Preprocess image
            val inputBuffer = preprocessImage(bitmap)
            
            // Prepare output buffers for 3 YOLO output layers
            val output1 = Array(1) { Array(13) { Array(13) { Array(3) { FloatArray(85) } } } }
            val output2 = Array(1) { Array(26) { Array(26) { Array(3) { FloatArray(85) } } } }
            val output3 = Array(1) { Array(52) { Array(52) { Array(3) { FloatArray(85) } } } }
            
            val outputs = mapOf(
                0 to output1,
                1 to output2, 
                2 to output3
            )
            
            // Run inference
            interpreter.runForMultipleInputsOutputs(arrayOf(inputBuffer), outputs)
            
            // Post-process results
            val detections = postProcessOutputs(output1, output2, output3, bitmap.width, bitmap.height)
            val processingTime = System.currentTimeMillis() - startTime
            
            // Find the main person (largest bounding box area)
            val mainPerson = detections.maxByOrNull { 
                val box = it.boundingBox
                (box.right - box.left) * (box.bottom - box.top) 
            }
            
            Log.d(TAG, "Detected ${detections.size} persons in ${processingTime}ms")
            
            return PersonDetectionResult(detections, mainPerson, processingTime)
            
        } catch (e: Exception) {
            Log.e(TAG, "Error during inference: ${e.message}")
            return null
        }
    }
    
    private fun preprocessImage(bitmap: Bitmap): ByteBuffer {
        val inputBuffer = ByteBuffer.allocateDirect(4 * INPUT_SIZE * INPUT_SIZE * PIXEL_SIZE)
        inputBuffer.order(ByteOrder.nativeOrder())
        
        // Resize bitmap to model input size
        val resizedBitmap = bitmap.scale(INPUT_SIZE, INPUT_SIZE)
        
        val intValues = IntArray(INPUT_SIZE * INPUT_SIZE)
        resizedBitmap.getPixels(intValues, 0, INPUT_SIZE, 0, 0, INPUT_SIZE, INPUT_SIZE)
        
        // Convert to normalized float values
        var pixel = 0
        for (i in 0 until INPUT_SIZE) {
            for (j in 0 until INPUT_SIZE) {
                val pixelValue = intValues[pixel++]
                inputBuffer.putFloat(((pixelValue shr 16 and 0xFF) - IMAGE_MEAN) / IMAGE_STD)
                inputBuffer.putFloat(((pixelValue shr 8 and 0xFF) - IMAGE_MEAN) / IMAGE_STD) 
                inputBuffer.putFloat(((pixelValue and 0xFF) - IMAGE_MEAN) / IMAGE_STD)
            }
        }
        
        if (resizedBitmap != bitmap) {
            resizedBitmap.recycle()
        }
        
        return inputBuffer
    }
    
    private fun postProcessOutputs(
        output1: Array<Array<Array<Array<FloatArray>>>>,
        output2: Array<Array<Array<Array<FloatArray>>>>,
        output3: Array<Array<Array<Array<FloatArray>>>>,
        originalWidth: Int,
        originalHeight: Int
    ): List<Detection> {
        
        val allDetections = mutableListOf<Detection>()
        
        // Process each output layer
        allDetections.addAll(processOutputLayer(output1[0], 13, originalWidth, originalHeight))
        allDetections.addAll(processOutputLayer(output2[0], 26, originalWidth, originalHeight))
        allDetections.addAll(processOutputLayer(output3[0], 52, originalWidth, originalHeight))
        
        // Apply Non-Maximum Suppression
        return applyNMS(allDetections)
    }
    
    private fun processOutputLayer(
        output: Array<Array<Array<FloatArray>>>,
        gridSize: Int,
        originalWidth: Int,
        originalHeight: Int
    ): List<Detection> {
        
        val detections = mutableListOf<Detection>()
        val anchors = getAnchorsForGridSize(gridSize)
        
        for (y in 0 until gridSize) {
            for (x in 0 until gridSize) {
                for (anchor in 0 until 3) {
                    val prediction = output[y][x][anchor]
                    
                    val objectConfidence = sigmoid(prediction[4])
                    if (objectConfidence < CONFIDENCE_THRESHOLD) continue
                    
                    val classConfidence = sigmoid(prediction[5]) // Person class probability
                    val confidence = objectConfidence * classConfidence
                    
                    if (confidence < CONFIDENCE_THRESHOLD) continue
                    
                    // Decode bounding box
                    val centerX = (x + sigmoid(prediction[0])) / gridSize
                    val centerY = (y + sigmoid(prediction[1])) / gridSize
                    val width = anchors[anchor][0] * exp(prediction[2].toDouble()) / INPUT_SIZE
                    val height = anchors[anchor][1] * exp(prediction[3].toDouble()) / INPUT_SIZE
                    
                    // Convert to original image coordinates
                    val left = ((centerX - width / 2) * originalWidth).toFloat()
                    val top = ((centerY - height / 2) * originalHeight).toFloat()
                    val right = ((centerX + width / 2) * originalWidth).toFloat()
                    val bottom = ((centerY + height / 2) * originalHeight).toFloat()
                    
                    val boundingBox = RectF(
                        max(0f, left),
                        max(0f, top), 
                        min(originalWidth.toFloat(), right),
                        min(originalHeight.toFloat(), bottom)
                    )
                    
                    detections.add(Detection(boundingBox, confidence, PERSON_CLASS_ID))
                }
            }
        }
        
        return detections
    }
    
    private fun getAnchorsForGridSize(gridSize: Int): Array<FloatArray> {
        return when (gridSize) {
            13 -> arrayOf(
                floatArrayOf(116f, 90f),
                floatArrayOf(156f, 198f), 
                floatArrayOf(373f, 326f)
            )
            26 -> arrayOf(
                floatArrayOf(30f, 61f),
                floatArrayOf(62f, 45f),
                floatArrayOf(59f, 119f)
            )
            52 -> arrayOf(
                floatArrayOf(10f, 13f),
                floatArrayOf(16f, 30f),
                floatArrayOf(33f, 23f)
            )
            else -> arrayOf(floatArrayOf(10f, 13f))
        }
    }
    
    private fun sigmoid(x: Float): Float = (1.0f / (1.0f + exp(-x.toDouble()))).toFloat()
    
    private fun applyNMS(detections: List<Detection>): List<Detection> {
        if (detections.isEmpty()) return emptyList()
        
        val sortedDetections = detections.sortedByDescending { it.confidence }
        val selectedDetections = mutableListOf<Detection>()
        
        for (detection in sortedDetections) {
            var shouldSelect = true
            
            for (selectedDetection in selectedDetections) {
                val iou = calculateIoU(detection.boundingBox, selectedDetection.boundingBox)
                if (iou > IOU_THRESHOLD) {
                    shouldSelect = false
                    break
                }
            }
            
            if (shouldSelect) {
                selectedDetections.add(detection)
            }
        }
        
        return selectedDetections
    }
    
    private fun calculateIoU(box1: RectF, box2: RectF): Float {
        val intersectionLeft = max(box1.left, box2.left)
        val intersectionTop = max(box1.top, box2.top)
        val intersectionRight = min(box1.right, box2.right)
        val intersectionBottom = min(box1.bottom, box2.bottom)
        
        if (intersectionLeft >= intersectionRight || intersectionTop >= intersectionBottom) {
            return 0f
        }
        
        val intersectionArea = (intersectionRight - intersectionLeft) * (intersectionBottom - intersectionTop)
        val box1Area = (box1.right - box1.left) * (box1.bottom - box1.top)
        val box2Area = (box2.right - box2.left) * (box2.bottom - box2.top)
        val unionArea = box1Area + box2Area - intersectionArea
        
        return if (unionArea > 0) intersectionArea / unionArea else 0f
    }
    
    fun close() {
        interpreter?.close()
        interpreter = null
    }
}