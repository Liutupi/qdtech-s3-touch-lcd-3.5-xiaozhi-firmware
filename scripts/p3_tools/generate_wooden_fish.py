#!/usr/bin/env python3
"""Prepare a real mokugyo strike for the firmware notification asset.

Reference recording: "Mokugyo drum sounds" by George Papargyris,
licensed CC BY 4.0.
https://freesound.org/people/George_Papargyris/sounds/837177/

The source is a late-Edo-era mokugyo struck repeatedly with a mallet. This
script isolates the final clean strike, keeps its authentic low wooden cavity
tone, and removes the room tail for a short embedded notification.
"""

import argparse
import subprocess
import wave


SAMPLE_RATE = 16000
DURATION_SECONDS = 0.18


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Downloaded CC0 reference WAV/MP3")
    parser.add_argument("output", help="Output mono 16 kHz WAV path")
    args = parser.parse_args()

    subprocess.run(
        [
            "ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
            "-i", args.input,
            "-af", (
                "atrim=start=2.475:end=2.655,asetpts=PTS-STARTPTS,"
                "volume=6dB,afade=t=out:st=0.135:d=0.045"
            ),
            "-ac", "1", "-ar", str(SAMPLE_RATE), "-c:a", "pcm_s16le",
            args.output,
        ],
        check=True,
    )

    with wave.open(args.output, "rb") as output:
        if output.getnchannels() != 1 or output.getsampwidth() != 2:
            raise ValueError("prepared sound must be mono 16-bit PCM")
        if output.getframerate() != SAMPLE_RATE:
            raise ValueError("prepared sound must be 16 kHz")
        frames = output.getnframes()

    expected_frames = int(SAMPLE_RATE * DURATION_SECONDS)
    if frames != expected_frames:
        raise ValueError(f"expected {expected_frames} frames, got {frames}")
    print(f"prepared late-Edo mokugyo {args.output}: {frames} samples, {DURATION_SECONDS:.2f}s")


if __name__ == "__main__":
    main()
