#!/usr/bin/env python3
"""Record one local voice command and return one allow-listed action as JSON."""

from __future__ import annotations

import argparse
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import time


PROMPT = (
    "HAL voice commands: mute audio, unmute audio, volume up, volume down, "
    "play, pause, next track, previous track."
)


def normalize(transcript: str) -> str:
    text = transcript.lower().replace("’", "'")
    text = re.sub(r"[^a-z0-9' ]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip()
    # Whisper frequently hears the wake word HAL as “how”. Only discard these
    # words at the beginning; they never become executable input themselves.
    text = re.sub(r"^(?:hey |hello )?(?:hal|hall|how)\b[ ,]*", "", text).strip()
    return text


def classify(text: str) -> tuple[str, str]:
    # Specific negative forms must be checked before the substring “mute”.
    if re.search(r"\b(?:unmute|un mute|restore)(?: the)? (?:audio|sound)\b", text):
        return "unmute", "Audio restored, Starbadger."
    if re.search(r"\bmute(?: the)? (?:audio|sound)\b|\bsilence(?: the)? (?:audio|sound)\b", text):
        return "mute", "Audio muted, Starbadger."
    if re.search(r"\b(?:volume|audio|sound) up\b|\b(?:raise|increase)(?: the)? volume\b", text):
        return "volume_up", "Increasing the volume."
    if re.search(r"\b(?:volume|audio|sound) down\b|\b(?:lower|decrease)(?: the)? volume\b", text):
        return "volume_down", "Decreasing the volume."
    if re.search(r"\bnext(?: track| song)?\b|\bskip(?: track| song)?\b", text):
        return "next_track", "Advancing to the next track."
    if re.search(r"\bprevious(?: track| song)?\b|\bgo back(?: one track)?\b", text):
        return "previous_track", "Returning to the previous track."
    if re.search(r"\b(?:play|pause)(?: the)?(?: music| audio| track)?\b", text):
        return "play_pause", "Adjusting playback."
    return "", "I did not understand that command."


def record(source: str, duration: float, output_path: str) -> None:
    command = [
        "pw-record",
        f"--target={source}",
        "--rate=16000",
        "--channels=1",
        "--format=s16",
        output_path,
    ]
    process = subprocess.Popen(
        command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True
    )
    try:
        time.sleep(duration)
        process.send_signal(signal.SIGINT)
        _, error = process.communicate(timeout=3)
    except BaseException:
        process.kill()
        process.wait()
        raise
    if process.returncode not in (0, -signal.SIGINT):
        raise RuntimeError((error or "pw-record failed").strip())


def transcribe(path: str) -> str:
    from pywhispercpp.model import Model

    model_name = os.environ.get("DARKSPARK_WHISPER_MODEL", "base.en")
    model = Model(
        model_name,
        n_threads=max(1, min(6, os.cpu_count() or 1)),
        print_progress=False,
        print_realtime=False,
    )
    segments = model.transcribe(path, initial_prompt=PROMPT)
    return " ".join(segment.text.strip() for segment in segments).strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--duration", type=float, default=4.5)
    args = parser.parse_args()

    with tempfile.NamedTemporaryFile(prefix="darkspark-listen-", suffix=".wav") as audio:
        record(args.source, max(1.0, min(args.duration, 10.0)), audio.name)
        transcript = transcribe(audio.name)

    normalized = normalize(transcript)
    command, response = classify(normalized)
    print(
        json.dumps(
            {
                "transcript": transcript,
                "normalized": normalized,
                "command": command,
                "response": response,
            },
            ensure_ascii=True,
        ),
        flush=True,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"HAL voice command failed: {error}", file=sys.stderr)
        raise SystemExit(1)
