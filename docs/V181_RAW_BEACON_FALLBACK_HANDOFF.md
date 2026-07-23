# v1.8.1 Raw Beacon Fallback Handoff

Date: 2026-07-19

## Scope and safety boundary

- This is a QDTech-only, default-off diagnostic behind
  `CONFIG_QDTECH_EXPERIMENT_RAW_BEACON_FALLBACK`.
- It adds one `esp_timer` and no FreeRTOS task. Every 100 ms it submits an
  83-byte open-network beacon for the already configured provisioning identity:
  SSID `Xiaozhi-8E81`, BSSID `7c:e8:b1:b2:8e:81`, channel 1, and 100 TU beacon
  interval.
- Only the App partition at `0x100000` was flashed. NVS and every other
  partition were left untouched.
- No temporary hotspot SSID or password is present in either raw-beacon App.

## Build evidence

The AP-interface diagnostic App is:

- `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-raw-beacon-fallback-on.bin`
- size: 6,749,360 bytes (`0x66fcb0`)
- SHA256: `3186bd7d30fe2c0fb4c206b18eb77c4f8eab191ac8bf7efb5076b89f50125f2e`

The STA-interface queue-isolation App is currently flashed:

- `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-raw-beacon-sta-interface-on.bin`
- size: 6,749,392 bytes (`0x66fcd0`)
- SHA256: `7564425b47b1afd14479f11acb6cadfba0ad23e54bc5bfe34f9c8f0d57decdef`

The selected single-driver APSTA baseline App is 6,744,416 bytes
(`0x66e960`), so the current diagnostic adds 4,976 bytes. Static DIRAM is
172,055 of 341,760 bytes (50.34%), 56 bytes above the single-driver baseline.
Runtime cost is one timer, the 83-byte frame vector, and small atomic counters;
there is no task stack.

## Hardware results

### AP-interface submission

- App-only flash and esptool hash verification passed.
- The normal APSTA provisioning services started at about 8.5 seconds.
- At 1,100 attempts the API reported 1,100 successful submissions and the
  TX-done callback reported 1,099 successes, with no API or callback failure.
- Repeated exact-name Mac scans returned `COUNT=0`; a full scan returned zero
  target SSID/BSSID matches.

### STA-interface submission

- The identical frame was submitted through `WIFI_IF_STA` while APSTA remained
  on channel 1. App-only flash and esptool hash verification passed.
- At 2,300 attempts the API reported 2,300 successful submissions and the
  TX-done callback reported 2,299 successes, with no failure.
- Internal free memory stayed at 178,223 bytes, the largest internal block at
  102,400 bytes, and PSRAM free at 8,373,956 bytes. There was no reboot, panic,
  or progressive leak.
- The exact-name scan still returned `COUNT=0`; the full scan found six nearby
  networks and zero target SSID/BSSID matches.

## Conclusion

The raw transmit API and TX-done callback accepting a frame do not prove that
the frame was observable over the air. Both AP and STA submission queues gave
the same negative external result. Together with the earlier successful
Station connection test, this rules out heap starvation, general antenna/PA
failure, and Wi-Fi driver reinitialization as the primary cause.

A new, separately approved diagnostic should now A/B the provisioning channel.
The board and Mac repeatedly see the strongest nearby network on channel 6,
while every failed SoftAP test has been fixed to channel 1. Testing the normal
SoftAP plus the same raw beacon on channel 6 is the smallest remaining
firmware-side discriminator before disrupting the Mac Wi-Fi interface for a
monitor-mode packet capture.

## Rollback

Flash only the credential-free RX self-test App back to `0x100000`:

- `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-wifi-rx-self-test-on.bin`
- size: 6,742,880 bytes (`0x66e360`)
- SHA256: `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`

Do not erase flash and do not write NVS for rollback.
