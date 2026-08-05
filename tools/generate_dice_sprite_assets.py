#!/usr/bin/env python3
"""Generate deterministic full-screen stage and anti-aliased 3D dice sprites.

The firmware consumes raw RGB565 and ARGB8888 data directly from the SD card.
All expensive geometry, lighting, beveling, shadows, and pip projection happen
offline here so the ESP32 only swaps image descriptors during a roll.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "sdcard" / "shake_lab" / "dice"
STAGE_SOURCE = ROOT / "tools" / "dice-stage-source.png"

STAGE_SIZE = (480, 320)
SPRITE_SIZE = 96
SUPERSAMPLE = 4
ROLL_FRAMES = 12


@dataclass(frozen=True)
class Face:
    value: int
    normal: np.ndarray
    u_axis: np.ndarray
    v_axis: np.ndarray


def vec(x: float, y: float, z: float) -> np.ndarray:
    return np.array([x, y, z], dtype=np.float64)


# Opposite faces sum to seven. Axis direction only controls pip orientation.
FACES = (
    Face(1, vec(0, 0, 1), vec(1, 0, 0), vec(0, 1, 0)),
    Face(6, vec(0, 0, -1), vec(-1, 0, 0), vec(0, 1, 0)),
    Face(2, vec(0, 1, 0), vec(1, 0, 0), vec(0, 0, -1)),
    Face(5, vec(0, -1, 0), vec(1, 0, 0), vec(0, 0, 1)),
    Face(3, vec(1, 0, 0), vec(0, 0, -1), vec(0, 1, 0)),
    Face(4, vec(-1, 0, 0), vec(0, 0, 1), vec(0, 1, 0)),
)

PIPS = {
    1: ((0.0, 0.0),),
    2: ((-0.48, -0.48), (0.48, 0.48)),
    3: ((-0.5, -0.5), (0.0, 0.0), (0.5, 0.5)),
    4: ((-0.48, -0.48), (0.48, -0.48), (-0.48, 0.48), (0.48, 0.48)),
    5: ((-0.5, -0.5), (0.5, -0.5), (0.0, 0.0), (-0.5, 0.5), (0.5, 0.5)),
    6: ((-0.5, -0.52), (0.5, -0.52), (-0.5, 0.0), (0.5, 0.0),
        (-0.5, 0.52), (0.5, 0.52)),
}


def rotation_x(angle: float) -> np.ndarray:
    c, s = math.cos(angle), math.sin(angle)
    return np.array(((1, 0, 0), (0, c, -s), (0, s, c)), dtype=np.float64)


def rotation_y(angle: float) -> np.ndarray:
    c, s = math.cos(angle), math.sin(angle)
    return np.array(((c, 0, s), (0, 1, 0), (-s, 0, c)), dtype=np.float64)


def rotation_z(angle: float) -> np.ndarray:
    c, s = math.cos(angle), math.sin(angle)
    return np.array(((c, -s, 0), (s, c, 0), (0, 0, 1)), dtype=np.float64)


def rotation_axis(axis: np.ndarray, angle: float) -> np.ndarray:
    axis = axis / np.linalg.norm(axis)
    x, y, z = axis
    c, s, q = math.cos(angle), math.sin(angle), 1.0 - math.cos(angle)
    return np.array((
        (c + x*x*q, x*y*q - z*s, x*z*q + y*s),
        (y*x*q + z*s, c + y*y*q, y*z*q - x*s),
        (z*x*q - y*s, z*y*q + x*s, c + z*z*q),
    ), dtype=np.float64)


def align_vectors(source: np.ndarray, target: np.ndarray) -> np.ndarray:
    source = source / np.linalg.norm(source)
    target = target / np.linalg.norm(target)
    cross = np.cross(source, target)
    dot = float(np.clip(np.dot(source, target), -1.0, 1.0))
    length = float(np.linalg.norm(cross))
    if length < 1e-8:
        if dot > 0:
            return np.identity(3)
        helper = vec(1, 0, 0) if abs(source[0]) < 0.8 else vec(0, 1, 0)
        return rotation_axis(np.cross(source, helper), math.pi)
    return rotation_axis(cross / length, math.acos(dot))


def face_corners(face: Face) -> list[np.ndarray]:
    n, u, v = face.normal, face.u_axis, face.v_axis
    return [n - u - v, n + u - v, n + u + v, n - u + v]


def pip_polygon(face: Face, u: float, v: float, radius: float = 0.135) -> list[np.ndarray]:
    center = face.normal + face.u_axis * u + face.v_axis * v
    return [
        center + face.u_axis * (math.cos(a) * radius) +
        face.v_axis * (math.sin(a) * radius)
        for a in np.linspace(0.0, math.tau, 18, endpoint=False)
    ]


def project(point: np.ndarray, matrix: np.ndarray, center: tuple[float, float],
            scale: float) -> tuple[float, float]:
    p = matrix @ point
    # A light perspective term avoids the perfectly parallel look of an
    # isometric drawing while remaining stable for small sprites.
    perspective = 1.0 + p[2] * 0.045
    return (center[0] + p[0] * scale * perspective,
            center[1] - p[1] * scale * perspective)


def tone(base: tuple[int, int, int], multiplier: float) -> tuple[int, int, int, int]:
    return tuple(max(0, min(255, int(channel * multiplier))) for channel in base) + (255,)


def render_die(matrix: np.ndarray, lift: float, lucky_face: int | None = None) -> Image.Image:
    size = SPRITE_SIZE * SUPERSAMPLE
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    scale = 23.0 * SUPERSAMPLE
    center = (size / 2.0, (50.5 - lift) * SUPERSAMPLE)

    # Fixed ground shadow while the cube rises and rotates above it.
    shadow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    altitude = min(1.0, lift / 12.0)
    shadow_w = (48.0 - altitude * 13.0) * SUPERSAMPLE
    shadow_h = (10.0 - altitude * 2.5) * SUPERSAMPLE
    sx, sy = size / 2.0, 80.0 * SUPERSAMPLE
    sd.ellipse((sx-shadow_w/2, sy-shadow_h/2, sx+shadow_w/2, sy+shadow_h/2),
               fill=(3, 33, 34, int(112 - altitude * 48)))
    shadow = shadow.filter(ImageFilter.GaussianBlur(3.1 * SUPERSAMPLE))
    image.alpha_composite(shadow)

    draw = ImageDraw.Draw(image, "RGBA")
    light = vec(-0.42, 0.72, 0.78)
    light /= np.linalg.norm(light)
    visible: list[tuple[float, Face, np.ndarray]] = []
    for face in FACES:
        normal = matrix @ face.normal
        if normal[2] > 0.035:
            visible.append((float((matrix @ face.normal)[2]), face, normal))
    visible.sort(key=lambda item: item[0])

    base = (248, 239, 214)
    for _, face, normal in visible:
        corners = [project(point, matrix, center, scale) for point in face_corners(face)]
        brightness = 0.70 + max(0.0, float(np.dot(normal, light))) * 0.34
        outer = tone((180, 164, 146), 0.78 + brightness * 0.23)
        inner = tone(base, brightness)
        outline = (61, 67, 65, 245)

        draw.polygon(corners, fill=outer)
        face_center = np.mean(np.array(corners), axis=0)
        inset = [tuple(face_center + (np.array(p) - face_center) * 0.875) for p in corners]
        draw.polygon(inset, fill=inner)

        # Soft highlight on the upper-left part of every lit face.
        highlight = [
            tuple(face_center + (np.array(p) - face_center) * 0.79)
            for p in corners
        ]
        if float(np.dot(normal, light)) > 0.12:
            draw.line([highlight[0], highlight[1], highlight[2]],
                      fill=(255, 255, 250, 110), width=2 * SUPERSAMPLE,
                      joint="curve")

        pip_color = (37, 96, 89, 255)
        if lucky_face == face.value and face.value == 6:
            pip_color = (207, 84, 98, 255)
        for u, v in PIPS[face.value]:
            poly = [project(point, matrix, center, scale)
                    for point in pip_polygon(face, u, v)]
            shadow_poly = [(x + 0.8 * SUPERSAMPLE, y + 1.1 * SUPERSAMPLE)
                           for x, y in poly]
            draw.polygon(shadow_poly, fill=(18, 48, 46, 90))
            draw.polygon(poly, fill=pip_color)
            px, py = poly[3]
            r = 0.72 * SUPERSAMPLE
            draw.ellipse((px-r, py-r, px+r, py+r), fill=(255, 255, 255, 125))

        draw.line(corners + [corners[0]], fill=outline,
                  width=2 * SUPERSAMPLE, joint="curve")
        for x, y in corners:
            r = 1.25 * SUPERSAMPLE
            draw.ellipse((x-r, y-r, x+r, y+r), fill=outline)

    # A small specular glint reinforces the glossy toy-like material.
    glint = Image.new("RGBA", image.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glint)
    gx, gy = 34 * SUPERSAMPLE, (28 - lift * 0.25) * SUPERSAMPLE
    gd.ellipse((gx-4*SUPERSAMPLE, gy-2*SUPERSAMPLE,
                gx+4*SUPERSAMPLE, gy+2*SUPERSAMPLE), fill=(255, 255, 255, 58))
    glint = glint.filter(ImageFilter.GaussianBlur(1.6 * SUPERSAMPLE))
    image.alpha_composite(glint)
    return image.resize((SPRITE_SIZE, SPRITE_SIZE), Image.Resampling.LANCZOS)


def rgba_to_lvgl_argb8888(image: Image.Image) -> bytes:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    # lv_color32_t is BGRA in little-endian memory for ARGB8888.
    return rgba[:, :, [2, 1, 0, 3]].tobytes()


def rgb_to_rgb565_le(image: Image.Image) -> bytes:
    rgb = np.asarray(image.convert("RGB"), dtype=np.uint16)
    packed = ((rgb[:, :, 0] >> 3) << 11) | ((rgb[:, :, 1] >> 2) << 5) | (rgb[:, :, 2] >> 3)
    return packed.astype("<u2").tobytes()


def make_stage() -> Image.Image:
    if STAGE_SOURCE.exists():
        source = Image.open(STAGE_SOURCE).convert("RGB")
        stage = ImageOps.fit(source, STAGE_SIZE, method=Image.Resampling.LANCZOS,
                             centering=(0.5, 0.48))
        stage = ImageEnhance.Color(stage).enhance(0.92)
        stage = ImageEnhance.Contrast(stage).enhance(0.94)
    else:
        stage = Image.new("RGB", STAGE_SIZE, (13, 84, 80))

    # Calm the center for sprites and add a soft illustrated vignette.
    overlay = Image.new("RGBA", STAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay, "RGBA")
    draw.rounded_rectangle((14, 12, 466, 307), radius=28,
                           fill=(8, 62, 61, 32), outline=(250, 211, 120, 45), width=2)
    draw.ellipse((72, 26, 408, 280), fill=(122, 222, 184, 14))
    draw.ellipse((134, 54, 346, 248), fill=(255, 225, 141, 11))
    # Small corner decorations keep the cute comic language without competing
    # with dice, controls, or status text.
    for x, y, color in ((24, 24, (255, 214, 110, 135)),
                        (444, 38, (255, 240, 183, 115)),
                        (28, 286, (129, 224, 193, 100)),
                        (447, 284, (255, 170, 175, 100))):
        draw.line((x-5, y, x+5, y), fill=color, width=2)
        draw.line((x, y-5, x, y+5), fill=color, width=2)
    overlay = overlay.filter(ImageFilter.GaussianBlur(0.4))
    return Image.alpha_composite(stage.convert("RGBA"), overlay).convert("RGB")


def roll_matrices() -> list[tuple[np.ndarray, float]]:
    frames: list[tuple[np.ndarray, float]] = []
    base = rotation_z(math.radians(-9)) @ rotation_x(math.radians(18))
    for frame in range(ROLL_FRAMES):
        t = frame / ROLL_FRAMES
        matrix = (rotation_z(math.tau * t * 0.92) @
                  rotation_y(math.tau * t * 1.27 + 0.55) @
                  rotation_x(math.tau * t * 1.63 + 0.35) @ base)
        lift = 4.0 + 9.0 * (0.5 + 0.5 * math.sin(math.tau * t - math.pi / 2))
        frames.append((matrix, lift))
    return frames


def landing_matrix(value: int) -> np.ndarray:
    face = next(face for face in FACES if face.value == value)
    target = vec(0.0, 0.55, 0.835)
    target /= np.linalg.norm(target)
    aligned = align_vectors(face.normal, target)
    twist = rotation_axis(target, math.radians(-20 + value * 6.5))
    return twist @ aligned


def make_preview(roll: list[Image.Image], landing: list[Image.Image]) -> Image.Image:
    cell = 104
    canvas = Image.new("RGB", (cell * 6, cell * 3 + 50), (12, 78, 75))
    draw = ImageDraw.Draw(canvas)
    draw.text((14, 10), "3D DICE / ROLL FRAMES", fill=(255, 239, 204))
    for index, frame in enumerate(roll):
        x = (index % 6) * cell + 4
        y = (index // 6) * cell + 34
        canvas.paste(frame, (x, y), frame)
    base_y = cell * 2 + 42
    draw.text((14, base_y), "LANDING 1-6", fill=(255, 214, 120))
    for index, frame in enumerate(landing):
        x = index * cell + 4
        y = base_y + 18
        canvas.paste(frame, (x, y), frame)
    return canvas


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    stage = make_stage()
    (OUT / "stage.rgb565").write_bytes(rgb_to_rgb565_le(stage))
    stage.save(OUT / "stage-preview.jpg", quality=92, optimize=True)

    roll = [render_die(matrix, lift) for matrix, lift in roll_matrices()]
    landing = [render_die(landing_matrix(value), 0.0, value) for value in range(1, 7)]
    (OUT / "roll.argb8888").write_bytes(b"".join(rgba_to_lvgl_argb8888(frame) for frame in roll))
    (OUT / "land.argb8888").write_bytes(b"".join(rgba_to_lvgl_argb8888(frame) for frame in landing))
    make_preview(roll, landing).save(OUT / "dice-sprites-preview.png", optimize=True)

    expected = {
        "stage.rgb565": STAGE_SIZE[0] * STAGE_SIZE[1] * 2,
        "roll.argb8888": SPRITE_SIZE * SPRITE_SIZE * 4 * ROLL_FRAMES,
        "land.argb8888": SPRITE_SIZE * SPRITE_SIZE * 4 * 6,
    }
    for name, size in expected.items():
        actual = (OUT / name).stat().st_size
        if actual != size:
            raise RuntimeError(f"{name}: expected {size}, got {actual}")
        print(f"{name}: {actual} bytes")
    print(f"preview: {OUT / 'dice-sprites-preview.png'}")


if __name__ == "__main__":
    main()
