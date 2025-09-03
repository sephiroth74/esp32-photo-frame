# YOLOv3 Person Detection Model

## Overview
This directory should contain the YOLOv3 TensorFlow Lite model for person detection. The app uses this model to identify people in images and apply smart cropping to focus on the main person.

## Required File
- `yolov3_person.tflite` - YOLOv3 model converted to TensorFlow Lite format

## How to obtain the model

### Option 1: Convert from official YOLOv3 weights
1. Download the official YOLOv3 weights from the YOLO website
2. Use TensorFlow's conversion tools to convert to TensorFlow Lite format
3. Place the resulting `.tflite` file in this directory as `yolov3_person.tflite`

### Option 2: Use pre-converted model
1. Search for "yolov3 tensorflow lite" models online
2. Download a pre-converted model that supports person detection (COCO dataset)
3. Rename it to `yolov3_person.tflite` and place it in this directory

### Option 3: Alternative person detection models
You can also use other person detection models in TensorFlow Lite format:
- MobileNet SSD with person detection
- Other YOLO variants (YOLOv4, YOLOv5) converted to TensorFlow Lite

## Model Requirements
- Input size: 416x416x3 (RGB)
- Output: YOLO format with bounding boxes and class probabilities
- Must support person class detection (typically class ID 0 in COCO dataset)

## Fallback Behavior
If the model file is not found, the app will:
- Log a warning message
- Continue processing images without person detection
- Use standard center cropping instead of smart cropping
- All other functionality remains intact

## Performance Notes
- GPU acceleration is automatically enabled if supported by the device
- Larger models provide better accuracy but slower inference
- Consider using quantized models for better performance on mobile devices

## Example conversion script (Python)
```python
import tensorflow as tf

# Convert YOLOv3 to TensorFlow Lite
converter = tf.lite.TFLiteConverter.from_saved_model("yolov3_saved_model")
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()

# Save the model
with open('yolov3_person.tflite', 'wb') as f:
    f.write(tflite_model)
```