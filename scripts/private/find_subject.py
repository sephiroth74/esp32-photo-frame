#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""find_subject.py
This script detects people in an input image using a pre-trained YOLO model (YOLOv3) via OpenCV's DNN module.

Main Function:
---------------
detect_people_yolo(image_path, config_path, weights_path, coco_names_path, confidence_threshold=0.5, nms_threshold=0.4)


    Description:
        - Loads COCO class names and identifies the 'person' class index.
        - Loads the YOLO model and processes the input image.
        - Runs inference to detect bounding boxes for people.
        - Applies non-maximum suppression to filter overlapping detections.
        - Calculates and prints the bounding box coordinates, center, and offset from image center.
        - Exits with error if no people are detected or if input files are invalid.

Usage:
------
Run from command line with required arguments:
    --image         Path to input image (required)
    --config        Path to YOLOv3 config file (.cfg)
    --weights       Path to YOLOv3 weights file (.weights)
    --names         Path to COCO names file (.names)
    --confidence    Minimum confidence threshold (default: 0.75)
    --nms_threshold Non-maximum suppression threshold (default: 0.4)

Example:
--------
python find_subject.py --image input.jpg --config yolov3.cfg --weights yolov3.weights --names coco.names

Dependencies:
-------------
- OpenCV (cv2)
- NumPy (numpy)
"""

import cv2
import numpy as np
import argparse
import sys, os

def detect_people_yolo(image_path, config_path, weights_path, coco_names_path, confidence_threshold=0.5, nms_threshold=0.4):
    """
    Detects people in an image using a pre-trained YOLO model.

    Args:
        image_path (str): Path to the input image.
        config_path (str): Path to the YOLO model configuration file (.cfg).
        weights_path (str): Path to the YOLO model weights file (.weights).
        coco_names_path (str): Path to the COCO class names file (.names).
        confidence_threshold (float): Minimum confidence to consider a detection.
        nms_threshold (float): Non-maximum suppression threshold.
    """

    # Load COCO class names
    with open(coco_names_path, 'r') as f:
        classes = [line.strip() for line in f.readlines()]

    # Filter for 'person' class index
    try:
        person_class_id = classes.index('person')
    except ValueError:
        print("Error: 'person' class not found in coco.names. Please ensure the file is correct.")
        sys.exit(1)
        return

    # Load YOLO model
    net = cv2.dnn.readNetFromDarknet(config_path, weights_path)

    # Enable CUDA if available (for faster inference)
    # Uncomment the following lines if you have a compatible NVIDIA GPU and CUDA installed
    # net.setPreferableBackend(cv2.dnn.DNN_BACKEND_CUDA)
    # net.setPreferableTarget(cv2.dnn.DNN_TARGET_CUDA)

    # Get output layer names
    layer_names = net.getLayerNames()
    # Ensure output_layers is a flat list of integers
    output_layer_indices = net.getUnconnectedOutLayers()
    output_layer_indices = [i[0] if isinstance(i, (list, np.ndarray)) else i for i in output_layer_indices]
    output_layers = [layer_names[i - 1] for i in output_layer_indices]

    # Load the input image
    image = cv2.imread(image_path)
    if image is None:
        print(f"Error: Could not load image from {image_path}")
        sys.exit(1)
        return

    height, width, _ = image.shape

    # Create a blob from the image
    # The image is scaled to 416x416, and the pixel values are scaled to [0, 1]
    blob = cv2.dnn.blobFromImage(image, 1/255.0, (416, 416), swapRB=True, crop=False)

    # Set the input to the network and run forward pass
    net.setInput(blob)
    outs = net.forward(output_layers)

    # Initialize lists for detected bounding boxes, confidences, and class IDs
    boxes = []
    confidences = []
    class_ids = []

    # Iterate over each output layer and detection
    for out in outs:
        for detection in out:
            scores = detection[5:]
            class_id = np.argmax(scores)
            confidence = scores[class_id]

            # Filter for 'person' class and confidence
            if class_id == person_class_id and confidence > confidence_threshold:
                # YOLO returns center_x, center_y, width, height
                center_x = int(detection[0] * width)
                center_y = int(detection[1] * height)
                w = int(detection[2] * width)
                h = int(detection[3] * height)

                # Calculate top-left corner coordinates
                x = int(center_x - w / 2)
                y = int(center_y - h / 2)

                boxes.append([x, y, w, h])
                confidences.append(float(confidence))
                class_ids.append(class_id)

    # Apply Non-Maximum Suppression to suppress weak, overlapping bounding boxes
    indexes = cv2.dnn.NMSBoxes(boxes, confidences, confidence_threshold, nms_threshold)

    x_min, y_min = float('inf'), float('inf')
    w_max, h_max = 0, 0

    # print image size
    print(f"imagesize: {width},{height}")

    if len(indexes) > 0:
        for i in indexes.flatten():
            x, y, w, h = boxes[i]

            right = x + w
            bottom = y + h

            x_min = min(x_min, x)
            y_min = min(y_min, y)
            w_max = max(w_max, right)
            h_max = max(h_max, bottom)

            # label = str(classes[class_ids[i]])
            confidence = str(round(confidences[i], 2))

            # color = (0, 255, 0)  # Green color for bounding box

            # Draw bounding box
            # cv2.rectangle(image, (x, y), (x + w, y + h), color, 2)
            # Draw label and confidence
            # text = f"{label}: {confidence} (rect: {x}, {y}, {w}, {h})"
            # cv2.putText(image, text, (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
            # print(f"Detected {label} with confidence {confidence} at (x={x}, y={y}, w={w}, h={h})")
    else:
        print("No people detected in the image.")

        print(r"box: 0,0,${width},${height}")
        print(r"center: ${width//2},${height//2}")
        print(r"offset: 0,0")
        sys.exit(0)
        return

    # print(f"Bounding box coordinates: x_min={x_min}, y_min={y_min}, w_max={w_max}, h_max={h_max}")
    print(f"box: {x_min},{y_min},{w_max},{h_max}")

    # print the x,y coordinates of the center of the bounding box
    center_x = (x_min + w_max) // 2
    center_y = (y_min + h_max) // 2
    print(f"center: {center_x},{center_y}")

    # print the offset of center_x, center_y related to the image center
    image_center_x = width // 2
    image_center_y = height // 2
    offset_x = center_x - image_center_x
    offset_y = center_y - image_center_y
    # print(f"Offset from image center: (offset_x={offset_x}, offset_y={offset_y})")
    print(f"offset: {offset_x},{offset_y}")

    # Draw the bounding box on the image
    # cv2.rectangle(image, (x_min, y_min), (w_max, h_max), (255, 0, 0), 4)
    # cv2.imshow("People Detection", image)
    # cv2.waitKey(0)
    # cv2.destroyAllWindows()
    # now crop the image to the given bounding box, using the x_min, y_min, w_max, h_max coordinates as center of the cropped image

    # cv2.imshow("Cropped Image", image)
    # cv2.waitKey(0)
    cv2.destroyAllWindows()
    sys.exit(0)

if __name__ == "__main__":
    # Current script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Detect people in an image using YOLOv3.")
    parser.add_argument("--image", type=str, required=True, help="Path to the input image.")
    parser.add_argument("--config", type=str, default=script_dir + "/assets/yolov3.cfg", help="Path to YOLOv3 config file.")
    parser.add_argument("--weights", type=str, default=script_dir + "/assets/yolov3.weights", help="Path to YOLOv3 weights file.")
    parser.add_argument("--names", type=str, default=script_dir + "/assets/coco.names", help="Path to COCO names file.")
    parser.add_argument("--confidence", type=float, default=0.70, help="Minimum confidence threshold.")
    parser.add_argument("--nms_threshold", type=float, default=0.4, help="Non-maximum suppression threshold.")

    args = parser.parse_args()

    detect_people_yolo(args.image, args.config, args.weights, args.names, args.confidence, args.nms_threshold)