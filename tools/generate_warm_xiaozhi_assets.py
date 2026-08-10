#!/usr/bin/env python3
"""Build the warm XiaoZhi theme GIFs and their LVGL C wrappers.

The source portraits are intentionally kept outside the firmware source tree.
Each portrait is normalized to the 300x238 display slot, then animated only in
small local regions so the GIFs remain light enough for the embedded build.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter


WIDTH = 300
HEIGHT = 238
ASPECT = WIDTH / HEIGHT
GIF_COLORS = 32

STATE_ORDER = (
    "standby",
    "listening",
    "speaking",
    "thinking",
    "happy",
    "surprised",
    "sad",
    "angry",
    "sleepy",
)

STATE_DURATIONS = {
    "standby": [520, 120, 120, 900],
    "listening": [120, 120, 120, 120],
    "speaking": [90, 90, 90, 90],
    "thinking": [300, 150, 300],
    "happy": [220, 120, 220],
    "surprised": [250, 100, 250],
    "sad": [450, 120, 450],
    "angry": [220, 110, 220],
    "sleepy": [500, 180, 500, 220],
}

PULSE_BOXES = {
    "standby": [(82, 88, 126, 137), (174, 88, 218, 137)],
    "listening": [(47, 101, 76, 164), (224, 101, 253, 164)],
    "thinking": [(198, 30, 237, 77)],
    "happy": [(46, 28, 87, 84), (214, 113, 257, 176)],
    "surprised": [(49, 33, 251, 91)],
    "sad": [(82, 88, 126, 139), (174, 88, 218, 139)],
    "angry": [(48, 29, 252, 91)],
    "sleepy": [(80, 90, 221, 145), (135, 125, 208, 181)],
}


def normalize_portrait(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGB")
    source_w, source_h = image.size

    # The generated portraits share a roughly 1024-pixel character scale even
    # when the canvas aspect ratio differs. Cropping a centered 1024px-tall
    # window prevents the mascot from jumping in size between expressions.
    crop_h = min(source_h, 1024)
    crop_w = round(crop_h * ASPECT)
    if crop_w > source_w:
        crop_w = source_w
        crop_h = round(crop_w / ASPECT)

    left = (source_w - crop_w) // 2
    top = (source_h - crop_h) // 2
    crop = image.crop((left, top, left + crop_w, top + crop_h))
    return crop.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)


def brighten_regions(image: Image.Image, boxes: list[tuple[int, int, int, int]], factor: float) -> Image.Image:
    frame = image.copy()
    for box in boxes:
        region = frame.crop(box)
        region = ImageEnhance.Brightness(region).enhance(factor)
        frame.paste(region, box[:2])
    return frame


def add_soft_glint(image: Image.Image, center: tuple[int, int], radius: int, opacity: int) -> Image.Image:
    frame = image.convert("RGBA")
    glow = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    x, y = center
    draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=(255, 180, 48, opacity))
    glow = glow.filter(ImageFilter.GaussianBlur(max(1, radius // 2)))
    return Image.alpha_composite(frame, glow).convert("RGB")


def speaking_frame(base: Image.Image, mouth_scale: float) -> Image.Image:
    frame = base.copy()
    box = (126, 126, 177, 174)
    mouth = frame.crop(box)
    new_h = max(24, round(mouth.height * mouth_scale))
    mouth = mouth.resize((mouth.width, new_h), Image.Resampling.BICUBIC)
    # Fill from the same face neighborhood before centering the changed mouth.
    fill = base.crop(box).filter(ImageFilter.GaussianBlur(2))
    frame.paste(fill, box[:2])
    frame.paste(mouth, (box[0], box[1] + (box[3] - box[1] - new_h) // 2))
    return frame


def make_frames(state: str, base: Image.Image) -> list[Image.Image]:
    if state == "speaking":
        return [
            speaking_frame(base, 1.00),
            speaking_frame(base, 0.76),
            speaking_frame(base, 0.92),
            speaking_frame(base, 0.66),
        ]

    boxes = PULSE_BOXES[state]
    factors = {
        "standby": [1.00, 1.045, 1.075, 1.00],
        "listening": [0.96, 1.08, 1.18, 1.07],
        "thinking": [0.98, 1.20, 1.00],
        "happy": [1.00, 1.18, 1.02],
        "surprised": [1.00, 1.16, 1.00],
        "sad": [1.00, 0.94, 1.00],
        "angry": [1.00, 1.16, 1.00],
        "sleepy": [1.00, 0.94, 1.00, 0.97],
    }[state]
    frames = [brighten_regions(base, boxes, factor) for factor in factors]

    # Small local glints make idle/listening motion legible at 300x238 without
    # moving the whole portrait or forcing expensive full-frame GIF updates.
    if state == "standby":
        frames[1] = add_soft_glint(frames[1], (112, 106), 2, 105)
        frames[2] = add_soft_glint(frames[2], (188, 106), 2, 130)
    elif state == "listening":
        frames[2] = add_soft_glint(frames[2], (58, 132), 4, 145)
        frames[2] = add_soft_glint(frames[2], (242, 132), 4, 145)
    return frames


def save_gif(frames: list[Image.Image], durations: list[int], destination: Path) -> None:
    # One shared embedded palette keeps flash use modest and avoids colour
    # flicker between frames. The changed regions remain visually smooth at the
    # panel's native resolution.
    palette = frames[0].quantize(colors=GIF_COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    indexed = [palette]
    indexed.extend(
        frame.quantize(palette=palette, dither=Image.Dither.NONE)
        for frame in frames[1:]
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    indexed[0].save(
        destination,
        save_all=True,
        append_images=indexed[1:],
        duration=durations,
        loop=0,
        optimize=True,
        disposal=1,
    )


def write_lvgl_c(gif_path: Path, destination: Path, symbol: str) -> None:
    data = gif_path.read_bytes()
    macro = f"LV_ATTRIBUTE_IMG_{symbol.upper()}"
    byte_lines = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        byte_lines.append("    " + ", ".join(f"0x{value:02x}" for value in chunk) + ",")

    source = f"""#ifdef __has_include
    #if __has_include(\"lvgl.h\")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include \"lvgl.h\"
#else
    #include \"lvgl/lvgl.h\"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef {macro}
#define {macro}
#endif

const LV_ATTRIBUTE_MEM_ALIGN {macro} uint8_t {symbol}_map[] = {{
{chr(10).join(byte_lines)}
}};

const lv_image_dsc_t {symbol} = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RAW,
        .flags = 0,
        .w = {WIDTH},
        .h = {HEIGHT},
        .reserved_2 = 0,
        .stride = 0,
    }},
    .data_size = {len(data)},
    .data = {symbol}_map,
    .reserved = NULL,
}};
"""
    destination.write_text(source, encoding="utf-8", newline="\n")


def save_contact_sheet(board_dir: Path, destination: Path) -> None:
    sheet = Image.new("RGB", (WIDTH * 3, HEIGHT * 3), (250, 244, 232))
    for index, state in enumerate(STATE_ORDER):
        x = (index % 3) * WIDTH
        y = (index // 3) * HEIGHT
        with Image.open(board_dir / f"qd_tupi_bot_{state}.gif") as gif:
            sheet.paste(gif.convert("RGB"), (x, y))
    destination.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(destination, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--board-dir", type=Path, required=True)
    parser.add_argument("--preview-dir", type=Path, required=True)
    args = parser.parse_args()

    portraits: dict[str, Image.Image] = {}
    for state in STATE_ORDER:
        source = args.sources / f"{state}.png"
        if not source.exists():
            raise FileNotFoundError(source)
        portrait = normalize_portrait(source)
        portraits[state] = portrait
        gif_path = args.board_dir / f"qd_tupi_bot_{state}.gif"
        frames = make_frames(state, portrait)
        save_gif(frames, STATE_DURATIONS[state], gif_path)
        write_lvgl_c(gif_path, args.board_dir / f"qd_tupi_bot_{state}.c", f"qd_tupi_bot_{state}")
        with Image.open(gif_path) as gif:
            gif.convert("RGB").save(
                args.preview_dir / f"warm-xiaozhi-{state}-300x238.png",
                optimize=True,
            )

    save_contact_sheet(args.board_dir, args.preview_dir / "warm-xiaozhi-v2-contact-sheet.png")

    for state in STATE_ORDER:
        gif_path = args.board_dir / f"qd_tupi_bot_{state}.gif"
        print(f"{state:10s} {gif_path.stat().st_size:7d} bytes")


if __name__ == "__main__":
    main()
