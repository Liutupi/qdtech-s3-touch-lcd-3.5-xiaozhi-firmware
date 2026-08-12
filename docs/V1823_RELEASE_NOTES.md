# v1.8.23 发布说明

## 本次更新

- 提升部分路由器和 Mesh 网络下首次配网、断线重连的成功率。
- 同名 Wi-Fi 会优先连接信号更好的节点，并在失败后切换其他节点。
- 对特定 `AUTH_EXPIRE` 认证兼容问题增加严格限定的两级回退，不影响正常网络。
- 保留 `v1.8.22` 的界面、主题、小智、天气、日历、摇一摇、益智游戏馆、MD 双模式和 SD 卡内容。

## 已验证

- 普通首次配网与兼容身份启动。
- OTA 检查、MQTT 连接、天气 HTTP 200、时间同步。
- NVS 备份、清空测试和恢复。
- 11 项 Wi-Fi 自动化契约测试。

## 固件选择

- `qdtech-s3-touch-lcd-3.5-v1.8.23-app.bin`：正常 OTA 或 App-only 更新，保留配网、激活和 SD 卡数据。
- `qdtech-s3-touch-lcd-3.5-v1.8.23-full.bin`：完整双模式恢复镜像，仅在新板、彻底恢复或明确需要重建分区时使用。
- `qdtech-s3-touch-lcd-3.5-v1.8.23-firmware.zip`：包含烧录所需文件、校验值和说明。

## 构建与校验

- App：7,064,592 字节，SHA256 `d559cbfba87f259851dc0a11abb91aba92ee9e9bc1d539c659ca493eca378ecb`
- Full：16,728,480 字节，SHA256 `b5805e03d705efab43c63aa6c6feb2432a93c32292007aacf3b916508ea97fd8`
- Firmware ZIP：5,734,389 字节，SHA256 `bbfd011127a6e93c44bd1eb593d900e8aff0e2e05e17d43a7e627128121d8716`
- 7 MB OTA 槽剩余 275,440 字节；诊断强制 MAC 选项在正式构建中为关闭。
- 完整镜像的两个 OTA 槽均与 App 附件逐字节一致，MD 模拟器分区沿用已验证镜像。

## 发布状态

- 公开 Release：<https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/tag/v1.8.23>
- 状态：非草稿、非预发布，已设为 Latest。
- GitHub Latest API 已返回 `v1.8.23`；App、Full、Firmware ZIP 和 `SHA256SUMS.txt` 均上传完成。
