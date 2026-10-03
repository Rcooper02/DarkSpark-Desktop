#!/usr/bin/env python3
"""Local PCM activity detection and optional Whisper-based HAL wake phrase."""
import argparse
from array import array
from collections import deque
import contextlib
import json
import math
import os
import re
import subprocess
import sys
import tempfile
import threading
import time
import wave

RATE = 16000
FRAME_SECONDS = .02
FRAME_BYTES = 640
WAKE = re.compile(
    r'^\s*(?:'
    r'(?:(?:hey|okay|ok)\s+)?(?:hal|hall|hell|cal)'
    r'|(?:hello|hey|okay|ok)\s+pal'
    r')'
    r'(?=[\s,.:!?]|$)[\s,.:!?]*(.*)$',
    re.I
)


def wake_command(text):
    match = WAKE.fullmatch(text.strip())
    return match.group(1).strip() if match else None


def rms(frame):
    samples = array('h', frame)
    if sys.byteorder != 'little':
        samples.byteswap()
    return math.sqrt(sum(x*x for x in samples) / max(1, len(samples)))


class Segmenter:
    def __init__(self, threshold=450, silence=.8, maximum=12):
        self.threshold = threshold
        self.silence_frames = int(silence / FRAME_SECONDS)
        self.maximum_frames = int(maximum / FRAME_SECONDS)
        self.reset()

    def reset(self):
        self.pre = deque(maxlen=15)
        self.frames = []
        self.quiet = 0
        self.voiced = 0

    def feed(self, frame):
        loud = rms(frame) >= self.threshold
        if not self.frames:
            self.pre.append(frame)
            if not loud:
                return None
            self.frames = list(self.pre)
            self.voiced = 1
        else:
            self.frames.append(frame)
            self.voiced += int(loud)
        self.quiet = 0 if loud else self.quiet + 1
        if self.quiet < self.silence_frames and len(self.frames) < self.maximum_frames:
            return None
        result = b''.join(self.frames) if self.voiced >= 10 else b''
        self.reset()
        return result


def microphone_names(device):
    if device != 'all':
        return [device]
    result = subprocess.run(['pactl', '-f', 'json', 'list', 'sources'],
                            capture_output=True, text=True, check=True)
    names = [source['name'] for source in json.loads(result.stdout)
             if not source['name'].endswith('.monitor')]
    if not names:
        raise RuntimeError('No microphone inputs were found')
    return names


class Microphones:
    """Bounded per-device queues; select one mic for each whole utterance."""
    def __init__(self, names):
        self.devices = []
        self.lock = threading.Lock()
        self.stopped = threading.Event()
        self.next_frame = time.monotonic()
        for name in names:
            process = subprocess.Popen([
                'parec',
                '--device='+name,
                '--format=s16le',
                '--rate=16000',
                '--channels=1',
                '--latency-msec=20',
                '--process-time-msec=20',
                '--raw'
            ], stdout=subprocess.PIPE, stderr=sys.stderr)
            device = {'name': name, 'process': process, 'frames': deque(maxlen=3), 'ended': False}
            self.devices.append(device)
            threading.Thread(target=self.reader, args=(device,), daemon=True).start()

    def reader(self, device):
        while not self.stopped.is_set():
            frame = device['process'].stdout.read(FRAME_BYTES)
            with self.lock:
                if len(frame) != FRAME_BYTES:
                    device['ended'] = True
                    return
                device['frames'].append(frame)

    def read(self, selected=None):
        self.next_frame += FRAME_SECONDS
        self.stopped.wait(max(0, self.next_frame-time.monotonic()))
        if self.next_frame < time.monotonic() - .1:
            self.next_frame = time.monotonic()

        while True:
            with self.lock:
                if all(d['ended'] for d in self.devices):
                    raise RuntimeError('All microphone streams ended')

                # Once a microphone has been selected for an utterance,
                # wait for THAT microphone. Never silently fall back to
                # another device.
                if selected is not None:
                    device = next(
                        (d for d in self.devices if d['name'] == selected),
                        None
                    )

                    if device is None:
                        raise RuntimeError(
                            f'Selected microphone disappeared: {selected}'
                        )

                    if device['ended']:
                        raise RuntimeError(
                            f'Selected microphone stream ended: {selected}'
                        )

                    if device['frames']:
                        return selected, device['frames'].popleft()

                else:
                    frames = {
                        d['name']: d['frames'].popleft()
                        for d in self.devices
                        if not d['ended'] and d['frames']
                    }

                    if frames:
                        name = max(
                            frames,
                            key=lambda key: rms(frames[key])
                        )
                        return name, frames[name]

            if self.stopped.wait(0.002):
                raise RuntimeError('Microphone capture stopped')

    def close(self):
        self.stopped.set()
        for device in self.devices:
            process = device['process']
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()


def emit(event, **fields):
    print(json.dumps({'event': event, **fields}), flush=True)


class WakeDetector:
    """Streaming openWakeWord detector fed by 16 kHz mono s16 PCM."""

    CHUNK_BYTES = 2560  # 80 ms @ 16 kHz, 16-bit mono

    def __init__(self):
        import numpy as np
        from openwakeword.model import Model as WakeModel

        self.np = np
        self.buffer = bytearray()
        self.threshold = float(os.environ.get(
            'DARKSPARK_WAKE_THRESHOLD', '0.5'
        ))
        self.model_name = os.environ.get(
            'DARKSPARK_WAKE_MODEL', 'hey_jarvis'
        )

        self.model = WakeModel(
            wakeword_models=[self.model_name],
            inference_framework='onnx'
        )

    def reset(self):
        self.buffer.clear()
        reset = getattr(self.model, 'reset', None)
        if callable(reset):
            reset()

    def feed(self, frame):
        self.buffer.extend(frame)

        while len(self.buffer) >= self.CHUNK_BYTES:
            chunk = bytes(self.buffer[:self.CHUNK_BYTES])
            del self.buffer[:self.CHUNK_BYTES]

            audio = self.np.frombuffer(chunk, dtype=self.np.int16)
            scores = self.model.predict(audio)

            score = max(scores.values()) if scores else 0.0

            if score >= self.threshold:
                print(
                    f"[wake-debug] {self.model_name} score={score:.3f}",
                    file=sys.stderr,
                    flush=True
                )
                self.reset()
                return True

        return False


def run(args):
    from pywhispercpp.model import Model

    with contextlib.redirect_stdout(sys.stderr):
        model = Model(
            'base.en',
            n_threads=4,
            print_progress=False,
            print_realtime=False
        )

    lock = threading.Lock()
    state = {
        'paused': False,
        'manual': False,
        'deadline': 0.,
        'generation': 0,
        'stop': False,
        'awaiting_wake_silence': False,
        'wake_capture_after': 0.0,
        'wake_pending': False,
        'wake_device': None
    }

    def commands():
        for line in sys.stdin:
            with lock:
                cmd = line.strip()

                if cmd == 'pause':
                    state.update(
                        paused=True,
                        manual=False,
                        deadline=0.
                    )
                    state['generation'] += 1

                elif cmd == 'resume':
                    state['paused'] = False
                    state['generation'] += 1

                elif cmd == 'listen' and not state['paused']:
                    state.update(
                        manual=True,
                        deadline=time.monotonic() + 8,
                        awaiting_wake_silence=False,
                        wake_capture_after=0.0,
                        wake_pending=False,

                        # The wake microphone is useful only for wake detection.
                        # For the actual command, allow all microphones again
                        # and lock onto whichever one hears speech first.
                        wake_device=None
                    )
                    state['generation'] += 1
                    emit('listening')

                elif cmd == 'stop':
                    state['stop'] = True
                    break

        with lock:
            state['stop'] = True

    threading.Thread(target=commands, daemon=True).start()

    capture = Microphones(microphone_names(args.device))

    # Each microphone needs its own streaming wake detector so audio
    # from different devices is never combined into one wake-word buffer.
    wake_detectors = {
        device['name']: WakeDetector()
        for device in capture.devices
    } if args.wake else {}

    selected = None
    segmenter = Segmenter(args.threshold)
    generation = -1

    emit(
        'ready',
        wake=args.wake,
        microphones=[d['name'] for d in capture.devices]
    )

    try:
        while True:
            name, frame = capture.read(selected)

            with lock:
                snapshot = state.copy()

            if snapshot['stop']:
                break

            if generation != snapshot['generation']:
                segmenter.reset()
                selected = (
                    snapshot.get('wake_device')
                    if snapshot.get('manual')
                    else None
                )
                generation = snapshot['generation']

                for detector in wake_detectors.values():
                    detector.reset()

            if snapshot['paused']:
                segmenter.reset()
                selected = None
                continue

            #
            # Wake mode:
            # openWakeWord listens continuously. Whisper is NOT involved
            # until the dedicated wake detector fires.
            #
            if (
                args.wake
                and not snapshot['manual']
                and not snapshot.get('wake_pending')
            ):
                if wake_detectors[name].feed(frame):
                    print(
                        f"[wake-debug] wake device={name} rms={rms(frame):.1f}",
                        file=sys.stderr,
                        flush=True
                    )
                    with lock:
                        if (
                            not state['paused']
                            and state['generation'] == generation
                        ):
                            state.update(
                                manual=False,
                                deadline=0.0,
                                awaiting_wake_silence=False,
                                wake_capture_after=0.0,
                                wake_pending=True,
                                wake_device=name
                            )
                            state['generation'] += 1
                            emit('wake_detected', device=name)

                    segmenter.reset()
                    selected = name

                continue

            #
            # Without wake mode, only explicit Listen should capture.
            #
            if not args.wake and not snapshot['manual']:
                segmenter.reset()
                selected = None
                continue

            #
            # Wake has fired (or Listen was clicked).
            # For a wake-triggered capture, first wait until the wake phrase
            # itself has ended so Whisper does not transcribe "Hey Jarvis".
            #
            if snapshot['manual'] and snapshot.get('awaiting_wake_silence'):
                if time.monotonic() >= snapshot.get('wake_capture_after', 0.0):
                    with lock:
                        if state['generation'] == generation:
                            state['awaiting_wake_silence'] = False
                            state['deadline'] = time.monotonic() + 8
                else:
                    segmenter.reset()
                    selected = snapshot.get('wake_device') or name
                    continue

            #
            # Capture the actual command using the existing VAD.
            #
            if (
                snapshot['manual']
                and time.monotonic() > snapshot['deadline']
                and not segmenter.frames
            ):
                with lock:
                    if state['generation'] == generation:
                        state.update(
                            manual=False,
                            deadline=0.
                        )
                        emit('transcript', text='')
                continue

            audio = segmenter.feed(frame)

            if segmenter.frames:
                selected = name
            else:
                selected = snapshot.get('wake_device')

            if not audio:
                continue

            emit('transcribing')

            with tempfile.NamedTemporaryFile(suffix='.wav') as temporary:
                with wave.open(temporary.name, 'wb') as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(2)
                    wav.setframerate(RATE)
                    wav.writeframes(audio)

                with contextlib.redirect_stdout(sys.stderr):
                    segments = model.transcribe(temporary.name)

                text = ' '.join(
                    segment.text.strip()
                    for segment in segments
                ).strip()

            text = re.sub(
                r'\[[^\]]*\]|<\|[^>]*\|>',
                '',
                text
            ).strip()

            # Ignore short wake-word remnants accidentally captured by Whisper.
            normalized = re.sub(r'[^a-z ]+', ' ', text.lower())
            normalized = ' '.join(normalized.split())

            if normalized in {
                'hal',
                'hall',
                'hey hal',
                'hey hall',
                'hey al',
                'hey how',
                'okay amy',
            }:
                print(
                    f"[wake-debug] discarded wake remnant: {text!r}",
                    file=sys.stderr,
                    flush=True
                )
                text = ''

            with lock:
                if (
                    state['paused']
                    or state['generation'] != generation
                ):
                    continue

                state.update(
                    manual=False,
                    deadline=0.
                )

                emit('transcript', text=text)

    finally:
        capture.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--device', required=True)
    parser.add_argument('--wake', action='store_true')
    parser.add_argument('--threshold', type=float, default=float(os.environ.get('DARKSPARK_VAD_THRESHOLD', '450')))
    args = parser.parse_args()
    try:
        run(args)
    except Exception as error:
        emit('error', message=str(error))
        sys.exit(1)
