import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PRODUCTION_CSV = ROOT / "partitions/v1/16m_qdtech_7m_ota.csv"
DUAL_MODE_CSV = ROOT / "partitions/v1/16m_qdtech_7m_ota_md.csv"
KCONFIG = ROOT / "main/Kconfig.projbuild"
BOARD_DEFAULTS = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults"
EXPERIMENT_DEFAULTS = ROOT / "sdkconfig.md-dual-mode.defaults"
SERVICE = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/md_boot_service.cc"
SERVICE_HEADER = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/md_boot_service.h"
CATALOG_SOURCE = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/md_catalog_service.cc"
CATALOG_HEADER = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/md_catalog_service.h"
OTA_SOURCE = ROOT / "main/ota.cc"
DESKTOP_UI = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc"
DESKTOP_UI_HEADER = ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h"
WIFI_BOARD_SOURCE = ROOT / "main/boards/common/wifi_board.cc"
WIFI_BOARD_HEADER = ROOT / "main/boards/common/wifi_board.h"
BOARD_SOURCE = (
    ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc"
)
FIRMWARE_UPDATE_SERVICE = (
    ROOT / "main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.cc"
)


def parse_size(value: str) -> int:
    value = value.strip().upper()
    if value.endswith("M"):
        return int(value[:-1], 0) * 1024 * 1024
    if value.endswith("K"):
        return int(value[:-1], 0) * 1024
    return int(value, 0)


def parse_partitions(path: Path):
    rows = []
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        columns = [item.strip() for item in line.split(",")]
        rows.append(
            {
                "name": columns[0],
                "type": columns[1],
                "subtype": columns[2],
                "offset": parse_size(columns[3]),
                "size": parse_size(columns[4]),
            }
        )
    return rows


class MdDualModeContractTest(unittest.TestCase):
    def test_production_layout_is_preserved_and_tail_is_added(self):
        production = parse_partitions(PRODUCTION_CSV)
        dual_mode = parse_partitions(DUAL_MODE_CSV)

        self.assertEqual(dual_mode[: len(production)], production)
        self.assertEqual(len(dual_mode), len(production) + 1)
        self.assertEqual(
            dual_mode[-1],
            {
                "name": "mdemu",
                "type": "app",
                "subtype": "ota_2",
                "offset": 0xF00000,
                "size": 0x100000,
            },
        )
        self.assertEqual(dual_mode[-1]["offset"] + dual_mode[-1]["size"], 0x1000000)

    def test_experiment_is_default_off_and_uses_opt_in_layout(self):
        kconfig = KCONFIG.read_text(encoding="utf-8")
        block = re.search(
            r"config QDTECH_EXPERIMENT_MD_DUAL_MODE\n(?P<body>.*?)(?=\nconfig |\Z)",
            kconfig,
            re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertIn("default n", block.group("body"))
        self.assertNotIn("QDTECH_EXPERIMENT_MD_DUAL_MODE", BOARD_DEFAULTS.read_text(encoding="utf-8"))

        defaults = EXPERIMENT_DEFAULTS.read_text(encoding="utf-8")
        self.assertIn("CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE=y", defaults)
        self.assertIn('16m_qdtech_7m_ota_md.csv"', defaults)

    def test_service_is_guarded_and_has_no_background_runtime(self):
        header = SERVICE_HEADER.read_text(encoding="utf-8")
        source = SERVICE.read_text(encoding="utf-8")
        guard = "CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE"
        self.assertIn(guard, header)
        self.assertIn(guard, source)
        self.assertLess(header.index('#include "sdkconfig.h"'), header.index(guard))
        for forbidden in (
            "xTaskCreate",
            "xTaskCreatePinnedToCore",
            "xTimerCreate",
            "xQueueCreate",
            "std::thread",
        ):
            self.assertNotIn(forbidden, source)

    def test_switch_requires_valid_main_slot_emulator_and_durable_handoff(self):
        source = SERVICE.read_text(encoding="utf-8")
        for required in (
            'strcmp(running->label, "ota_0")',
            'strcmp(running->label, "ota_1")',
            "ESP_PARTITION_SUBTYPE_APP_OTA_2",
            'kEmulatorLabel[] = "mdemu"',
            "kEmulatorAddress = 0xF00000",
            "kEmulatorSize = 0x100000",
            "esp_ota_get_partition_description",
            'cJSON_AddStringToObject(root, "return_partition"',
            "fflush(file)",
            "fsync(fileno(file))",
            "rename(kHandoffNewPath, kHandoffPath)",
            "esp_ota_set_boot_partition(emulator)",
        ):
            self.assertIn(required, source)

        self.assertLess(
            source.index("WriteHandoff(relative_rom"),
            source.index("esp_ota_set_boot_partition(emulator)"),
        )

    def test_rom_path_is_confined_and_normal_ota_path_is_unchanged(self):
        source = SERVICE.read_text(encoding="utf-8")
        for required in (
            'kRomPrefix[] = "roms/md/"',
            "kMaxRelativeRomPath = 235",
            'component == ".."',
            "path.find('\\\\')",
            "S_ISREG(info.st_mode)",
            'extension == ".md"',
            'extension == ".gen"',
            'extension == ".bin"',
            'extension == ".smd"',
            'extension == ".zip"',
        ):
            self.assertIn(required, source)

        ota_source = OTA_SOURCE.read_text(encoding="utf-8")
        self.assertIn("const esp_partition_t* GetNextMainOtaPartition()", ota_source)
        self.assertIn('target_label = "ota_1"', ota_source)
        self.assertIn('target_label = "ota_0"', ota_source)
        self.assertIn("GetNextMainOtaPartition();", ota_source)
        self.assertNotIn("mdemu", ota_source)

        for consumer in (DESKTOP_UI, FIRMWARE_UPDATE_SERVICE):
            text = consumer.read_text(encoding="utf-8")
            self.assertIn("CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE", text)
            self.assertIn("GetNextMainOtaPartition()", text)
            self.assertIn("esp_ota_get_next_update_partition", text)

    def test_catalog_is_bounded_psram_only_and_sd_local(self):
        header = CATALOG_HEADER.read_text(encoding="utf-8")
        source = CATALOG_SOURCE.read_text(encoding="utf-8")
        for text in (header, source):
            self.assertIn("CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE", text)
        self.assertLess(header.index('#include "sdkconfig.h"'), header.index("CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE"))
        for required in (
            "kMaxEntries = 128",
            'kCatalogRoot[] = "/sdcard/roms/md"',
            'kCatalogTsv[] = "/sdcard/roms/md/catalog.tsv"',
            "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT",
            'FindTsvColumn(fields, field_count, "filename")',
            'FindTsvColumn(fields, field_count, "title")',
            'FindTsvColumn(fields, field_count, "category")',
            "compact headerless rows: path, title, category",
            'strcasecmp(dot, ".md")',
            'strcasecmp(dot, ".gen")',
            'strcasecmp(dot, ".bin")',
            'strcasecmp(dot, ".smd")',
            'strcasecmp(dot, ".zip")',
            "heap_caps_free(entries_)",
        ):
            self.assertIn(required, header + source)
        for forbidden in (
            "xTaskCreate",
            "xTimerCreate",
            "xQueueCreate",
            "esp_http",
            "WiFi",
        ):
            self.assertNotIn(forbidden, source)

    def test_md_library_is_lazy_and_does_not_switch_or_restart(self):
        source = DESKTOP_UI.read_text(encoding="utf-8")
        header = DESKTOP_UI_HEADER.read_text(encoding="utf-8")
        for required in (
            "DesktopPage::MD_LIBRARY",
            "CreateMdLibraryPage(lv_scr_act())",
            "ReleaseMdLibraryPage()",
            "md_catalog_.Load()",
            "md_catalog_.Clear()",
            "/sdcard/roms/md",
            "md_launch_callback_(entry->relative_path, md_resume_mode_, md_save_slot_)",
        ):
            self.assertIn(required, source)
        self.assertIn("MD_LIBRARY", header)
        self.assertIn("SetMdLaunchCallback", header)

        request_block = re.search(
            r"void DesktopUI::RequestMdLaunch\(\) \{(?P<body>.*?)\n\}",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(request_block)
        for forbidden in (
            "esp_restart",
            "esp_ota_set_boot_partition",
            "PrepareAndSelect",
        ):
            self.assertNotIn(forbidden, request_block.group("body"))

        refresh_block = re.search(
            r"void DesktopUI::RefreshMdCatalog\(\) \{(?P<body>.*?)\n\}",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(refresh_block)
        refresh_body = refresh_block.group("body")
        self.assertNotIn("selected ? COLOR_CREAM : COLOR_SURFACE", refresh_body)
        self.assertIn("selected_bg", refresh_body)
        self.assertIn("selected ? lv_color_hex(0xfffbf4)", refresh_body)
        self.assertIn("selected ? COLOR_GOLD : COLOR_MUTED", refresh_body)

    def test_puzzle_arcade_selection_keeps_title_and_tag_readable(self):
        source = DESKTOP_UI.read_text(encoding="utf-8")
        header = DESKTOP_UI_HEADER.read_text(encoding="utf-8")
        for required in (
            "puzzle_arcade_game_tags_[8]",
            "puzzle_arcade_game_tags_[i] = tag",
            "selected ? 0x55364f : 0xfffcfa",
            "selected ? 0xfffbf4 : 0x453a48",
            "selected ? 0xffd98a : 0x7b6675",
        ):
            self.assertIn(required, header + source)

    def test_puzzle_arcade_more_page_entry_stays_readable_in_touch_states(self):
        source = DESKTOP_UI.read_text(encoding="utf-8")
        entry = re.search(
            r"const lv_color_t puzzle_entry_bg = lv_color_hex\(0x55364f\);"
            r"(?P<body>.*?)lv_obj_align\(puzzle_title,",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(entry)
        body = entry.group("body")
        for required in (
            "puzzle_entry_bg, LV_STATE_PRESSED",
            "puzzle_entry_bg, LV_STATE_FOCUSED",
            "puzzle_entry_bg, LV_STATE_CHECKED",
            "lv_color_hex(0xfffbf4)",
            "lv_color_hex(0xffd98a)",
        ):
            self.assertIn(required, body)

    def test_legacy_app_ota_hides_md_entry_until_emulator_partition_is_installed(self):
        source = DESKTOP_UI.read_text(encoding="utf-8")
        header = DESKTOP_UI_HEADER.read_text(encoding="utf-8")
        for required in (
            "HasInstalledMdEmulator()",
            "ESP_PARTITION_SUBTYPE_APP_OTA_2",
            'partition->address != 0xF00000',
            'partition->size != 0x100000',
            "esp_ota_get_partition_description",
            "md_emulator_available_ = HasInstalledMdEmulator()",
            "md_emulator_available_ ? 210 : 432",
            "md_emulator_available_ && hit(246, 240, 210, 48)",
            "if (md_emulator_available_)",
            "bool md_emulator_available_ = false",
        ):
            self.assertIn(required, source + header)

    def test_md_return_gets_one_shot_wifi_grace_without_changing_normal_boot(self):
        source = BOARD_SOURCE.read_text(encoding="utf-8")
        for required in (
            '"/sdcard/retro-go/config/md_return.pending"',
            "S_ISREG(marker_info.st_mode)",
            "unlink(kMdReturnMarker)",
            "md_return_grace_active_ = true",
            "if (md_return_grace_active_)",
            "return 25 * 1000",
            "return 8 * 1000",
            "ShouldEnterProvisioningOnStartupTimeout() const override",
            "return false",
            "return WifiBoard::ShouldEnterProvisioningOnStartupTimeout()",
        ):
            self.assertIn(required, source)

        self.assertLess(
            source.index("unlink(kMdReturnMarker)"),
            source.index("md_return_grace_active_ = true"),
        )

        wifi_source = WIFI_BOARD_SOURCE.read_text(encoding="utf-8")
        wifi_header = WIFI_BOARD_HEADER.read_text(encoding="utf-8")
        self.assertIn("ShouldEnterProvisioningOnStartupTimeout() const", wifi_header)
        self.assertIn("return true", wifi_source)
        self.assertIn("if (!ShouldEnterProvisioningOnStartupTimeout())", wifi_source)
        self.assertIn("waiting with Station active without provisioning", wifi_source)
        self.assertIn("while (!wifi_station.WaitForConnected(30 * 1000))", wifi_source)
        self.assertIn("provisioning stays disabled", wifi_source)
        self.assertIn("Saved WiFi connected during extended Station wait", wifi_source)

    def test_board_wires_nonblocking_safe_switch_without_network_drain(self):
        source = BOARD_SOURCE.read_text(encoding="utf-8")
        for required in (
            '#include "md_boot_service.h"',
            "InitializeMdDualMode();",
            "SetMdLaunchCallback",
            "md_launch_in_progress_.compare_exchange_strong",
            "xTaskCreateWithCaps",
            '"md_launch", 6144',
            "MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT",
            "Provisioning runs before Xiaozhi enters MainEventLoop()",
            "validating/selecting an OTA",
            "MdBootService::GetInstance().PrepareAndSelect(request)",
            "radio_service_.Stop();",
            "podcast_service_.Stop();",
            "fc_emulator_service_.Stop();",
            "Wi-Fi NVS is untouched",
            "esp_restart();",
        ):
            self.assertIn(required, source)

        self.assertNotIn("Application::GetInstance().Schedule(\n                    [this, rom_path", source)

        callback = re.search(
            r"void InitializeMdDualMode\(\) \{(?P<body>.*?)\n    \}\n#endif",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(callback)
        body = callback.group("body")
        self.assertLess(body.index("PrepareAndSelect(request)"), body.index("radio_service_.Stop();"))
        self.assertNotIn("app.PrepareForFirmwareUpgrade();", body)
        self.assertLess(body.index("radio_service_.Stop();"), body.index("esp_restart();"))


if __name__ == "__main__":
    unittest.main()
