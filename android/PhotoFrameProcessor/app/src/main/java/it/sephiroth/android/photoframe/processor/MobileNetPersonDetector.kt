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
import androidx.core.graphics.scale

class MobileNetPersonDetector(private val context: Context) {
    
    companion object {
        private const val TAG = "MobileNetPersonDetector"
        private const val MODEL_FILE = "models/yolov3_person.tflite"
        private const val INPUT_SIZE = 150 // MobileNet SSD input size (model tensor requirement)
        private const val PIXEL_SIZE = 3 // RGB
        private const val IMAGE_MEAN = 127.5f
        private const val IMAGE_STD = 127.5f
        private const val CONFIDENCE_THRESHOLD = 0.3f
        private const val PERSON_CLASS_ID = 21 // Person class (actual class ID from model)
        
        // MobileNet SSD output tensor sizes
        private const val NUM_DETECTIONS = 10
        private const val NUM_CLASSES = 91
    }
    
    private var interpreter: Interpreter? = null
    
    init {
        try {
            val model = loadModelFile()
            val options = Interpreter.Options()
            
            interpreter = Interpreter(model, options)
            
            // Log model details
            val inputTensor = interpreter!!.getInputTensor(0)
            val outputTensorCount = interpreter!!.outputTensorCount
            Log.i(TAG, "MobileNet PersonDetector initialized successfully")
            Log.i(TAG, "Input tensor shape: ${inputTensor.shape().contentToString()}")
            Log.i(TAG, "Input tensor type: ${inputTensor.dataType()}")
            Log.i(TAG, "Number of output tensors: $outputTensorCount")
            
            for (i in 0 until outputTensorCount) {
                val outputTensor = interpreter!!.getOutputTensor(i)
                Log.i(TAG, "Output tensor $i shape: ${outputTensor.shape().contentToString()}")
                Log.i(TAG, "Output tensor $i type: ${outputTensor.dataType()}")
            }
            
        } catch (e: java.io.IOException) {
            Log.w(TAG, "MobileNet model file not found: ${e.message}")
            interpreter = null
        } catch (e: Exception) {
            Log.e(TAG, "Error initializing MobileNet PersonDetector: ${e.message}")
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
        val interpreter = this.interpreter
        if (interpreter == null) {
            Log.w(TAG, "detectPersons: No interpreter available")
            return null
        }
        
        val startTime = System.currentTimeMillis()
        Log.d(TAG, "detectPersons: Starting detection on ${bitmap.width}x${bitmap.height} bitmap")
        
        try {
            // Preprocess image
            val inputBuffer = preprocessImage(bitmap)
            Log.d(TAG, "detectPersons: Preprocessed image to buffer size ${inputBuffer.capacity()}")
            
            // Prepare output buffers for MobileNet SSD
            val outputLocations = Array(1) { Array(NUM_DETECTIONS) { FloatArray(4) } }
            val outputClasses = Array(1) { FloatArray(NUM_DETECTIONS) }
            val outputScores = Array(1) { FloatArray(NUM_DETECTIONS) }
            val outputCount = FloatArray(1)
            
            val outputs = mapOf(
                0 to outputLocations,
                1 to outputClasses,
                2 to outputScores,
                3 to outputCount
            )
            
            // Run inference
            Log.d(TAG, "detectPersons: Running inference...")
            interpreter.runForMultipleInputsOutputs(arrayOf(inputBuffer), outputs)
            Log.d(TAG, "detectPersons: Inference completed, found ${outputCount[0].toInt()} total detections")
            
            // Log some raw outputs for debugging
            Log.d(TAG, "detectPersons: First few scores: ${outputScores[0].take(5).joinToString()}")
            Log.d(TAG, "detectPersons: First few classes: ${outputClasses[0].take(5).joinToString()}")
            
            // Post-process results
            val detections = postProcessOutputs(
                outputLocations[0], 
                outputClasses[0], 
                outputScores[0], 
                outputCount[0].toInt(),
                bitmap.width, 
                bitmap.height
            )
            
            val processingTime = System.currentTimeMillis() - startTime
            
            // Find the main person (largest bounding box area)
            val mainPerson = detections.maxByOrNull { 
                val box = it.boundingBox
                (box.right - box.left) * (box.bottom - box.top) 
            }
            
            Log.i(TAG, "detectPersons: Detected ${detections.size} persons in ${processingTime}ms")
            if (detections.isNotEmpty()) {
                detections.forEach { detection ->
                    Log.d(TAG, "detectPersons: Person detected with confidence ${detection.confidence} at ${detection.boundingBox}")
                }
            }
            
            return PersonDetectionResult(detections, mainPerson, processingTime)
            
        } catch (e: Exception) {
            Log.e(TAG, "detectPersons: Error during inference: ${e.message}", e)
            return null
        }
    }
    
    private fun preprocessImage(bitmap: Bitmap): ByteBuffer {
        val inputBuffer = ByteBuffer.allocateDirect(4 * INPUT_SIZE * INPUT_SIZE * PIXEL_SIZE)
        inputBuffer.order(ByteOrder.nativeOrder())
        
        // Resize bitmap to model input size (150x150) - input can be larger, we resize here for model
        val resizedBitmap = bitmap.scale(INPUT_SIZE, INPUT_SIZE)
        
        val intValues = IntArray(INPUT_SIZE * INPUT_SIZE)
        resizedBitmap.getPixels(intValues, 0, INPUT_SIZE, 0, 0, INPUT_SIZE, INPUT_SIZE)
        
        // Convert to normalized float values (MobileNet preprocessing)
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
        locations: Array<FloatArray>,
        classes: FloatArray,
        scores: FloatArray,
        numDetections: Int,
        originalWidth: Int,
        originalHeight: Int
    ): List<Detection> {
        
        Log.d(TAG, "postProcessOutputs: Processing $numDetections detections")
        val detections = mutableListOf<Detection>()
        
        for (i in 0 until minOf(numDetections, NUM_DETECTIONS)) {
            val score = scores[i]
            val classId = classes[i].toInt()
            
            Log.v(TAG, "postProcessOutputs: Detection $i - class: $classId, score: $score, person_class: $PERSON_CLASS_ID, threshold: $CONFIDENCE_THRESHOLD")
            
            // Only process person detections with sufficient confidence
            if (classId == PERSON_CLASS_ID && score >= CONFIDENCE_THRESHOLD) {
                
                // MobileNet SSD outputs normalized coordinates (0-1)
                val yMin = locations[i][0]
                val xMin = locations[i][1] 
                val yMax = locations[i][2]
                val xMax = locations[i][3]
                
                Log.d(TAG, "postProcessOutputs: Person detected! Score: $score, coords: ($xMin,$yMin) to ($xMax,$yMax)")
                
                // Convert to original image coordinates
                val left = (xMin * originalWidth).coerceAtLeast(0f)
                val top = (yMin * originalHeight).coerceAtLeast(0f)
                val right = (xMax * originalWidth).coerceAtMost(originalWidth.toFloat())
                val bottom = (yMax * originalHeight).coerceAtMost(originalHeight.toFloat())
                
                val boundingBox = RectF(left, top, right, bottom)
                detections.add(Detection(boundingBox, score, PERSON_CLASS_ID))
                
                Log.d(TAG, "postProcessOutputs: Added person detection with box: $boundingBox out of image rect ${RectF(0f, 0f, originalWidth.toFloat(), originalHeight.toFloat())}")
            }
        }
        
        Log.d(TAG, "postProcessOutputs: Returning ${detections.size} person detections")
        return detections
    }
    
    fun close() {
        interpreter?.close()
        interpreter = null
    }
}