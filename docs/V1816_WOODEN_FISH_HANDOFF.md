# QDTech v1.8.16 功德木鱼交接

更新日期：2026-08-04

## 发布范围

- 基线：GitHub Release `v1.8.15`。
- 新功能：在“摇一摇实验室”增加“功德木鱼”入口。
- 交互：触摸木鱼或轻敲设备时，播放敲击动画、显示“功德 +1”，并播放短促木鱼音效。
- 视觉：从 SD 卡直读 RGB565 背景，路径为 `/sdcard/wooden_fish/background.rgb565`。
- 稳定性：功能由 QDTech 专用编译开关控制；不改变原有网络、桌面、OTA 和音频主路径。

## SD 卡资源

- 文件：`wooden_fish/background.rgb565`
- 格式：RGB565，240 × 160，无文件头
- 大小：76,800 字节
- SHA-256：`69ef564a504f4d46fa92055ec5364ff26e4268b547b8236e9cfcd5d748fe165a`
- 预览：`wooden_fish/background-preview.jpg`

缺少或无法读取 SD 图片时，界面会保留内置绘制兜底，不应崩溃。

## 敲击识别

- 复用现有 BMI270 采样任务，不新增任务。
- 仅进入“功德木鱼”页面后启用 10 ms 快速采样。
- 当前标定：冲击阈值 18、偏差阈值 12、陀螺仪上限 25、静止阈值 8。
- 有 200 ms 限流，避免一次敲击被重复计数。

## 音频

- 使用现有通知音频队列与解码路径，不新增音频任务或大缓冲。
- 仅设备空闲且没有外部音频占用时播放。
- P3 音效为 3 帧、约 0.18 秒；最终增益 +6 dB，保留峰值余量，无削波。
- 声音来源：George Papargyris，Freesound “Mokugyo drum sounds”，CC BY 4.0。
- 来源：https://freesound.org/people/George_Papargyris/sounds/837177/
- 许可证与处理说明：`main/assets/common/wooden_fish.LICENSE.txt`。

## 编译开关

正式 QDTech 构建通过 `sdkconfig.wooden-fish.defaults` 开启：

- `CONFIG_QDTECH_EXPERIMENT_WOODEN_FISH=y`
- `CONFIG_QDTECH_EXPERIMENT_WOODEN_FISH_AUDIO=y`

源码默认关闭，其他板型不会自动启用。

## 验证与回退

- 自动测试：`tests/qdtech_wooden_fish/`。
- 烧录边界：仅刷 App 分区，不写 NVS、分区表或 bootloader。
- 回退：仅刷回 v1.8.15 App；或移除 `sdkconfig.wooden-fish.defaults` 中两个开关重新构建。
- 最终构建大小、哈希、烧录和实体板验证结果以本次 v1.8.16 Release Notes 与 `SHA256SUMS.txt` 为准。
