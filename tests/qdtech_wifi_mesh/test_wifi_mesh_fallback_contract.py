from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
COMPAT = (
    ROOT
    / "managed_components/78__esp-wifi-connect/include/qdtech_provisioning_compat.h"
).read_text(encoding="utf-8")
CONFIG_HEADER = (
    ROOT
    / "managed_components/78__esp-wifi-connect/include/wifi_configuration_ap.h"
).read_text(encoding="utf-8")
CONFIG_SOURCE = (
    ROOT / "managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc"
).read_text(encoding="utf-8")
STATION_SOURCE = (
    ROOT / "managed_components/78__esp-wifi-connect/wifi_station.cc"
).read_text(encoding="utf-8")


class WifiMeshFallbackContractTest(unittest.TestCase):
    def test_mesh_fallback_is_qdtech_production_only(self):
        block = COMPAT.split("#define QDTECH_WIFI_MESH_FALLBACK 1", 1)[0]
        self.assertIn("#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT)", block)
        self.assertIn("QDTECH_WIFI_MESH_FALLBACK", CONFIG_SOURCE)
        self.assertIn("QDTECH_WIFI_MESH_FALLBACK", STATION_SOURCE)


    def test_scan_api_exposes_each_radio_identity_without_breaking_ssid_clients(self):
        self.assertIn('"{\\\"ssid\\\":\\\"%s\\\"', CONFIG_SOURCE)
        self.assertIn('"\\\"channel\\\":%u', CONFIG_SOURCE)
        self.assertIn('\\\"bssid\\\":\\\"%02x:%02x:%02x:%02x:%02x:%02x\\\"', CONFIG_SOURCE)


    def test_softap_and_compatibility_beacon_share_the_live_radio_channel(self):
        start_ap = CONFIG_SOURCE.split(
            "void WifiConfigurationAp::StartAccessPoint()", 1
        )[1].split("void WifiConfigurationAp::StartRawBeaconFallback", 1)[0]
        self.assertIn("esp_wifi_set_channel(", start_ap)
        self.assertIn("wifi_config.ap.channel, WIFI_SECOND_CHAN_NONE", start_ap)
        self.assertLess(
            start_ap.index("esp_wifi_set_channel("),
            start_ap.index("StartRawBeaconFallback(ssid, wifi_config.ap.channel)"),
        )


    def test_provisioning_orders_and_pins_all_matching_bssids(self):
        self.assertIn("return lhs.rssi > rhs.rssi", CONFIG_SOURCE)
        self.assertIn("candidates.push_back(record)", CONFIG_SOURCE)
        self.assertIn("wifi_config.sta.channel = candidate->primary", CONFIG_SOURCE)
        self.assertIn("wifi_config.sta.bssid_set = true", CONFIG_SOURCE)
        self.assertIn("attempt_count = candidates.empty() ? 1 : candidates.size()", CONFIG_SOURCE)
        self.assertIn("Provisioning %s candidate failed", CONFIG_SOURCE)


    def test_sta_authentication_pauses_the_visibility_fallback(self):
        connect = CONFIG_SOURCE.split(
            "bool WifiConfigurationAp::ConnectToWifi", 1
        )[1].split("void WifiConfigurationAp::Save", 1)[0]
        self.assertIn("StopRawBeaconFallback()", connect)
        self.assertIn("StartRawBeaconFallback(GetSsid(), active_channel)", connect)
        self.assertLess(
            connect.index("StopRawBeaconFallback()"),
            connect.index("esp_wifi_connect()"),
        )


    def test_auth_expire_enables_scoped_legacy_retry(self):
        connect = CONFIG_SOURCE.split(
            "bool WifiConfigurationAp::ConnectToWifi", 1
        )[1].split("void WifiConfigurationAp::Save", 1)[0]
        self.assertIn("reason != WIFI_REASON_AUTH_EXPIRE", connect)
        self.assertIn('try_candidates("normal", false)', connect)
        self.assertIn('try_candidates("legacy-b/g", true)', connect)
        self.assertIn("kLegacyStaProtocol", connect)
        self.assertLess(
            connect.index('try_candidates("normal", false)'),
            connect.index('try_candidates("legacy-b/g", true)'),
        )


    def test_legacy_profile_is_persisted_per_ssid_and_reused(self):
        self.assertIn('nvs_set_str(nvs, "legacy_ssid", ssid.c_str())', CONFIG_SOURCE)
        self.assertIn('nvs_get_str(nvs, "legacy_ssid"', STATION_SOURCE)
        self.assertIn("ap_record.ssid == legacy_wifi_ssid_", STATION_SOURCE)
        self.assertIn("legacy-b/g", STATION_SOURCE)


    def test_failed_auth_resumes_beacon_on_active_channel(self):
        connect = CONFIG_SOURCE.split(
            "bool WifiConfigurationAp::ConnectToWifi", 1
        )[1].split("void WifiConfigurationAp::Save", 1)[0]
        self.assertIn("esp_wifi_get_channel(&active_channel", connect)
        self.assertIn("StartRawBeaconFallback(GetSsid(), active_channel)", connect)
        self.assertNotIn("esp_wifi_set_channel(\n        kProvisioningChannel", connect)


    def test_provisioning_success_requires_dhcp_not_only_association(self):
        self.assertIn("#define WIFI_ASSOCIATED_BIT BIT2", CONFIG_SOURCE)
        connected_handler = CONFIG_SOURCE.split(
            "event_id == WIFI_EVENT_STA_CONNECTED", 1
        )[1].split("event_id == WIFI_EVENT_STA_DISCONNECTED", 1)[0]
        self.assertIn("WIFI_ASSOCIATED_BIT", connected_handler)
        self.assertNotIn("WIFI_CONNECTED_BIT", connected_handler)
        got_ip_handler = CONFIG_SOURCE.split("event_id == IP_EVENT_STA_GOT_IP", 1)[1]
        self.assertIn("WIFI_CONNECTED_BIT", got_ip_handler)
        self.assertIn("Connected to WiFi %s with DHCP", CONFIG_SOURCE)


    def test_saved_network_path_rotates_rejected_mesh_candidate_immediately(self):
        for reason in (2, 4, 15, 200, 201, 202, 203, 204, 205):
            self.assertIn(f"case {reason}:", STATION_SOURCE)
        self.assertIn("ShouldRotateMeshCandidate(reason)", STATION_SOURCE)
        rotate = STATION_SOURCE.split("ShouldRotateMeshCandidate(reason)", 1)[1]
        self.assertLess(rotate.index("StartConnect()"), rotate.index("MAX_RECONNECT_COUNT"))
        self.assertIn("wifi_config.sta.bssid_set = true", STATION_SOURCE)


    def test_alternate_identity_is_scoped_persistent_and_restart_safe(self):
        mesh_blocks = CONFIG_SOURCE[CONFIG_SOURCE.index("std::vector<wifi_ap_record_t> candidates") :]
        station_policy = STATION_SOURCE[STATION_SOURCE.index("ShouldRotateMeshCandidate") :]
        connect = mesh_blocks[: mesh_blocks.index("void WifiConfigurationAp::Save")]
        self.assertIn('kAlternateStaMacKey = "alt_sta_mac"', CONFIG_SOURCE)
        self.assertIn("auth_expire_only && !IsAlternateStaMacEnabled()", connect)
        self.assertIn("EnableAlternateStaMac()", connect)
        self.assertIn('xTaskCreate([](void*)', connect)
        self.assertIn("esp_restart()", connect)
        self.assertIn("alternate_sta_mac_enabled_", STATION_SOURCE)
        self.assertIn("esp_wifi_set_mac(WIFI_IF_STA", STATION_SOURCE)
        for forbidden in ("xQueueCreate", "SsidManager::GetInstance().AddSsid"):
            self.assertNotIn(forbidden, connect)
            self.assertNotIn(forbidden, station_policy[: station_policy.index("WifiStation& WifiStation::GetInstance")])
        self.assertIn("std::atomic<int> last_disconnect_reason_", CONFIG_HEADER)


if __name__ == "__main__":
    unittest.main()
