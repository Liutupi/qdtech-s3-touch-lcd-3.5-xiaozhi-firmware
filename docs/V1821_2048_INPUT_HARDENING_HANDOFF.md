# v1.8.21 2048 防误触与快速输入稳态交接

## 范围

- 基线：v1.8.21。
- 默认关闭开关：`CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING`。
- 依赖已验证的 `CONFIG_QDTECH_EXPERIMENT_2048_STABLE_RENDERER`。
- 不新增 RTOS 任务、FreeRTOS 队列、SD 资源或 NVS 字段。

## 实现

- “重新开始”从 `y=150, h=32` 下移到 `y=180, h=36`，与方向键之间保留 40 px 空白区。
- 缓存画布、回退绘制与触摸命中区使用相同坐标。
- 130 ms 输入冷却从渲染完成时开始计算。
- 快速点击只保存最后一个待处理方向；使用页面生命周期内的 LVGL 25 ms 定时器重试，离开页面、回主页或重新开始时立即释放。
- 方块以每层最多 4 个的小批次绘制，最坏情况下从 16 次方块画布完成操作降到 4 次，同时限制 LVGL 绘制任务峰值。
- 2048 页面不再为每次快速点击同步输出 UART Tap 日志；保留缓存渲染器的低频移动、拒绝、耗时和堆内存诊断。

## 验证（2026-08-11）

- 契约检查：16 项通过。
- ESP-IDF 5.5.2 正式构建通过。
- App：`/Users/tupi/qdtech-v1821-2048-build-on/xiaozhi.bin`
- App 大小：7,060,992 字节；相对上一硬件验证版增加 336 字节。
- SHA256：`12a01c93217196765c45f0fbbf9f85d0941c5cbbf5f698c6876f889c9d4fc972`。
- 构建开关确认：Puzzle Arcade、2048 cached renderer、2048 input hardening、摇一摇幸运左轮/数字华容道、MD dual mode 均开启。
- 仅 App 写入 `0x100000`，写后 digest 校验一致；未写 NVS、分区表、bootloader、OTA data、模型或 `mdemu`。
- 冷启动确认：v1.8.21、8MB PSRAM、显示/LVGL、触摸、BMI270、SD、音频、Wi-Fi、手机配置页和 MD 安装检测均正常；IP `192.168.1.106`，无 panic、watchdog 或重启。
- 待用户完成：2048 连续快速点击数十步，以及重新开始按钮防误触的手感验证。

## 回退

关闭 `CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING` 并重新构建 App，即回到上一版 2048 缓存渲染、坐标与输入逻辑；无需改动或擦除 NVS。
