#!/usr/bin/env python3
"""Static boundary/resource checks for the QDTech Calendar Zodiac feature."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DETAILS = ROOT / "sdcard/calendar/zodiac/details.tsv"
IMAGES = ROOT / "sdcard/calendar/zodiac/images"
SERVICE = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/zodiac_service.cc"
UI = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc"

SIGNS = (
    "aries", "taurus", "gemini", "cancer", "leo", "virgo",
    "libra", "scorpio", "sagittarius", "capricorn", "aquarius", "pisces",
)


def sign_for(month: int, day: int) -> str:
    ends = (19, 18, 20, 19, 20, 21, 22, 22, 22, 23, 22, 21)
    before = (
        "capricorn", "aquarius", "pisces", "aries", "taurus", "gemini",
        "cancer", "leo", "virgo", "libra", "scorpio", "sagittarius",
    )
    after = (
        "aquarius", "pisces", "aries", "taurus", "gemini", "cancer",
        "leo", "virgo", "libra", "scorpio", "sagittarius", "capricorn",
    )
    return before[month - 1] if day <= ends[month - 1] else after[month - 1]


def jpeg_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    assert data[:2] == b"\xff\xd8", f"not a JPEG: {path}"
    offset = 2
    while offset + 9 < len(data):
        if data[offset] != 0xFF:
            offset += 1
            continue
        marker = data[offset + 1]
        offset += 2
        if marker in (0xD8, 0xD9):
            continue
        length = int.from_bytes(data[offset:offset + 2], "big")
        if marker in range(0xC0, 0xC4):
            height = int.from_bytes(data[offset + 3:offset + 5], "big")
            width = int.from_bytes(data[offset + 5:offset + 7], "big")
            return width, height
        offset += length
    raise AssertionError(f"JPEG dimensions not found: {path}")


def check_boundaries() -> None:
    expected = (
        (1, 19, "capricorn", "aquarius"),
        (2, 18, "aquarius", "pisces"),
        (3, 20, "pisces", "aries"),
        (4, 19, "aries", "taurus"),
        (5, 20, "taurus", "gemini"),
        (6, 21, "gemini", "cancer"),
        (7, 22, "cancer", "leo"),
        (8, 22, "leo", "virgo"),
        (9, 22, "virgo", "libra"),
        (10, 23, "libra", "scorpio"),
        (11, 22, "scorpio", "sagittarius"),
        (12, 21, "sagittarius", "capricorn"),
    )
    for month, last_day, before, after in expected:
        assert sign_for(month, last_day) == before
        assert sign_for(month, last_day + 1) == after

    source = SERVICE.read_text(encoding="utf-8")
    for month, last_day, _, _ in expected:
        branch = f"case {month}: return" if month < 12 else "default: return"
        assert f"{branch} day <= {last_day}" in source


def check_details() -> None:
    records: dict[tuple[str, int], tuple[str, str]] = {}
    for number, raw in enumerate(DETAILS.read_text(encoding="utf-8").splitlines(), 1):
        fields = raw.split("\t")
        assert len(fields) == 4, f"line {number}: expected four TSV fields"
        sign, page_text, title, text = fields
        page = int(page_text)
        assert sign in SIGNS, f"line {number}: unknown sign {sign}"
        assert 0 <= page < 6, f"line {number}: invalid page {page}"
        assert (sign, page) not in records, f"line {number}: duplicate record"
        assert 0 < len(title.encode("utf-8")) < 48
        assert 0 < len(text.encode("utf-8")) < 640
        assert len(raw.encode("utf-8")) < 1024
        records[(sign, page)] = (title, text)
    assert set(records) == {(sign, page) for sign in SIGNS for page in range(6)}


def check_dice() -> None:
    source = UI.read_text(encoding="utf-8")
    assert "{true, true, false, true, true, true, true}" in source
    assert "{true, true, true, true, true, true, true}" not in source


def check_async_sd_loading() -> None:
    source = UI.read_text(encoding="utf-8")
    show_start = source.index("void DesktopUI::ShowZodiacReader()")
    show_end = source.index("void DesktopUI::HideZodiacReader()", show_start)
    show_body = source[show_start:show_end]
    assert "QdZodiac::LoadImage" not in show_body
    assert "RefreshZodiacReader(true)" in show_body
    refresh_start = source.index("void DesktopUI::RefreshZodiacReader(bool load_image)")
    refresh_end = source.index("bool DesktopUI::HandleZodiacTap", refresh_start)
    refresh_body = source[refresh_start:refresh_end]
    assert "background->Schedule" in refresh_body
    assert "lvgl_port_lock" in refresh_body
    assert "zodiac_load_request_id_" in refresh_body


def check_images(required: bool) -> None:
    files = sorted(IMAGES.glob("*.jpg"))
    if not files and not required:
        return
    assert {path.stem for path in files} == set(SIGNS)
    for path in files:
        assert path.stat().st_size <= 256 * 1024
        assert jpeg_dimensions(path) == (160, 160), path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-images", action="store_true")
    args = parser.parse_args()
    check_boundaries()
    check_details()
    check_dice()
    check_async_sd_loading()
    check_images(args.require_images)
    print("QDTech zodiac checks passed")


if __name__ == "__main__":
    main()
