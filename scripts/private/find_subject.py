#!/usr/bin/env python
# -*- coding: utf-8 -*-

# wget https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n.pt
# installation: https://docs.ultralytics.com/it/integrations/onnx/#installation

"""find_subject.py
This script detects people in an input image using a pre-trained YOLO model (YOLOv3) via OpenCV's DNN module.

Main Function:
---------------
detect_people_yolo(args={image_path, model_path, coco_names_path, confidence_threshold=0.5})

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
    --model         Path to YOLOv3 model file (.onnx)
    --names         Path to COCO names file (.names)
    --confidence    Minimum confidence threshold (default: 0.7)
    --filter        Comma-separated list of class names to filter (e.g., 'person,car')
    --output-format Output format: 'text' or 'json' (default: 'text')
    --debug         Enable debug mode to display images with detections

Example:
--------
    python find_subject.py --image input.jpg --model yolov3.onnx --names coco.names --confidence 0.7

Dependencies:
-------------
- OpenCV (cv2)
- NumPy (numpy)


Sources:
--------
Microsoft COCO: Common Objects in Context
Tsung-Yi Lin and Michael Maire and Serge Belongie and Lubomir Bourdev and Ross Girshick and James Hays and Pietro Perona and Deva Ramanan and C. Lawrence Zitnick and Piotr Dollár
2015
"""

import onnxruntime as ort
import cv2
import numpy as np
import argparse
import sys, os
import json

def _filter_detections(results, thresh = 0.5):
    # if model is trained on 1 class only
    if len(results[0]) == 5:
        # filter out the detections with confidence > thresh
        considerable_detections = [detection for detection in results if detection[4] > thresh]
        considerable_detections = np.array(considerable_detections)
        return considerable_detections

    # if model is trained on multiple classes
    else:
        A = []
        for detection in results:

            class_id = detection[4:].argmax()
            confidence_score = detection[4:].max()

            new_detection = np.append(detection[:4],[class_id,confidence_score])

            A.append(new_detection)

        A = np.array(A)

        # filter out the detections with confidence > thresh
        considerable_detections = [detection for detection in A if detection[-1] > thresh]
        considerable_detections = np.array(considerable_detections)

        return considerable_detections

def _NMS(boxes, conf_scores, iou_thresh = 0.55):
    #  boxes [[x1,y1, x2,y2], [x1,y1, x2,y2], ...]

    x1 = boxes[:,0]
    y1 = boxes[:,1]
    x2 = boxes[:,2]
    y2 = boxes[:,3]

    areas = (x2-x1)*(y2-y1)

    order = conf_scores.argsort()

    keep = []
    keep_confidences = []

    while len(order) > 0:
        idx = order[-1]
        A = boxes[idx]
        conf = conf_scores[idx]

        order = order[:-1]

        xx1 = np.take(x1, indices= order)
        yy1 = np.take(y1, indices= order)
        xx2 = np.take(x2, indices= order)
        yy2 = np.take(y2, indices= order)

        keep.append(A)
        keep_confidences.append(conf)

        # iou = inter/union

        xx1 = np.maximum(x1[idx], xx1)
        yy1 = np.maximum(y1[idx], yy1)
        xx2 = np.minimum(x2[idx], xx2)
        yy2 = np.minimum(y2[idx], yy2)

        w = np.maximum(xx2-xx1, 0)
        h = np.maximum(yy2-yy1, 0)

        intersection = w*h

        # union = areaA + other_areas - intesection
        other_areas = np.take(areas, indices= order)
        union = areas[idx] + other_areas - intersection

        iou = intersection/union

        boleans = iou < iou_thresh

        order = order[boleans]

        # order = [2,0,1]  boleans = [True, False, True]
        # order = [2,1]

    return keep, keep_confidences

# function to rescale bounding boxes 
def _rescale_back(results, img_w, img_h):
    if results.shape[0] == 0:  # Check if the array is empty (no detections)
        return [], []
    cx, cy, w, h, class_id, confidence = results[:,0], results[:,1], results[:,2], results[:,3], results[:,4], results[:,-1]
    cx = cx /640.0 * img_w
    cy = cy/640.0 * img_h
    w = w/640.0 * img_w
    h = h/640.0 * img_h
    x1 = cx - w/2
    y1 = cy - h/2
    x2 = cx + w/2
    y2 = cy + h/2

    boxes = np.column_stack((x1, y1, x2, y2, class_id))
    keep, keep_confidences = _NMS(boxes,confidence)
    return keep, keep_confidences

def _detect_people_yolo(args):
    """
    Detects people in an image using a pre-trained YOLO model.

    Args:
        image_path (str): Path to the input image.
        model_path (str): Path to the YOLO model weights file (.weights).
        coco_names_path (str): Path to the COCO class names file (.names).
        filter_names (list): List of class names to filter (e.g., ['person', 'car']).
        confidence_threshold (float): Minimum confidence to consider a detection.
        output_format (str): Output format, either 'text' or 'json'.

    """

    names = args.names
    image_path = args.image
    model_path = args.model
    confidence_threshold = args.confidence
    output_format = args.output_format

    # check if input files exist
    if not os.path.isfile(image_path):
        if output_format == 'json':
            print(json.dumps({"error": f"Image file {image_path} does not exist."}))
        else:
            print(f"Error: Image file {image_path} does not exist.")
        sys.exit(1)
        return

    # check if model file exists
    if not os.path.isfile(model_path):
        if output_format == 'json':
            print(json.dumps({"error": f"Model file {model_path} does not exist."}))
        else:
            print(f"Error: Model file {model_path} does not exist.")
        sys.exit(1)
        return
    
    # check if coco names file exists
    if not os.path.isfile(names):
        if output_format == 'json':
            print(json.dumps({"error": f"COCO names file {names} does not exist."}))
        else:
            print(f"Error: COCO names file {names} does not exist.")
        sys.exit(1)
        return

    # Load COCO class names
    with open(names, 'r') as f:
        classes = [line.strip() for line in f.readlines()]

    filter_names = args.filter.split(",") if args.filter else classes

    if args.debug:
        for i, cls in enumerate(classes):
            print(i, cls)

    # Load YOLO model
    onnx_model = ort.InferenceSession(model_path)

    # Load the input image
    image = cv2.imread(image_path, flags = cv2.IMREAD_COLOR)
    if image is None:
        if output_format == 'json':
            print(json.dumps({"error": f"Could not load image from {image_path}"}))
        else:
            print(f"Error: Could not load image from {image_path}")
        sys.exit(1)
        return

    height, width, _ = image.shape
    
    # Create the root json object
    if output_format == 'json':
        root = {
            "image": os.path.basename(image_path),
            "imagesize": {"width": width, "height": height},
            "total_detections": 0,
            "box": [],
            "center": [],
            "offset": [],
            "detections": [],
        }

    # Create a blob from the image
    # The image is scaled to 416x416, and the pixel values are scaled to [0, 1]
    # blob = cv2.dnn.blobFromImage(image, 1/255.0, (416, 416), swapRB=True, crop=False)

    img = cv2.resize(image, (640, 640))
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = img.transpose(2, 0, 1)
    img = img.reshape(1, 3, 640, 640)

    img = img / 255.0
    img = img.astype(np.float32)

    outputs = onnx_model.run(None, {"images": img})

    results = outputs[0]
    results = results.transpose()

    results = _filter_detections(results, confidence_threshold)
    rescaled_results, confidences = _rescale_back(results, width, height)

    # Initialize lists for detected bounding boxes, confidences, and class IDs
    _boxes = []
    _confidences = []
    _class_ids = []
    _total_detections = 0

    _x_min, _y_min = float('inf'), float('inf')
    _w_max, _h_max = 0, 0

    if output_format == 'text':
        print(f"imagesize: {width},{height}")    

    for res, conf in zip(rescaled_results, confidences):
        x1, y1, x2, y2, cls_id = res
        cls_id = int(cls_id)

        # filter only cls_id present in the coco names file
        if cls_id < 0 or cls_id >= len(classes):
            continue

        class_name = classes[cls_id]

        if class_name not in filter_names:
            continue

        confidence = float(conf)

        if args.debug:
            print('confidence:', confidence, 'confidence_threshold:', confidence_threshold, 'class_name:', class_name)

        if confidence < confidence_threshold:
            continue

        x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
        conf = "{:.2f}".format(conf)

        center_x = ((x1 + x2) // 2)
        center_y = ((y1 + y2) // 2)

        _x_min = min(_x_min, x1)
        _y_min = min(_y_min, y1)
        _w_max = max(_w_max, x2)
        _h_max = max(_h_max, y2)

        # Calculate top-left corner coordinates
        _boxes.append([x1, y1, x2, y2])
        _confidences.append(float(confidence))
        _class_ids.append(cls_id)
        _total_detections += 1

        if output_format == 'json':
            root["detections"].append({"box": [x1, y1, x2, y2], "confidence": round(float(confidence), 2), "class": classes[cls_id], "class_id": int(cls_id)})

        # draw the bounding boxes
        if args.debug:
            cv2.rectangle(image,(int(x1),int(y1)),(int(x2),int(y2)),(255,0, 0),1)
            cv2.putText(image, classes[cls_id]+' '+conf,(x1,y1-17), cv2.FONT_HERSHEY_SIMPLEX,0.7,(255,0,0),1)

    if len(_boxes) == 0:
        if output_format == 'json':
            root["error"] = "No people detected in the image."
            root["box"] = [0, 0, width, height]
            root["center"] = [width // 2, height // 2]
            root["offset"] = [0, 0]
            print(json.dumps(root))
        else:
            print("No people detected in the image.")
            print(r"box: 0,0,${width},${height}")
            print(r"center: ${width//2},${height//2}")
            print(r"offset: 0,0")
        sys.exit(0)
        return                
    
    # print the x,y coordinates of the center of the bounding box
    _center_x = (_x_min + _w_max) // 2
    _center_y = (_y_min + _h_max) // 2

    if output_format == 'json':
        root["box"] = [_x_min, _y_min, _w_max, _h_max]
        root["center"] = [_center_x, _center_y]
    else:
        print(f"box: {_x_min},{_y_min},{_w_max},{_h_max}")
        print(f"center: {_center_x},{_center_y}")


    # print the offset of center_x, center_y related to the image center
    _image_center_x = width // 2
    _image_center_y = height // 2
    _offset_x = _center_x - _image_center_x
    _offset_y = _center_y - _image_center_y

    if output_format == 'json':
        root["total_detections"] = _total_detections
        root["offset"] = [_offset_x, _offset_y]
        print(json.dumps(root, indent=2))
    else:
        print(f"offset: {_offset_x},{_offset_y}")

    if args.debug:
        # Draw center crosshair
        cv2.drawMarker(image, (_center_x, _center_y), (255, 0, 255), markerType=cv2.MARKER_CROSS, markerSize=50, thickness=4)

        # Draw the bounding box on the image
        cv2.rectangle(image, (_x_min, _y_min), (_w_max, _h_max), (255, 0, 255), 4)
        cv2.imshow("People Detection", image)


        # Wait for a key press and close the image window
        cv2.waitKey(0)
        cv2.destroyAllWindows()
        # now crop the image to the given bounding box, using the x_min, y_min, w_max, h_max coordinates as center of the cropped image
        # cv2.imshow("Cropped Image", image)
    cv2.destroyAllWindows()
    sys.exit(0)


if __name__ == "__main__":
    # Current script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Detect people in an image using YOLOv11.")
    parser.add_argument("--image", type=str, required=True, help="Path to the input image.")
    parser.add_argument("--model", type=str, default=script_dir + "/assets/yolo11n.onnx", help="Path to YOLOv11 model file.")
    parser.add_argument("--names", type=str, default=script_dir + "/assets/coco.names", help="Path to COCO names file.")
    parser.add_argument("--confidence", type=float, default=0.60, help="Minimum confidence threshold.")
    parser.add_argument("--output-format", type=str, choices=['text', 'json'], default='text', help="Output format: 'text' or 'json'.")
    parser.add_argument("--filter", type=str, default="person", help="Comma-separated list of class names to filter (e.g., 'person,car').")
    parser.add_argument("--debug", action="store_true", help="Enable debug mode. Will display images with detections.")

    args = parser.parse_args()
    _detect_people_yolo(args)