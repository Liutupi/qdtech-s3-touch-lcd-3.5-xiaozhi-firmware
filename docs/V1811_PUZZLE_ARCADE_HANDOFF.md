# v1.8.11 益智游戏馆交接

## 版本范围

基线为 v1.8.9。本版本新增并完善游戏馆内容，不修改 Wi-Fi、天气、电台、掌卦、日历、星座、称骨及既有媒体功能。

## 新增功能

- 新增 2048 糖果方块：
  - 4×4 棋盘、随机生成 2/4、单次合并规则、得分、最大方块、胜利与无路可走判断。
  - 使用方向按钮控制，避免与全局触摸手势争用。
  - SD 封面：`games/puzzle_arcade/covers/tile_2048.jpg`。
- 新增空当接龙：
  - 52 张牌随机发至 8 列。
  - 4 个空当、4 个收牌区、红黑交替递减移动、撤销一步和重开。
  - 牌桌使用高对比奶油白牌面、粉紫边框和独立顶部卡槽。
  - SD 封面：`games/puzzle_arcade/covers/freecell.jpg`。
  - SD 牌背：`games/puzzle_arcade/freecell/card_back.jpg`。

## 推箱子关卡返工

- 移除旧版 12 张简单地图重复三遍的关卡集。
- 最终关卡选自 Apache-2.0 的 Boxoban Hard 数据集：
  - 固定 10×10、每关 4 箱与 4 个目标。
  - 本地推箱求解器验证 36 关全部可解。
  - 按最短推动次数排序：17 推至 37 推。
  - 求解搜索状态最高 138,630。
- 最终设备文件：`games/puzzle_arcade/sokoban/levels.tsv`。
- 数据来源：
  - https://huggingface.co/InstaDeepAI/boxoban-levels
  - 固定来源版本：`41fca526e00322682a7cfb0478a76df5643d69b1`

## 构建与资源

- 工程版本：`1.8.11`
- 实验配置：`CONFIG_QDTECH_EXPERIMENT_PUZZLE_ARCADE=y`
- SD 清单：`games/puzzle_arcade/manifest.json`
- SD 校验：`games/puzzle_arcade/SHA256SUMS.txt`
- App 分区仍保留约 7% 余量。

## 验证记录

- v1.8.10 功能版已在 ESP32-S3（8 MB PSRAM）上完整烧录并通过所有写入哈希校验。
- v1.8.11 已完成全量构建；App 镜像大小 6,798,960 字节，镜像内版本为 `1.8.11`，App 分区保留 `0x84190` 字节（约 7%）。
- v1.8.11 App 镜像 SHA-256 为 `7a8ee5faef9385533ebc1352cd740f91e006cd665342063fd0c1550d05c165f7`。
- SD 卡中的最终 Hard 关卡文件与工程文件已做 SHA-256 一致性比对。

## 使用提示

- 游戏馆功能依赖 SD 卡资源目录完整存在。
- 更新固件后，应同时更新发布包中的 `sdcard` 内容。
- 推箱子关卡更新只依赖 SD 文件；固件仍按关卡序号读取，不在 NVS 中缓存关卡内容。
