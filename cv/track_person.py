from ultralytics import YOLO
import cv2, time, imutils, threading
#import mediapipe as mp
import numpy as np
from centroid_tracker import CentroidTracker
import serial
from flask_stream_setup import streamer, run_server

MAX_LOST_FRAMES = 300
FRAME_DIM_SIZE = 640
FOCAL_LENGTH = 816     # Calibrated for Brio 105 @ 640x640
REAL_WIDTH = 0.45      # Meters (average human shoulder width)

WEB_STREAM_ENABLED = True
CV_SHOW_ENABLED = False
SER_WRITE_ENABLED = True

def export_model_yolo():
    model = YOLO('yolov8n.pt')
    model.export(format = 'ncnn', half = True)

def camera_setup():
    print('starting camera setup')
    cam = cv2.VideoCapture(0)
    wait_time_for_camera = 90 # 90 sec
    curr_wait = time.time()
    while True:
        if not cam.isOpened():
            if (time.time() - curr_wait) > wait_time_for_camera:
                raise Exception('Camera took too long to init')
            time.sleep(5)
        else:
            break
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_DIM_SIZE)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_DIM_SIZE)
    print(f'Camera inited in {(time.time() - curr_wait)} seconds')
    return cam

def camera_click_pic(cam):
    res, frame = cam.read()
    if not res:
        print('WARNING - issue with pic click')
        return None
    else:
        return frame

def basic_yolo_tracking_standalone(resave_model = False):
    if resave_model:
        export_model_yolo()
        print('model saved')

    model = YOLO('yolov8n_ncnn_model', task='detect')
    print('model loaded. starting camera init')
    cam = camera_setup()

    curr_error = 0
    error_thres = 5

    while True:
        frame = camera_click_pic(cam)
        if frame is None:
            curr_error += 1
            if curr_error > error_thres:
                raise Exception('Pic click failed 5 times. killling')
            continue
        results = model.track(
            source=frame, 
            persist=True, 
            imgsz=640, 
            tracker="yolo_tracker_custom.yaml", 
            classes=0, 
            stream=True
        )
        for r in results:
            # Use the built-in plotter to see the IDs on screen
            annotated_frame = r.plot()
            cv2.imshow("NCNN Person Re-ID", annotated_frame)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cam.release()
    cv2.destroyAllWindows()

def yolo_reid_rp(model, frame_og, ser, locked_id, lost_counter, last_known_center):
    frame = cv2.rotate(frame_og, cv2.ROTATE_90_COUNTERCLOCKWISE)
    results = model.track(
        source=frame, 
        persist=True, 
        imgsz=FRAME_DIM_SIZE, 
        tracker="yolo_tracker_custom.yaml", 
        classes=0, 
        iou = 0.45,
        device = 'cpu',
        conf = 0.25,
        stream=False
    )
    current_targets = {}
    distance = 2.0
    turn_error = 0.0
    for r in results:
        print('\t st iter -',  locked_id)
        # Use the built-in plotter to see the IDs on screen
        if r.boxes.id is not None:
            # {id: [x1, y1, x2, y2, center_x, area]}
            for box, track_id in zip(r.boxes.xyxy.cpu().numpy(), r.boxes.id.cpu().numpy().astype(int)):
                cx = (box[0] + box[2]) / 2
                area = (box[2] - box[0]) * (box[3] - box[1])
                current_targets[track_id] = {'box': box, 'cx': cx, 'area': area}

        # 1. INITIAL LOCK: Find biggest person
        if locked_id is None:
            locked_id = max(current_targets, key=lambda k: current_targets[k]['area'])
            lost_counter = 0
            print(f"LOCKED: ID {locked_id}")

        # 2. FOLLOW LOGIC
        if locked_id in current_targets:
            lost_counter = 0
            last_known_center = current_targets[locked_id]['cx']
            
            # FIXED: Center is now 320 (640 / 2)
            turn_error = (FRAME_DIM_SIZE // 2) - last_known_center
            # Distance (m) = (Real Width * Focal Length) / Pixel Width
            distance = (REAL_WIDTH * FOCAL_LENGTH) / max(current_targets[locked_id]['width'], 1) 
            
            # UNCOMMENT to move:
            print(f"TRACKING ID {locked_id} | Error: {turn_error:.2f} | Distance: {distance:.2f}")
            if SER_WRITE_ENABLED and ser and ser.is_open():
                message = f"{turn_error:.2f}, {distance:.2f}\n"
                ser.write(message.encode('utf-8'))

        # 3. RE-ID RECOVERY
        elif locked_id is not None:
            lost_counter += 1

            if last_known_center is not None:
                if SER_WRITE_ENABLED and ser and ser.is_open():
                    # Decay the error: Turn at 30% intensity of the last known position
                    turn_error = (last_known_center - 320) * 0.3
                    
                    # Use the last distance calculation (we'll assume they are still there)
                    # We'll use the 'distance' variable from the last successful frame
                    # (Ensure 'distance' is initialized to a safe value like 2.0 at the top of your script)
                    message = f"{turn_error:.2f}, {distance:.2f}\n"
                    ser.write(message.encode('utf-8'))

                best_new_id = None
                min_dist = 100 # 100px search radius
                for tid, data in current_targets.items():
                    dist = abs(data['cx'] - last_known_center)
                    if dist < min_dist:
                        min_dist = dist
                        best_new_id = tid

                if best_new_id:
                    print(f"RE-ID SUCCESS: {locked_id} -> {best_new_id}")
                    locked_id = best_new_id
                    lost_counter = 0
                    break

            if lost_counter > MAX_LOST_FRAMES:
                print("10s TIMEOUT: Target lost.")
                # if SER_WRITE_ENABLED and ser and ser.is_open():
                #     message = "S\n" # to stop
                #     ser.write(message.encode('utf-8'))
                locked_id = None
                last_known_center = None


        annotated_frame = r.plot()
        if locked_id in current_targets:
            target = current_targets[locked_id]
            box = target['box'].astype(int)
            
            # Draw a thicker GREEN border for the locked target
            cv2.rectangle(annotated_frame, (box[0], box[1]), (box[2], box[3]), (0, 255, 0), 4)
            
            # Create the label string with distance
            # distance is calculated in your follow logic
            label = f"LOCKED ID:{locked_id} | {distance:.2f}m"
            
            # Draw a background label box for readability
            (w, h), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 2)
            cv2.rectangle(annotated_frame, (box[0], box[1] - 25), (box[0] + w, box[1]), (0, 255, 0), -1)
            
            # Write the text
            cv2.putText(annotated_frame, label, (box[0], box[1] - 7), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)
        if CV_SHOW_ENABLED:
            cv2.imshow("NCNN Person Re-ID", annotated_frame)
        if WEB_STREAM_ENABLED:
            streamer.update_frame(annotated_frame)
        print('\t end iter -',  locked_id)
    return locked_id, lost_counter, last_known_center

def mediapipe_reid_rp(mp_drawing, detector, frame, ct_tracker = None):
    rects = []
    results = detector.process(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
    if results.detections:
        for det in results.detections:
            if ct_tracker is None:
                mp_drawing.draw_detection(frame, det)
            else:
                bbox = det.location_data.relative_bounding_box
                h, w, _ = frame.shape
                x1, y1 = int(bbox.xmin * w), int(bbox.ymin * h)
                x2, y2 = int((bbox.xmin + bbox.width) * w), int((bbox.ymin + bbox.height) * h)
                rects.append((x1, y1, x2, y2))
        if ct_tracker is not None:
            objects = ct_tracker.update(rects)
            for (object_id, centroid) in objects.items():
                text = f"ID {object_id}"
                cv2.putText(frame, text, (centroid[0]-10, centroid[1]-10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
                cv2.circle(frame, (centroid[0], centroid[1]), 4, (0, 255, 0), -1)
    cv2.imshow('Mediapipe tracker', frame)
    return

def haar_cascade_rp(full_cascade, upper_cascade, lower_cascade, frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    detections = []
    detections.extend(full_cascade.detectMultiScale(gray, 1.1, 3))
    detections.extend(upper_cascade.detectMultiScale(gray, 1.1, 3))
    detections.extend(lower_cascade.detectMultiScale(gray, 1.1, 3))

    largest_box = None
    max_area = 0

    # 2. Find the single largest "part" detected
    for (x, y, w, h) in detections:
        area = w * h
        if area > max_area:
            max_area = area
            largest_box = (x, y, w, h)

    # 3. Use that box for the Robot
    if largest_box:
        x, y, w, h = largest_box
        center_x = x + (w // 2)
        turn_error = center_x - 160
        
        cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)
        print(f"Tracking largest part | Error: {turn_error}")
    
    cv2.imshow('Fused Detection', frame)
    return

def hog_rp(hog, frame):
    (rects, weights) = hog.detectMultiScale(frame, winStride=(8, 8), 
                                            padding=(8, 8), scale=1.05)

    # 4. Filter and draw (HOG often detects the same person multiple times)
    for (x, y, w, h) in rects:
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 0, 255), 2)
        # Calculate your robot steering here (center_x = x + w//2)

    cv2.imshow("HOG Robot View", frame)
    return

def pi_optimized_tracking(tracking_type = 'yolo', test_mode = True):
    if tracking_type == 'yolo':
        export_model_yolo()
        print('model saved')
        model = YOLO('yolov8n_ncnn_model', task='detect')
    # elif tracking_type == 'mediapipe':
    #     mp_drawing = mp.solutions.drawing_utils
    #     mp_object_detection = mp.solutions.object_detection
    #     ct = CentroidTracker(max_disappeared=90)
    elif tracking_type == 'haar':
        full_cascade = cv2.CascadeClassifier('haarcascade_fullbody.xml')
        upper_cascade = cv2.CascadeClassifier('haarcascade_upperbody.xml')
        lower_cascade = cv2.CascadeClassifier('haarcascade_lowerbody.xml')
    elif tracking_type == 'hog':
        hog = cv2.HOGDescriptor()
        hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())
    else:
        print('bad type', tracking_type)
        return
    cam = camera_setup()

    ser = None
    if SER_WRITE_ENABLED:
        print('Starting serial comm setup')
        ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
        time.sleep(2)
        print('Starting serial comm setup done')

    cam_pic_click_error_count = 0
    cam_pic_click_error_thresh = 5

    locked_id = None
    lost_counter = 0
    last_known_center = None
    if WEB_STREAM_ENABLED:
        threading.Thread(target=run_server, daemon=True).start()
    while True:
        frame = camera_click_pic(cam)
        if frame is None:
            cam_pic_click_error_count += 1
            if cam_pic_click_error_count > cam_pic_click_error_thresh:
                raise Exception('Pic click failed 5 times. killling')
            continue

        if tracking_type == 'yolo':
            locked_id, lost_counter, last_known_center = yolo_reid_rp(model, frame, ser, locked_id, lost_counter, last_known_center)
        # elif tracking_type == 'mediapipe':
        #     with mp_object_detection.ObjectDetection(model_selection=0, min_detection_confidence=0.4) as detector:
        #         mediapipe_reid_rp(mp_drawing, detector, frame)
        elif tracking_type == 'haar':
            haar_cascade_rp(full_cascade, upper_cascade, lower_cascade, frame)
        elif tracking_type == 'hog':
            frame = imutils.resize(frame, width=min(400, frame.shape[1]))
            hog_rp(hog, frame)
        if CV_SHOW_ENABLED:
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    cam.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    pi_optimized_tracking(tracking_type='yolo')

