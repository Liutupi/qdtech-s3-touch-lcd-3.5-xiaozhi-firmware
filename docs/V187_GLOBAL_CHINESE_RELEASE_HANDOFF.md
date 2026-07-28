# QDTech v1.8.7 全局中文显示层：发布交接

发布日期：2026-07-28
发布基线：v1.8.6（GitHub Latest Release）

## 本次目标

将设备端桌面、应用、设置、状态提示与手机配置页的用户可见文案统一为中文；不改变原有功能状态、任务调度、网络逻辑或 NVS 数据格式，避免以功能稳定性换取文案本地化。

## 代码改动范围

- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
  - 为桌面及各应用的可见文本增加中文显示映射。
  - 检测 UTF-8 中文文本后绑定 `font_puhui_16_4`，英文内部状态值仍保持不变。
  - 覆盖主界面、应用入口、日历、飞控、电台、音乐、播客、专注、网络、设置、诊断和摇一摇实验室等可见页面。
- `main/boards/qdtech-s3-touch-lcd-3.5/qd_wifi_config_server.cc`
  - 手机配置页的可见文案改为中文。
- `CMakeLists.txt`
  - 项目版本从 `1.8.6` 升至 `1.8.7`。

本次没有新增 FreeRTOS 任务、定时器、队列、信号量、网络流程或存储格式。

## 构建与验证

- 构建目标：ESP32-S3，QDTech 3.5 寸触控屏生产配置。
- 完整 ESP-IDF 构建、合并固件均已通过。
- App 固件大小：6,757,808 bytes；7 MiB OTA 槽剩余 582,224 bytes（8%）。
- 中文字形审计：本次界面共使用 715 个独立中文字符，均包含在 `font_puhui_16_4` 中。
- 实机 App-only 验证已完成：启动、8 MB PSRAM、LCD/ST77922、LVGL、触摸、BMI270、SD 电台、Wi-Fi 以及手机配置页均正常初始化；进入应用与电台页期间未见 panic、watchdog 或重启。
- 验证网络中 DNS 查询失败（`getaddrinfo 202`），因此 OTA/天气/电台在线服务未在该局域网完成联机验收；该网络现象与本次本地中文显示改动无关。

最终 v1.8.7 二进制相对实机验证的 v1.8.6 测试载荷仅包含项目版本元数据变化，功能代码保持一致。

## 发布物

目录：`releases/v1.8.7/`

- `qdtech-s3-touch-lcd-3.5-v1.8.7-app.bin`：OTA / 仅 App 分区刷写。
- `qdtech-s3-touch-lcd-3.5-v1.8.7-full.bin`：完整合并镜像，仅供明确需要全量恢复时使用。
- `qdtech-s3-touch-lcd-3.5-v1.8.7-firmware.zip`：上述两个镜像的压缩包。
- `qdtech-s3-touch-lcd-3.5-v1.8.7-sdcard.zip`：沿用 v1.8.6 的 SD 卡资源包，本版本没有 SD 资源变更。
- `SHA256SUMS.txt`：全部发布物的 SHA-256 校验值。

GitHub Release 的 OTA 客户端应下载 `*-app.bin`；不要把完整合并镜像作为普通 OTA 包下发。

## 回滚

若需回退，使用 GitHub `v1.8.6` Release 的 `*-app.bin` 进行 OTA 或 App-only 刷写即可。不要擦除 NVS，除非另有明确的数据恢复需求。

## 后续验收建议

1. 由一台已运行 v1.8.6 的远程设备检查 OTA 是否识别并更新到 v1.8.7。
2. 在设备上逐页查看中文名称、空状态与弹窗；如发现截断，优先调整单页排版，不改动内部状态字符串。
3. 在可正常解析 DNS 的网络中复验天气、电台、播客及 OTA 检查。
