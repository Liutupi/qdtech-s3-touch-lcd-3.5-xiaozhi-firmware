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
- 日历页可选启用 SD 卡星座阅读器：输入公历年月日后推算十二星座，并按需加载六页文本与可爱 JPEG 图片。
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

## 首次配网回退

- **v1.8.2 生产发布：** QDTech 首次配网采用已验证的“信道 6 + 保留一个
  APSTA Wi-Fi 驱动 + Station 发送队列标准 Beacon”兼容路径；原生 AP 仍
  负责关联、DHCP、DNS 与配网页。该路径只在 QDTech 首次配网期间启用，
  RX/TX 自检、临时凭据、原始发送统计和周期诊断日志均关闭。
- v1.8.2 恢复完整应用先启动、再连已保存 Wi-Fi 的正常顺序；若随后需配网，
  原入口直接把运行中的 Station 驱动交给上述 APSTA 路径。已连接 Wi-Fi
  后，`http://<board-ip>/` 的手机配置页自动启动，可修改左上角 Logo、
  名称和天气城市。
- Apps 中心全部入口和常见状态改为中文，不改变布局、卡片位置或交互；
  IP、日期、进度和歌曲名仍显示真实内容。

- 公共 `WifiBoard` 只增加一个可覆盖的启动连接超时入口，默认仍为
  30 秒，其他板型行为不变。
- QDTech 可通过默认关闭的
  `CONFIG_QDTECH_EXPERIMENT_FAST_PROVISIONING_FALLBACK` 将无可用已保存
  网络时的等待缩短为 8 秒。
- 继续使用先停止 Station、再启动纯 SoftAP 的稳定路径，不引入 AP+STA
  并行，也不清除 NVS。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_NETWORK_FIRST_BOOT` 在构造屏幕、音频、
  触摸、传感器和应用服务之前完成已保存网络预检；连接失败时让极简
  SoftAP 独占启动资源，配网成功重启后才进入完整应用。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_STATELESS_SOFTAP_RF` 让 SoftAP Wi-Fi
  驱动不读取或写入驱动 NVS，并显式固定为可见 SSID、信道 1、HT20、
  11b/g/n、CN 1-13、100 ms 信标；同时回读并记录实际射频配置。实机
  回读正确但手机和 Mac 仍扫描不到信标，因此该开关继续默认关闭，
  不能视为已修复。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_WIFI_RX_SELF_TEST` 只汇总既有
  Station 扫描的热点数、最强 RSSI 和信道，不新增扫描，也不记录
  SSID/BSSID。实机连续接收到 11-15 个周边热点，但 Mac 仍完全
  看不到本机 SoftAP，故障边界已收敛到发射/信标路径。该开关
  默认关闭，仅用于诊断。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_WIFI_STA_TX_SELF_TEST` 把一组
  临时热点凭据只加入 RAM 扫描候选，不调用 `SsidManager`、不写 NVS。
  实机完成 WPA2 认证、关联、DHCP、HTTP 和 MQTT，证明 Station 模式
  的接收、发射、功放和天线都正常。剩余故障只在 SoftAP 信标或
  Station 停止/反初始化后再进入 SoftAP 的路径；该开关仍默认关闭。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_SINGLE_DRIVER_APSTA_PROVISIONING`
  在早期 Station 扫描失败后保留同一个 Wi-Fi 驱动和 Station netif，
  注销 Station 事件后直接切换到 APSTA 并启动原有配网服务。实机确认
  只有一次驱动初始化、APSTA/DHCP/DNS/Web 均正常且内存稳定，但连续
  Mac 扫描仍看不到信标，因而已排除 Station stop/deinit/reinit 转换
  这一原因；该开关默认关闭，不能视为生产修复。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_RAW_BEACON_FALLBACK` 使用一个
  `esp_timer` 每 100 ms 通过 `esp_wifi_80211_tx` 补发标准开放式信标，
  不新增任务。AP 与 STA 两个发送接口均连续返回 API/TX-done 成功，
  但外部扫描仍看不到目标 SSID/BSSID，说明发送回调不能证明空口可见；
  该开关继续默认关闭，仅用于诊断。
- 可选的 `CONFIG_QDTECH_EXPERIMENT_SOFTAP_CHANNEL_6_AB` 只把 QDTech
  配网热点和原始信标从信道 1 同步切换到信道 6。实机连续四次扫描均
  发现热点，客户端成功加入并由 DHCP 获得 `192.168.4.2`，配网页返回
  HTTP 200；这是首次得到可见且可用的配网热点。该 A/B 开关仍默认
  关闭，正式采用前还需单独验证关闭原始信标后的普通 SoftAP。

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
