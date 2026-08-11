from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
KCONFIG = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
HEADER = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h").read_text(encoding="utf-8")
UI = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc").read_text(encoding="utf-8")
LOGIC = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/number_slide_logic.cc").read_text(encoding="utf-8")


def test_atomic_feature_is_default_off():
    block = KCONFIG.split(
        "config QDTECH_EXPERIMENT_SHAKE_REVOLVER_NUMBER_SLIDE", 1
    )[1].split("\nconfig ", 1)[0]
    assert "depends on QDTECH_EXPERIMENT_PUZZLE_ARCADE" in block
    assert "default n" in block


def test_disabled_path_keeps_original_lucky_revolver_entry():
    assert "#else\n    LUCKY_REVOLVER," in (
        ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/puzzle_arcade_service.h"
    ).read_text(encoding="utf-8")
    assert '"体感迷宫", "2048", "空当接龙", "幸运左轮"' in UI


def test_enabled_path_moves_entry_and_has_dedicated_mode():
    assert "NUMBER_SLIDE" in HEADER
    assert "ShakeLabMode::LUCKY_REVOLVER" in UI
    assert "shake_lab_revolver_board_" in HEADER
    assert "ShakeLabRevolverDrawCb" in UI
    assert '"体感迷宫", "2048", "空当接龙", "数字华容道"' in UI


def test_number_slide_is_fixed_memory_and_solvable_by_construction():
    assert "uint8_t puzzle_number_slide_cells_" in HEADER
    assert "ResetSolved(cells);" in LOGIC
    assert "std::swap(cells[blank], cells[tile]);" in LOGIC
    assert "malloc" not in LOGIC
    assert "new " not in LOGIC


def test_feature_adds_no_task_timer_or_nvs():
    feature_sources = LOGIC + UI[UI.find("ResetPuzzleNumberSlide"):]
    assert "xTaskCreate" not in LOGIC
    assert "lv_timer_create" not in LOGIC
    assert "nvs_open" not in LOGIC
    assert "puzzle_number_slide" in feature_sources
