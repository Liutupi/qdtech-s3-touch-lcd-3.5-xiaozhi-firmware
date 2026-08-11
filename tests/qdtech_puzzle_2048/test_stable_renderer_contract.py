from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
KCONFIG = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
HEADER = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/puzzle_2048_canvas_renderer.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/puzzle_2048_canvas_renderer.cc").read_text(encoding="utf-8")
UI = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc").read_text(encoding="utf-8")


def test_feature_is_default_off_and_board_local():
    block = KCONFIG.split("config QDTECH_EXPERIMENT_2048_STABLE_RENDERER", 1)[1]
    block = block.split("\nconfig ", 1)[0]
    assert "depends on QDTECH_EXPERIMENT_PUZZLE_ARCADE" in block
    assert "default n" in block


def test_canvas_is_psram_backed_and_bounded():
    assert "kWidth = 480" in HEADER
    assert "kHeight = 266" in HEADER
    assert "MALLOC_CAP_SPIRAM" in SOURCE
    assert "heap_caps_aligned_alloc" in SOURCE
    assert "kBufferBytes" in SOURCE


def test_renderer_has_backpressure_metrics_and_release():
    assert "kInputIntervalMs" in SOURCE
    assert "AcceptInput" in SOURCE
    assert "heap_caps_get_largest_free_block" in SOURCE
    assert "heap_caps_get_minimum_free_size" in SOURCE
    assert "heap_caps_free(buffer_)" in SOURCE
    assert "ReleasePuzzle2048Renderer();" in UI


def test_allocation_failure_retains_v1821_path():
    assert "using v1.8.21 fallback" in UI
    assert "PuzzleArcadeDrawCb" in UI
    assert "puzzle_2048_renderer_->Active()" in UI


def test_up_button_is_below_information_redraw_region():
    assert 'DrawRect(layer, 308, 7, 156, 48' in SOURCE
    assert 'DrawButton(&layer, 374, 62, 48, 36, "上"' in SOURCE
    assert 'board_hit(374, 62, 48, 36)' in UI
    assert 'board_hit(322, 104, 48, 36)' in UI
    assert 'board_hit(374, 104, 48, 36)' in UI
    assert 'board_hit(426, 104, 48, 36)' in UI


def test_high_score_uses_isolated_deferred_nvs_record():
    assert 'nvs_open("puzzle2048", NVS_READONLY' in UI
    assert 'nvs_open("puzzle2048", NVS_READWRITE' in UI
    assert 'nvs_get_u32(handle, "high_score"' in UI
    assert 'nvs_set_u32(handle, "high_score"' in UI
    assert 'puzzle_2048_high_score_dirty_' in UI
    assert 'SavePuzzle2048HighScore();' in UI
    assert '"\u7eaa\u5f55 %lu \u00b7 %lu"' in SOURCE
