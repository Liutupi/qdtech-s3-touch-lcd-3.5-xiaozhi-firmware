#!/usr/bin/env python3
"""Convert a mono 16 kHz PCM WAV file to the firmware's P3 stream format."""

import argparse
import ctypes.util
import os
import struct
import wave


SAMPLE_RATE = 16000
FRAME_SAMPLES = 960
FRAME_BYTES = FRAME_SAMPLES * 2


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="16 kHz mono 16-bit PCM WAV")
    parser.add_argument("output", help="Output P3 stream")
    parser.add_argument("--opus-dll", help="Optional explicit path to opus.dll")
    parser.add_argument("--bitrate", type=int, default=32000)
    args = parser.parse_args()

    if args.opus_dll:
        opus_dll = os.path.abspath(args.opus_dll)
        original_find_library = ctypes.util.find_library
        ctypes.util.find_library = (
            lambda name: opus_dll if name == "opus" else original_find_library(name)
        )

    import opuslib  # Imported after the optional DLL override.

    with wave.open(args.input, "rb") as source:
        if source.getnchannels() != 1:
            raise ValueError("input must be mono")
        if source.getsampwidth() != 2:
            raise ValueError("input must be 16-bit PCM")
        if source.getframerate() != SAMPLE_RATE:
            raise ValueError("input must be 16 kHz")
        pcm = source.readframes(source.getnframes())

    encoder = opuslib.Encoder(SAMPLE_RATE, 1, opuslib.APPLICATION_AUDIO)
    encoder.bitrate = args.bitrate
    packets: list[bytes] = []
    for offset in range(0, len(pcm), FRAME_BYTES):
        frame = pcm[offset:offset + FRAME_BYTES]
        if len(frame) < FRAME_BYTES:
            frame += bytes(FRAME_BYTES - len(frame))
        packets.append(encoder.encode(frame, FRAME_SAMPLES))

    with open(args.output, "wb") as target:
        for packet in packets:
            target.write(struct.pack(">BBH", 0, 0, len(packet)))
            target.write(packet)

    # Decode every packet once so a malformed asset fails before firmware build.
    decoder = opuslib.Decoder(SAMPLE_RATE, 1)
    decoded_bytes = sum(len(decoder.decode(packet, FRAME_SAMPLES)) for packet in packets)
    output_size = os.path.getsize(args.output)
    duration = len(packets) * 0.060
    print(
        f"encoded {args.output}: {len(packets)} frames, {duration:.2f}s, "
        f"{output_size} bytes; decoded validation={decoded_bytes} bytes"
    )
    del decoder
    del encoder


if __name__ == "__main__":
    main()
