import cv2
import time
from flask import Flask, Response

app = Flask(__name__)

# This class acts as a bridge between your YOLO script and the web browser
class VideoStreamer:
    def __init__(self):
        self.output_frame = None

    def update_frame(self, frame):
        self.output_frame = frame

    def generate(self):
        while True:
            if self.output_frame is not None:
                # Encode at 50% quality to save Pi CPU
                _, encodedImage = cv2.imencode(".jpg", self.output_frame, [cv2.IMWRITE_JPEG_QUALITY, 50])
                yield (b'--frame\r\n' b'Content-Type: image/jpeg\r\n\r\n' + bytearray(encodedImage) + b'\r\n')
            time.sleep(0.1) # Throttle to ~10 FPS

streamer = VideoStreamer()

@app.route("/")
def video_feed():
    return Response(streamer.generate(), mimetype="multipart/x-mixed-replace; boundary=frame")

def run_server():
    # host 0.0.0.0 makes it accessible on your network
    app.run(host="0.0.0.0", port=5000, debug=False, use_reloader=False)
