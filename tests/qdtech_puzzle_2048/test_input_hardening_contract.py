from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
KCONFIG = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
HEADER = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h").read_text(encoding="utf-8")
UI = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc").read_text(encoding="utf-8")
RENDERER = (ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/puzzle_2048_canvas_renderer.cc").read_text(encoding="utf-8")


def test_feature_is_default_off_and_layers_on_cached_renderer():
    block = KCONFIG.split(
        "config QDTECH_EXPERIMENT_2048_INPUT_HARDENING", 1
    )[1].split("\nconfig ", 1)[0]
    assert "depends on QDTECH_EXPERIMENT_2048_STABLE_RENDERER" in block
    assert "default n" in block


def test_restart_button_has_matching_visual_and_touch_dead_zone():
    assert 'DrawButton(&layer, 322, 180, 152, 36, "重新开始"' in RENDERER
    assert 'button(322, 180, 152, 36, "重新开始"' in UI
    assert "board_hit(322, 180, 152, 36)" in UI
    # Direction controls end at y=140, leaving 40 px before restart.
    assert "board_hit(374, 104, 48, 36)" in UI


def test_rapid_input_is_single_slot_and_page_lifetime_bounded():
    assert "puzzle_2048_pending_dx_" in HEADER
    assert "puzzle_2048_pending_dy_" in HEADER
    assert "QueuePuzzle2048Input(dx, dy);" in UI
    assert "lv_timer_create(Puzzle2048InputTimerCb, 25, this)" in UI
    assert "lv_timer_resume(puzzle_2048_input_timer_)" in UI
    assert UI.count("lv_timer_pause(timer);") >= 2
    assert UI.count("StopPuzzle2048InputTimer();") >= 4


def test_render_cooldown_starts_at_completion_and_each_move_finishes_once():
    assert "last_render_finished_ms_" in RENDERER
    assert "kInputIntervalMs = 130" in RENDERER
    delta = RENDERER.split("void Puzzle2048CanvasRenderer::RenderDelta", 1)[1]
    assert delta.count("lv_canvas_init_layer") == 1
    assert delta.count("lv_canvas_finish_layer") == 1
    assert "RenderTiles(&layer, cells, changed_mask);" in delta
    assert "RenderInfo(&layer, score" in delta
    assert "last_render_finished_ms_ = esp_timer_get_time() / 1000" in RENDERER


def test_feature_does_not_add_rtos_task_queue_or_nvs():
    feature = UI[UI.find("void DesktopUI::QueuePuzzle2048Input"):UI.find(
        "bool DesktopUI::HandlePuzzleArcadeTap"
    )]
    assert "xTaskCreate" not in feature
    assert "xQueueCreate" not in feature
    assert "nvs_open" not in feature
