#!/usr/bin/env python3
"""Generate a short, deterministic comic impact sound for Lucky Revolver."""

import argparse
import math
import random
import wave
from array import array


SAMPLE_RATE = 16000
DURATION_SECONDS = 0.72


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def generate_samples() -> array:
    rng = random.Random(0x18814)
    count = int(SAMPLE_RATE * DURATION_SECONDS)
    mixed: list[float] = []
    previous_noise = 0.0

    for index in range(count):
        t = index / SAMPLE_RATE
        noise = rng.uniform(-1.0, 1.0)
        bright_noise = noise - previous_noise * 0.72
        previous_noise = noise

        # A fast comic crack followed by a speaker-friendly low punch.
        crack = bright_noise * math.exp(-t * 58.0) * 1.10
        body_phase = 2.0 * math.pi * (132.0 * t - 42.0 * t * t)
        body = math.sin(body_phase) * math.exp(-t * 7.2) * 0.92
        body += math.sin(body_phase * 2.03) * math.exp(-t * 10.0) * 0.30

        # Short metallic cylinder ring; deliberately stylised, not realistic.
        ring_phase = 2.0 * math.pi * (1180.0 * t - 520.0 * t * t)
        ring = math.sin(ring_phase) * math.exp(-t * 8.8) * 0.29
        ring += math.sin(2.0 * math.pi * 690.0 * t) * math.exp(-t * 11.5) * 0.18

        echoes = 0.0
        for delay, amplitude, frequency in (
            (0.165, 0.34, 560.0),
            (0.315, 0.22, 470.0),
            (0.475, 0.13, 390.0),
        ):
            u = t - delay
            if u >= 0.0:
                echoes += amplitude * math.sin(2.0 * math.pi * frequency * u) * math.exp(-u * 13.0)
                echoes += amplitude * 0.55 * bright_noise * math.exp(-u * 62.0)

        value = crack + body + ring + echoes
        value = math.tanh(value * 1.22)
        if t > DURATION_SECONDS - 0.08:
            value *= clamp((DURATION_SECONDS - t) / 0.08, 0.0, 1.0)
        mixed.append(value)

    peak = max(abs(value) for value in mixed) or 1.0
    scale = 0.92 * 32767.0 / peak
    return array("h", (int(clamp(value * scale, -32767.0, 32767.0)) for value in mixed))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", help="Output mono 16 kHz WAV path")
    args = parser.parse_args()

    samples = generate_samples()
    with wave.open(args.output, "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(samples.tobytes())

    print(f"generated {args.output}: {len(samples)} samples, {DURATION_SECONDS:.2f}s")


if __name__ == "__main__":
    main()
