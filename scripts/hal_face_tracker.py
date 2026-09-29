#!/usr/bin/env python3
"""Track the largest visible face and stream normalized gaze targets to HAL."""

from __future__ import annotations

import argparse
import signal
import socket
import sys
import time
from pathlib import Path

import cv2


DEFAULT_CASCADE = Path(
    "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="/dev/video0")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=45454)
    parser.add_argument("--cascade", type=Path, default=DEFAULT_CASCADE)
    parser.add_argument(
        "--no-mirror",
        action="store_true",
        help="Do not mirror horizontal movement for a user-facing camera.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if not args.cascade.is_file():
        print(f"HAL tracker: face model not found: {args.cascade}", file=sys.stderr)
        return 2

    detector = cv2.CascadeClassifier(str(args.cascade))
    if detector.empty():
        print(f"HAL tracker: could not load face model: {args.cascade}", file=sys.stderr)
        return 2

    capture = cv2.VideoCapture(args.device, cv2.CAP_V4L2)
    capture.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    capture.set(cv2.CAP_PROP_FPS, 30)
    capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    if not capture.isOpened():
        print(f"HAL tracker: could not open camera {args.device}", file=sys.stderr)
        return 3

    destination = (args.host, args.port)
    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    running = True

    def stop(_signum: int, _frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    smoothed_x = 0.0
    smoothed_y = 0.0
    frame_number = 0
    last_face: tuple[int, int, int, int] | None = None
    last_send = 0.0

    print(
        f"HAL tracker: watching {args.device} at 640x480; "
        f"sending to {args.host}:{args.port}",
        file=sys.stderr,
    )

    try:
        while running:
            ok, frame = capture.read()
            if not ok:
                print("HAL tracker: camera frame unavailable", file=sys.stderr)
                time.sleep(0.1)
                continue

            frame_number += 1
            if frame_number % 2 == 0:
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                gray = cv2.equalizeHist(gray)
                faces = detector.detectMultiScale(
                    gray,
                    scaleFactor=1.1,
                    minNeighbors=5,
                    minSize=(60, 60),
                    flags=cv2.CASCADE_SCALE_IMAGE,
                )
                last_face = (
                    max(faces, key=lambda face: int(face[2]) * int(face[3]))
                    if len(faces) > 0
                    else None
                )

            if last_face is None:
                continue

            x, y, width, height = last_face
            frame_height, frame_width = frame.shape[:2]
            target_x = ((x + width * 0.5) / frame_width) * 2.0 - 1.0
            target_y = ((y + height * 0.5) / frame_height) * 2.0 - 1.0

            if not args.no_mirror:
                target_x = -target_x

            target_x = max(-1.0, min(1.0, target_x))
            target_y = max(-1.0, min(1.0, target_y))

            smoothing = 0.30
            smoothed_x += (target_x - smoothed_x) * smoothing
            smoothed_y += (target_y - smoothed_y) * smoothing

            now = time.monotonic()
            if now - last_send >= 1.0 / 15.0:
                sender.sendto(
                    f"{smoothed_x:.4f} {smoothed_y:.4f}".encode("ascii"),
                    destination,
                )
                last_send = now
    finally:
        capture.release()
        sender.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
