# QDTech ESP32-S3 3.5 寸触摸横屏小智固件

本目录是 QDTech ESP32-S3 3.5 寸触摸屏的小智板级适配，目标是在官方 xiaozhi-esp32 基础上提供稳定的横屏桌面系统底座。

## 适配硬件说明

- MCU: ESP32-S3，Flash/PSRAM 容量以实际模组为准，当前工程按 16 MB 分区构建。
- LCD: 3.5 寸 320x480 物理竖屏面板，横屏逻辑分辨率 480x320。
- LCD 控制器: ST77922，QSPI/SPI 接口。
- Touch: CST9217/TDDI 触摸控制器，I2C 地址 `0x55`。
- MicroSD: SDMMC 4-bit，总线引脚来自产品规格书。
- Audio Codec: ES8311。
- 功放/PA: GPIO1，当前配置为反相使能 `AUDIO_CODEC_PA_INVERTED=true`。

源码确认的主要引脚：

- Audio I2S: MCLK GPIO17, BCLK GPIO18, WS GPIO21, DOUT GPIO15, DIN GPIO16。
- Audio/Touch I2C: SDA GPIO38, SCL GPIO39。
- LCD QSPI: CS GPIO10, CLK GPIO12, D0/MOSI GPIO11, D1/MISO GPIO13, D2 GPIO14, D3 GPIO9。
- MicroSD SDMMC: CLK GPIO5, CMD GPIO4, D0 GPIO6, D1 GPIO7, D2 GPIO2, D3 GPIO3。
- Backlight: GPIO41。
- Touch: INT GPIO47, RST GPIO48。
- Boot: GPIO0。

未从源码确认的信息请继续以原理图为准：屏幕背光电路细节、电池/充电检测、RGB LED、其它扩展外设。

## 屏幕方向和分辨率说明

- 物理面板为 320x480。
- LVGL 逻辑分辨率为 480x320 横屏。
- 当前 flush 回调会把 LVGL 横屏缓冲区旋转写入物理面板。
- 已加入慢刷新性能日志，便于后续评估是否需要局部刷新或更低成本的旋转路径。

## 触摸说明

- 当前触摸读取在 `QdtechTouch` 中手动轮询 CST9217 寄存器。
- 触摸坐标在板级文件中转换为 480x320 横屏坐标。
- 当前 UI 仍使用轻量手写 tap/swipe 分发，没有强行迁移到 `lv_indev_t`。
- 后续迁移建议：复用 `QdtechTouch::ReadFirstPoint()` 的原始读取和横屏转换，封装为 LVGL indev read callback，再逐步删除手写命中区域。

## 音频说明

- 小智语音核心使用官方 `Application` 音频输入/输出路径。
- 网络电台直接解码 MP3 并写入同一个 `AudioCodec` 输出。
- 已增加轻量 Audio Focus 机制：小智 connecting/listening/speaking/audio_testing 优先，电台自动暂停；回到 idle 后按用户播放意图恢复。

## 已实现功能

- 小智语音核心。
- 480x320 横屏显示。
- 触摸 tap/swipe 操作。
- 桌面 UI、应用页、小智表情页、设置页。
- 照片页：应用中心 Photos 入口读取 `/sdcard/photos` 下的 JPEG 照片并循环淡入淡出播放。
- 时间同步和天气显示。
- 网络电台 MP3 播放。
- 电台与小智语音之间的基础音频避让和自动恢复。
- MCP 工具：天气位置设置、电台控制、WiFi 列表/切换/删除/默认网络设置。

## 编译命令

在 ESP-IDF 5.5 或兼容环境中：

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" set-target esp32s3
idf.py build
```

Windows PowerShell 示例：

```powershell
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" set-target esp32s3
idf.py build
```

## 烧录命令

把串口替换为实际端口：

```bash
idf.py -p COM13 -b 921600 flash monitor
```

macOS/Linux 示例：

```bash
idf.py -p /dev/cu.usbmodem212401 -b 921600 flash monitor
```

## 常见问题

- 编译找不到板型：确认 `sdkconfig.defaults` 中启用了 `CONFIG_BOARD_TYPE_QDTECH_S3_TOUCH_LCD_3_5=y`。
- 屏幕方向不对：本板级目录已经按 480x320 横屏适配，请确认使用的是 QDTech 板型而不是 Waveshare 3.5 板型。
- 触摸无反应：查看日志中的 `Touch max points`、`touch down/tap/swipe`；若完全无事件，优先检查 I2C、RST/INT 引脚和触摸芯片型号。
- 电台无声：先确认小智不在 listening/speaking 状态，再看 `RadioService` 的 HTTP、MP3 sync、decode 和 audio focus 日志。
- 天气失败：断网或接口失败不会清空上次成功天气，日志会显示重试和缓存使用情况。

## 如何修改天气城市

推荐通过 MCP 工具设置：

- 工具名：`self.weather.set_location`
- 参数：`city`、`latitude`、`longitude`

默认值在 `time_weather_service.cc` 的 `LoadWeatherLocation()` 中，目前为 Zhongshan / 22.5176 / 113.3928。代码已经使用 NVS `weather_cfg` 保存，后续可接入设置页。

## 如何修改电台列表

当前电台列表在 `radio_service.cc` 的 `kStations[]` 中。每个电台包含：

- `name`: 电台名。
- `urls`: 最多 3 个 MP3 URL fallback。
- `codec`: 当前为 MP3。
- `bitrate_kbps`: UI 显示用码率。

后续建议迁移为 SPIFFS/SD 卡 `radio.json`，并保留内置列表作为 fallback。

## 如何使用照片播放

- 准备 FAT 格式 MicroSD 卡。
- 在 SD 卡根目录创建 `photos` 目录。
- 放入 `.jpg` 或 `.jpeg` 照片，建议先使用 480x320、800x480 或 960x640 以内的图片验证。
- 烧录启动后，从主屏左滑进入 Apps，点击 Photos。
- 页面会挂载 SD 卡、扫描 `/sdcard/photos`，并循环播放照片。
- 照片之间使用淡入淡出过渡；离开 Photos 页面后暂停解码，避免影响小智语音和电台。

## 已知问题

- 触摸尚未迁移到 LVGL 标准 `lv_indev_t`。
- LCD flush 仍存在旋转拷贝成本，已加性能日志但未重写显示驱动。
- 电台目前只支持直接 MP3 流，不支持 HLS/AAC。
- 照片播放目前只支持 JPEG，不支持 PNG/GIF；长文件名支持取决于 FATFS LFN 配置。
- 天气城市设置已经可写入 NVS，但设置页还没有可视化编辑控件。
- WiFi 设置页主要用于展示，实际切换/删除仍依赖 MCP 工具。

## 后续优化计划

- 将触摸迁移到 LVGL indev，减少手写命中区域。
- 优化显示刷新路径，减少大面积旋转拷贝。
- 支持 SPIFFS/SD 卡读取 `radio.json`。
- 设置页支持城市、音量、亮度、电台开机行为等 NVS 配置。
- 增加番茄钟模块。
- 尽量把 QDTech 专属功能留在本板级目录，降低合并官方上游时的冲突。
