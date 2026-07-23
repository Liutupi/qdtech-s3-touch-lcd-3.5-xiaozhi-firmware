#!/usr/bin/env python3
"""Create the SD-card resource pack used by Shake Lab's I Ching page.

The artwork intentionally contains no Chinese glyphs: the firmware renders the
name and reading from index.tsv using its bundled Chinese font, while every
editable word and every JPEG stays on the SD card.
"""

from pathlib import Path
import shutil
import subprocess
import sys


# id | name | six lines from bottom to top (1=solid, 0=broken) | modern prompt
DATA = """01_qian|乾为天|111111|适合主动承担，但先把方向与承诺想清楚。
02_kun|坤为地|000000|以包容和耐心成事，先把基础照顾周全。
03_zhun|水雷屯|100010|起步虽乱，先做最小的一步，不必急着求快。
04_meng|山水蒙|010001|保持学习姿态，向可靠的人请教会更顺。
05_xu|水天需|111010|机会在路上，准备好自己，比催促结果更重要。
06_song|天水讼|010111|争执里先守边界，别让一时情绪替你作答。
07_shi|地水师|010000|需要组织与纪律，和同伴明确分工再推进。
08_bi|水地比|000010|适合建立连接，真诚合作会带来支持。
09_xiaoxu|风天小畜|111011|小有积蓄，先收拢资源，暂不必孤注一掷。
10_lv|天泽履|110111|按规则稳步前行，谦逊会让你走得更远。
11_tai|地天泰|111000|上下通达，适合沟通、协作与推动好计划。
12_pi|天地否|000111|外部不畅时先修内功，减少无谓消耗。
13_tongren|天火同人|101111|以共同目标聚人，坦诚比技巧更有力量。
14_dayou|火天大有|111101|资源正聚拢，记得分享成果并保持克制。
15_qian|地山谦|001000|谦而不弱，低调完成眼前最关键的工作。
16_yu|雷地豫|000100|气氛渐暖，可以行动；别忘了预留余地。
17_sui|泽雷随|100110|顺势不等于盲从，先辨认什么真正值得跟随。
18_gu|山风蛊|011001|旧问题该整理了，从一个长期拖延点开始。
19_lin|地泽临|110000|机会靠近，及时回应；温和而坚定地迈出去。
20_guan|风地观|000011|先观察全局，再决定自己应该站在哪一边。
21_shihe|火雷噬嗑|100101|有阻碍就厘清规则，用清楚的行动打开局面。
22_bi|山火贲|101001|适度修饰能增色，核心仍是内容与诚意。
23_bo|山地剥|000001|不必硬撑旧结构，放下无效部分也是进展。
24_fu|地雷复|100000|重回初心，恢复一个对你真正有益的日常。
25_wuwang|天雷无妄|100111|真诚做事，少一些算计，反而能避开偏差。
26_daxu|山天大畜|111001|先蓄力再发，学习与准备会成为你的底气。
27_yi|山雷颐|100001|照顾身体与语言，输入什么会决定你输出什么。
28_daguo|泽风大过|011110|责任偏重，别独扛；及时调整结构与节奏。
29_kan|坎为水|010010|反复的难关要靠经验过，不逞强、不冒险。
30_li|离为火|101101|保持清明与热情，愿景需要持续的专注照亮。
31_xian|泽山咸|001110|感受正在被回应，真诚表达比猜测更好。
32_heng|雷风恒|011100|长期主义见效，坚持一件正确的小事。
33_dun|天山遁|001111|适当退一步保护能量，为下一次出发留空间。
34_dazhuang|雷天大壮|111100|力量充足也要有分寸，稳比猛更能赢得尊重。
35_jin|火地晋|000101|进展可见，及时展示成果，也感谢一路的支持。
36_mingyi|地火明夷|101000|光暂时收起来，先保护自己与重要的计划。
37_jiaren|风火家人|101011|把近处的关系经营好，秩序会带来安心。
38_kui|火泽睽|110101|分歧不必立刻消除，先找出双方共同的小目标。
39_jian|水山蹇|001010|路有阻力，换路线或找帮手，比硬闯更明智。
40_xie|雷水解|010100|紧张正在消散，先松一口气，再处理余下问题。
41_sun|山泽损|110001|做减法会更轻盈，舍去一项低价值消耗。
42_yi|风雷益|100011|利在互助与增长，今天适合学习或帮助他人。
43_guai|泽天夬|111110|该说清楚的事要说清楚，态度坚定但不伤人。
44_gou|天风姤|011111|意外相遇带来新讯号，保持判断，不急着投入。
45_cui|泽地萃|000110|适合聚集资源与同伴，让共识先于行动。
46_sheng|地风升|011000|一点点向上即可，稳定积累会超过一时冲刺。
47_kun|泽水困|010110|感到受限时先守住核心，不要用透支换结果。
48_jing|水风井|011010|回到长期资源：技能、关系与健康都值得维护。
49_ge|泽火革|101110|改变时机到了，先说清旧规则如何结束、新规则如何开始。
50_ding|火风鼎|011101|把经验炼成能力，适合打磨作品和方法。
51_zhen|震为雷|100100|突发变化先稳住，安顿好自己再作反应。
52_gen|艮为山|001001|适合暂停与定心，不必每件事都立刻回应。
53_jian|风山渐|001011|循序渐进最可靠，慢一点反而走得稳。
54_guimei|雷泽归妹|110100|关系与合作要讲清位置，别在仓促中许诺。
55_feng|雷火丰|101100|能见度很高，适合完成重要展示，也留意别过度消耗。
56_lv|火山旅|001101|身处变化中，轻装前行，先让自己适应新环境。
57_xun|巽为风|011011|柔和渗透比强推有效，持续沟通会打开空间。
58_dui|兑为泽|110110|愉悦来自交流，和信任的人聊聊会有新灵感。
59_huan|风水涣|010011|散乱可以被重新整理，先聚焦一件最重要的事。
60_jie|水泽节|110010|设好界限与预算，节制不是压抑，而是留有余裕。
61_zhongfu|风泽中孚|110011|以真心取信，答应的事就把它做好。
62_xiaoguo|雷山小过|001100|小事宜认真，大事宜谨慎；别为了快而跳步。
63_jiji|水火既济|101010|事情接近完成，更要认真收尾并检查细节。
64_weiji|火水未济|010101|尚未完成并不等于失败，保持耐心，下一步会更清楚。"""


def put_pixel(canvas, width, x, y, color):
    if 0 <= x < width and 0 <= y < width:
        p = (y * width + x) * 3
        canvas[p:p + 3] = bytes(color)


def rect(canvas, width, x, y, w, h, color):
    for yy in range(max(0, y), min(width, y + h)):
        start = (yy * width + max(0, x)) * 3
        end = (yy * width + min(width, x + w)) * 3
        canvas[start:end] = bytes(color) * max(0, min(width, x + w) - max(0, x))


def write_card(path, bits, ordinal):
    size = 180
    paper = (239, 224, 187)
    ink = (35, 28, 25)
    cinnabar = (164, 57, 48)
    gold = (183, 137, 62)
    canvas = bytearray(bytes(paper) * (size * size))
    # A restrained ink-paper field: warm border, seal, and small gold dust.
    rect(canvas, size, 8, 8, 164, 2, cinnabar)
    rect(canvas, size, 8, 170, 164, 2, cinnabar)
    rect(canvas, size, 8, 8, 2, 164, cinnabar)
    rect(canvas, size, 170, 8, 2, 164, cinnabar)
    for i in range(18):
        x = (ordinal * 29 + i * 41) % 152 + 14
        y = (ordinal * 17 + i * 31) % 148 + 16
        rect(canvas, size, x, y, 2, 2, gold)
    rect(canvas, size, 144, 18, 20, 20, cinnabar)
    rect(canvas, size, 150, 24, 8, 8, paper)
    for row, bit in enumerate(bits):
        y = 46 + (5 - row) * 18
        if bit == "1":
            rect(canvas, size, 42, y, 96, 10, ink)
        else:
            rect(canvas, size, 42, y, 40, 10, ink)
            rect(canvas, size, 98, y, 40, 10, ink)
    with path.open("wb") as output:
        output.write(f"P6\n{size} {size}\n255\n".encode("ascii"))
        output.write(canvas)


def main():
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home() / "Desktop" / "摇卦SD资源包"
    if output.exists():
        shutil.rmtree(output)
    image_dir = output / "shake_lab" / "divination" / "images"
    image_dir.mkdir(parents=True)
    rows = []
    for ordinal, raw in enumerate(DATA.splitlines(), 1):
        item_id, name, bits, guidance = raw.split("|", 3)
        ppm = image_dir / f"{item_id}.ppm"
        jpg = image_dir / f"{item_id}.jpg"
        write_card(ppm, bits, ordinal)
        subprocess.run(["cjpeg", "-quality", "88", "-outfile", str(jpg), str(ppm)], check=True)
        ppm.unlink()
        judgment = f"{name} · 今日卦意"
        rows.append("\t".join((item_id, name, f"images/{item_id}.jpg", judgment, guidance, bits)))
    index = output / "shake_lab" / "divination" / "index.tsv"
    index.write_text(
        "# id\t卦名\t图片相对路径\t卦辞标题\t现代解读\t六爻（自下而上，1阳0阴）\\n"
        "# 可用 \\n 在文字中换行；字段之间请保留制表符。\\n" + "\n".join(rows) + "\n",
        encoding="utf-8",
    )
    (output / "README.txt").write_text(
        "摇卦 SD 资源包\n\n"
        "使用方法：把本文件夹里的 shake_lab 文件夹整体复制到 SD 卡根目录。\n"
        "最终路径必须是：/sdcard/shake_lab/divination/index.tsv\n\n"
        "每次摇卦，固件会从 index.tsv 随机选择一卦，读取相应 JPG 与文字。\n"
        "你可以替换 images 里的 JPG（建议 180×180、JPG、单张小于 256KB），\n"
        "并修改 index.tsv 中的文字；不要改变制表符分隔的五列结构。\n\n"
        "本功能用于传统文化与自我反思，不构成任何现实决策建议。\n",
        encoding="utf-8",
    )
    print(output)


if __name__ == "__main__":
    main()
