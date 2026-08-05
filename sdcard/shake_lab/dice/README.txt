QDTech v1.8.17 立体骰子 SD 资源

请把 shake_lab 文件夹复制到 SD 卡根目录，最终必须包含：

/shake_lab/dice/stage.rgb565
/shake_lab/dice/roll.argb8888
/shake_lab/dice/land.argb8888

stage.rgb565：480x320 RGB565 小端序，307200 字节。
roll.argb8888：12帧、每帧96x96、LVGL ARGB8888，442368 字节。
land.argb8888：1至6点落地姿态、每帧96x96，221184 字节。

stage-preview.jpg 和 dice-sprites-preview.png 只用于电脑预览，固件不会读取。
如果舞台图片缺失，固件使用深青色满屏背景；如果旋转或落地精灵缺失，
自动回退到原来的平面骰子。摇动检测和1至6点随机结果不会失效。
