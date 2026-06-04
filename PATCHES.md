# PATCHES

本文件记录本仓库相对于官方 `xiaozhi-esp32` 的主要改动，方便后续合并官方上游版本时快速判断哪些改动需要保留。

## 板级新增

- 新增 `main/boards/qdtech-s3-touch-lcd-3.5/`。
- 新增 QDTech ESP32-S3 3.5 寸触摸屏板型配置。
- `main/CMakeLists.txt` 和 `main/Kconfig.projbuild` 增加 `CONFIG_BOARD_TYPE_QDTECH_S3_TOUCH_LCD_3_5` 选择路径。
- `config.json` / `sdkconfig.defaults` 增加 QDTech 构建入口。

## 横屏显示适配

- 使用 ST77922 初始化序列。
- 物理 320x480 面板映射为 LVGL 480x320 横屏逻辑分辨率。
- 自定义 LVGL flush 回调做横屏到物理面板的旋转写入。
- 保留性能日志，用于后续减少旋转拷贝和大面积刷新。

## 触摸适配

- 使用 CST9217/TDDI 触摸寄存器读取。
- 在板级文件内完成横屏坐标转换。
- 当前仍采用轻量手写 tap/swipe 分发。
- 已为未来迁移到 LVGL `lv_indev_t` 留出读取/转换边界。

## 桌面 UI

- 新增 `desktop_ui.cc` / `desktop_ui.h`。
- 增加主屏、应用页、照片页、日历页、电台页、小智页、设置页。
- 小智状态和表情页与 `Application` 当前状态联动。

## 照片播放服务

- 新增 `photo_service.cc` / `photo_service.h`。
- 应用页中 Photos 入口替代原 Weather 应用卡片入口。
- 按产品规格书接入 MicroSD SDMMC 4-bit 引脚：CLK GPIO5, CMD GPIO4, D0 GPIO6, D1 GPIO7, D2 GPIO2, D3 GPIO3。
- 读取 `/sdcard/photos` 下的 JPEG 文件，解码为 RGB565 LVGL 图像并循环播放。
- 照片之间使用淡入淡出过渡，离开 Photos 页面后暂停后台解码。

## 时间天气服务

- 新增 `time_weather_service.cc` / `time_weather_service.h`。
- 支持 SNTP 时间同步。
- 默认天气位置为 Zhongshan。
- 支持通过 MCP 工具 `self.weather.set_location` 写入 NVS 配置。
- 天气失败时保留上一次成功数据，并记录上次更新时间。

## 网络电台服务

- 新增 `radio_service.cc` / `radio_service.h`。
- 支持内置 MP3 电台列表、HTTP 拉流、MP3 解码和 AudioCodec 输出。
- 支持 Play/Stop/Next/Previous MCP 工具和桌面触摸控制。
- 多 URL fallback 会记忆最近成功 URL，下次优先尝试。
- 流断开、HTTP 失败、MP3 解码失败有明确日志和自动重连。

## 与小智状态联动

- `Application` 增加轻量设备状态回调。
- 电台注册状态回调，实现 Audio Focus：
  - 小智 connecting/listening/speaking/audio_testing 优先。
  - 电台在小智占用音频时暂停并释放外部音频标记。
  - 小智回到 idle 后，电台按用户播放意图自动恢复。

## 上游合并建议

- 尽量把 QDTech 专属逻辑保留在 `main/boards/qdtech-s3-touch-lcd-3.5/`。
- 避免把桌面 UI、电台、天气等板级实验直接扩散到官方公共核心。
- 公共核心改动应保持很薄，并写清楚用途；当前 `Application` 状态回调用于外部服务避让音频。
- 合并官方上游时优先检查：
  - `main/application.h`
  - `main/application.cc`
  - `main/CMakeLists.txt`
  - `main/Kconfig.projbuild`
  - `main/boards/common/wifi_board.*`
  - `main/idf_component.yml`
  - `sdkconfig.defaults`
