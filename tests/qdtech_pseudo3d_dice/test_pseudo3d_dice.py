#!/usr/bin/env python3
"""Static guards for QDTech full-screen, pre-rendered 3D dice."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc"
UI_H = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h"
DETECTOR = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/shake_detector.cc"
DETECTOR_H = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/shake_detector.h"
BOARD = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc"
ASSET_CC = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/dice_theme_asset.cc"
ASSET_H = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/dice_theme_asset.h"
ASSET_DIR = ROOT / "sdcard/shake_lab/dice"
KCONFIG = ROOT / "main/Kconfig.projbuild"
PROFILE = ROOT / "sdkconfig.pseudo3d-dice.defaults"


def config_block(kconfig: str, name: str) -> str:
    return kconfig.split(f"config {name}", 1)[1].split("config ", 1)[0]


def main() -> None:
    ui = UI.read_text(encoding="utf-8")
    ui_h = UI_H.read_text(encoding="utf-8")
    detector = DETECTOR.read_text(encoding="utf-8")
    detector_h = DETECTOR_H.read_text(encoding="utf-8")
    board = BOARD.read_text(encoding="utf-8")
    asset_cc = ASSET_CC.read_text(encoding="utf-8")
    asset_h = ASSET_H.read_text(encoding="utf-8")
    kconfig = KCONFIG.read_text(encoding="utf-8")
    profile = PROFILE.read_text(encoding="utf-8")

    expected_sizes = {
        "stage.rgb565": 480 * 320 * 2,
        "roll.argb8888": 96 * 96 * 4 * 12,
        "land.argb8888": 96 * 96 * 4 * 6,
    }
    for name, expected in expected_sizes.items():
        assert (ASSET_DIR / name).stat().st_size == expected, name

    roll = (ASSET_DIR / "roll.argb8888").read_bytes()
    landing = (ASSET_DIR / "land.argb8888").read_bytes()
    assert min(roll[3::4]) == 0 and max(roll[3::4]) == 255
    assert min(landing[3::4]) == 0 and max(landing[3::4]) == 255

    assert 'kStagePath[] = "/sdcard/shake_lab/dice/stage.rgb565"' in asset_cc
    assert 'kRollPath[] = "/sdcard/shake_lab/dice/roll.argb8888"' in asset_cc
    assert 'kLandingPath[] = "/sdcard/shake_lab/dice/land.argb8888"' in asset_cc
    assert "LV_COLOR_FORMAT_ARGB8888" in asset_cc
    assert "kRollFrameCount = 12" in asset_h
    assert "kLandingFrameCount = 6" in asset_h

    for name in (
        "QDTECH_EXPERIMENT_PSEUDO3D_DICE",
        "QDTECH_EXPERIMENT_SHAKE_LAB_FULLSCREEN",
    ):
        assert f"config {name}" in kconfig
        assert re.search(r"\bdefault n\b", config_block(kconfig, name))
        assert f"CONFIG_{name}=y" in profile

    assert "shake_lab_dice_sprites_ready_" in ui
    assert "lv_image_set_src(image, &shake_lab_dice_roll_atlas_.frames[frame])" in ui
    assert "shake_lab_dice_landing_atlas_.frames[value - 1]" in ui
    assert "lv_timer_set_period(shake_lab_anim_timer_, 60)" in ui
    assert "lv_timer_set_period(shake_lab_anim_timer_, 80)" in ui
    assert "lv_obj_set_style_bg_opa(shake_lab_mode_group_, LV_OPA_COVER" in ui
    assert "Missing SD" in config_block(kconfig, "QDTECH_EXPERIMENT_PSEUDO3D_DICE")

    # Dice alone gets a bounded roll: other Shake Lab modes retain the proven
    # generic stillness/settling state machine.
    assert "kDiceAutoRevealMs = 420" in detector
    assert "kDiceMinimumShakeDurationMs = 90" in detector
    assert "kDiceMinimumPeakCount = 2" in detector
    assert "kDiceMinimumDirectionReversals = 1" in detector
    assert "dice_auto_reveal_ ? kDiceMinimumPeakCount : kMinimumPeakCount" in detector
    assert "dice_auto_reveal_ &&" in detector
    assert "SetDiceAutoReveal" in detector_h
    assert "mode == ShakeLabMode::DICE" in ui
    assert "SetShakeLabDiceAutoRevealCallback" in ui_h
    assert "shake_lab_dice_auto_reveal_.load" in board
    assert "dice_auto_reveal=%d" in board
    assert "kDiceUiAutoRevealTicks = 7" in ui
    assert "Shake Lab Dice UI auto reveal" in ui
    assert "shake_lab_dice_result_revealed_" in ui_h
    assert "Shake Lab Dice duplicate reveal ignored" in ui

    dice_assignments = re.findall(
        r"shake_lab_dice_values_state_\[[^\]]+\]\s*=\s*"
        r"(?:\n\s*)?static_cast<uint8_t>\(esp_random\(\) % (\d+) \+ 1\)",
        ui,
    )
    assert dice_assignments and set(dice_assignments) == {"6"}
    print("3D dice sprite and full-screen guards passed")


if __name__ == "__main__":
    main()
