#!/usr/bin/env python3
"""Generate the small warm-theme avatar and daily-card LVGL assets."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter, ImageOps


ASSETS = {
    "qd_tupi_avatar": (40, 40),
    "qd_tupi_daily": (64, 52),
}


def prepare(source: Path, width: int, height: int) -> Image.Image:
    with Image.open(source) as opened:
        image = opened.convert("RGB")
    image = ImageOps.fit(
        image,
        (width, height),
        method=Image.Resampling.LANCZOS,
        centering=(0.5, 0.5),
    )
    image = ImageEnhance.Contrast(image).enhance(1.04)
    image = ImageEnhance.Color(image).enhance(1.03)
    return image.filter(ImageFilter.UnsharpMask(radius=0.7, percent=115, threshold=2))


def rgb565_bytes(image: Image.Image) -> bytes:
    data = bytearray()
    for red, green, blue in image.getdata():
        value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        data.extend((value & 0xFF, value >> 8))
    return bytes(data)


def write_lvgl_c(image: Image.Image, destination: Path, symbol: str) -> None:
    width, height = image.size
    data = rgb565_bytes(image)
    macro = f"LV_ATTRIBUTE_IMG_{symbol.upper()}"
    lines = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in chunk) + ",")

    source = f'''#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef {macro}
#define {macro}
#endif

const LV_ATTRIBUTE_MEM_ALIGN {macro} uint8_t {symbol}_map[] = {{
{chr(10).join(lines)}
}};

const lv_image_dsc_t {symbol} = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .flags = 0,
        .w = {width},
        .h = {height},
        .reserved_2 = 0,
        .stride = {width * 2},
    }},
    .data_size = sizeof({symbol}_map),
    .data = {symbol}_map,
    .reserved = NULL,
}};
'''
    destination.write_text(source, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--avatar-source", type=Path, required=True)
    parser.add_argument("--daily-source", type=Path, required=True)
    parser.add_argument("--board-dir", type=Path, required=True)
    parser.add_argument("--preview-dir", type=Path, required=True)
    args = parser.parse_args()

    args.board_dir.mkdir(parents=True, exist_ok=True)
    args.preview_dir.mkdir(parents=True, exist_ok=True)
    sources = {
        "qd_tupi_avatar": args.avatar_source,
        "qd_tupi_daily": args.daily_source,
    }

    for symbol, (width, height) in ASSETS.items():
        image = prepare(sources[symbol], width, height)
        write_lvgl_c(image, args.board_dir / f"{symbol}.c", symbol)
        image.save(args.preview_dir / f"{symbol}-{width}x{height}.png", optimize=True)
        image.resize((width * 8, height * 8), Image.Resampling.NEAREST).save(
            args.preview_dir / f"{symbol}-pixel-preview.png",
            optimize=True,
        )
        print(f"{symbol}: {width}x{height}, {width * height * 2} bytes")


if __name__ == "__main__":
    main()
