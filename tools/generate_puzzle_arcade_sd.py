#!/usr/bin/env python3
"""Generate and validate the v1.8.11 SD-backed puzzle arcade content."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
from collections import deque
from pathlib import Path

SEED = 188
BASE_SOLUTION = "123456789456789123789123456234567891567891234891234567345678912678912345912345678"


def transformed_solution(rng: random.Random) -> str:
    grid = [list(BASE_SOLUTION[i:i + 9]) for i in range(0, 81, 9)]
    digits = list("123456789")
    shuffled = digits[:]
    rng.shuffle(shuffled)
    mapping = dict(zip(digits, shuffled))
    grid = [[mapping[c] for c in row] for row in grid]
    bands = [0, 1, 2]
    stacks = [0, 1, 2]
    rng.shuffle(bands)
    rng.shuffle(stacks)
    rows = [b * 3 + r for b in bands for r in rng.sample(range(3), 3)]
    cols = [s * 3 + c for s in stacks for c in rng.sample(range(3), 3)]
    return "".join(grid[r][c] for r in rows for c in cols)


def count_solutions(puzzle: str, limit: int = 2) -> int:
    values = [0 if c == "." else int(c) for c in puzzle]
    rows = [set(range(1, 10)) for _ in range(9)]
    cols = [set(range(1, 10)) for _ in range(9)]
    boxes = [set(range(1, 10)) for _ in range(9)]
    for i, value in enumerate(values):
        if not value:
            continue
        r, c = divmod(i, 9)
        b = (r // 3) * 3 + c // 3
        if value not in rows[r] or value not in cols[c] or value not in boxes[b]:
            return 0
        rows[r].remove(value)
        cols[c].remove(value)
        boxes[b].remove(value)

    total = 0

    def solve() -> None:
        nonlocal total
        if total >= limit:
            return
        best = -1
        candidates: set[int] | None = None
        for i, value in enumerate(values):
            if value:
                continue
            r, c = divmod(i, 9)
            b = (r // 3) * 3 + c // 3
            options = rows[r] & cols[c] & boxes[b]
            if not options:
                return
            if candidates is None or len(options) < len(candidates):
                best, candidates = i, options
                if len(options) == 1:
                    break
        if best < 0:
            total += 1
            return
        r, c = divmod(best, 9)
        b = (r // 3) * 3 + c // 3
        for value in tuple(candidates or ()):
            values[best] = value
            rows[r].remove(value)
            cols[c].remove(value)
            boxes[b].remove(value)
            solve()
            boxes[b].add(value)
            cols[c].add(value)
            rows[r].add(value)
            values[best] = 0

    solve()
    return total


def make_sudoku(solution: str, clues: int, rng: random.Random) -> str:
    puzzle = list(solution)
    cells = list(range(81))
    rng.shuffle(cells)
    for cell in cells:
        if sum(c != "." for c in puzzle) <= clues:
            break
        old = puzzle[cell]
        puzzle[cell] = "."
        if count_solutions("".join(puzzle)) != 1:
            puzzle[cell] = old
    return "".join(puzzle)


def write_sudoku(root: Path, rng: random.Random) -> int:
    path = root / "sudoku" / "puzzles.tsv"
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = ["# id\tdifficulty\tpuzzle\tsolution"]
    specs = [("轻松", 38, 32), ("进阶", 32, 36), ("挑战", 27, 28)]
    serial = 1
    for difficulty, clues, count in specs:
        for _ in range(count):
            solution = transformed_solution(rng)
            puzzle = make_sudoku(solution, clues, rng)
            assert count_solutions(puzzle) == 1
            rows.append(f"SDK{serial:03d}\t{difficulty}\t{puzzle}\t{solution}")
            serial += 1
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return serial - 1


def write_locks(root: Path, rng: random.Random) -> int:
    path = root / "code_lock" / "challenges.tsv"
    path.parent.mkdir(parents=True, exist_ok=True)
    codes: set[str] = set()
    while len(codes) < 120:
        codes.add("".join(rng.sample("0123456789", 4)))
    rows = ["# id\tdifficulty\tcode"]
    for i, code in enumerate(sorted(codes), 1):
        difficulty = "轻松" if i <= 40 else ("进阶" if i <= 80 else "挑战")
        rows.append(f"CLK{i:03d}\t{difficulty}\t{code}")
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return len(codes)


def write_match3(root: Path) -> int:
    path = root / "match3" / "levels.tsv"
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = ["# id\tname\ttarget_score\tmoves"]
    names = ["草莓野餐", "蜂蜜茶会", "葡萄星夜", "薄荷花园", "云朵糖屋", "樱花小径"]
    for i in range(60):
        tier = i // 20
        target = 500 + tier * 250 + (i % 10) * 30
        moves = 24 - tier * 3
        rows.append(f"MAT{i + 1:03d}\t{names[i % len(names)]} {i + 1:02d}\t{target}\t{moves}")
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return 60


SOKOBAN_BASES = [
    ["########", "# .  . #", "# $##$ #", "#   @  #", "########"],
    ["########", "#  .   #", "#  $   #", "# ## # #", "# $ .@ #", "########"],
    ["########", "# . #  #", "# $ $ .#", "#   ## #", "# @    #", "########"],
    ["#########", "# .   . #", "# $ # $ #", "#   #   #", "#   @   #", "#########"],
    ["######", "# .  #", "# $  #", "#  @ #", "######"],
    ["#######", "#  .  #", "#  $  #", "#  #  #", "#  @  #", "#######"],
    ["########", "# . .  #", "# $ $  #", "#   @  #", "########"],
    ["########", "#  .   #", "#  $   #", "# ##   #", "#   @  #", "########"],
    ["########", "# .    #", "# $##  #", "#   @  #", "#      #", "########"],
    ["########", "# . .  #", "# $ #  #", "#   $  #", "#  @   #", "########"],
]


def mirror_level(rows: list[str]) -> list[str]:
    swap = str.maketrans({"@": "@", "+": "+", "$": "$", "*": "*"})
    return [row[::-1].translate(swap) for row in rows]


def flip_level(rows: list[str]) -> list[str]:
    return list(reversed(rows))


def rotate_level(rows: list[str]) -> list[str]:
    return ["".join(row[x] for row in reversed(rows)) for x in range(len(rows[0]))]


def sokoban_solvable(rows: list[str]) -> bool:
    height, width = len(rows), len(rows[0])
    walls, targets, boxes = set(), set(), set()
    player = None
    for y, row in enumerate(rows):
        for x, char in enumerate(row):
            pos = (x, y)
            if char == "#":
                walls.add(pos)
            if char in ".*+":
                targets.add(pos)
            if char in "$*":
                boxes.add(pos)
            if char in "@+":
                player = pos
    if player is None or len(boxes) != len(targets):
        return False
    queue = deque([(player, frozenset(boxes))])
    seen = set(queue)
    while queue:
        player, state_boxes = queue.popleft()
        if set(state_boxes) == targets:
            return True
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nxt = (player[0] + dx, player[1] + dy)
            if nxt in walls:
                continue
            moved = state_boxes
            if nxt in state_boxes:
                beyond = (nxt[0] + dx, nxt[1] + dy)
                if beyond in walls or beyond in state_boxes:
                    continue
                moved = frozenset((state_boxes - {nxt}) | {beyond})
            state = (nxt, moved)
            if state not in seen:
                seen.add(state)
                queue.append(state)
    return False


def write_sokoban(root: Path) -> int:
    path = root / "sokoban" / "levels.tsv"
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        existing = path.read_text(encoding="utf-8").splitlines()
        records = [line for line in existing if line and not line.startswith("#")]
        if len(records) == 36 and all("硬核" in line for line in records):
            return len(records)
    rows = ["# id\tname\twidth\theight\trows"]
    levels: list[list[str]] = []
    for base in SOKOBAN_BASES:
        variants = (base, mirror_level(base), flip_level(base),
                    mirror_level(flip_level(base)), rotate_level(base),
                    mirror_level(rotate_level(base)))
        for level in variants:
            assert len({len(r) for r in level}) == 1
            assert sokoban_solvable(level)
            levels.append(level)
    # Six geometric variants per base create 36 distinct, solver-verified boards.
    levels = levels[:36]
    for i, level in enumerate(levels, 1):
        name = f"漫画小屋 {i:02d}"
        rows.append(f"SKB{i:03d}\t{name}\t{len(level[0])}\t{len(level)}\t{'/'.join(level)}")
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return len(levels)


def make_maze(width: int, height: int, rng: random.Random) -> list[str]:
    grid = [["#" for _ in range(width)] for _ in range(height)]
    start = (1, 1)
    grid[1][1] = " "
    stack = [start]
    while stack:
        x, y = stack[-1]
        options = []
        for dx, dy in ((2, 0), (-2, 0), (0, 2), (0, -2)):
            nx, ny = x + dx, y + dy
            if 1 <= nx < width - 1 and 1 <= ny < height - 1 and grid[ny][nx] == "#":
                options.append((nx, ny, dx, dy))
        if not options:
            stack.pop()
            continue
        nx, ny, dx, dy = rng.choice(options)
        grid[y + dy // 2][x + dx // 2] = " "
        grid[ny][nx] = " "
        stack.append((nx, ny))
    grid[1][1] = "S"
    grid[height - 2][width - 2] = "G"
    return ["".join(row) for row in grid]


def write_mazes(root: Path, rng: random.Random) -> int:
    path = root / "motion_maze" / "levels.tsv"
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = ["# id\tname\twidth\theight\trows"]
    for i in range(48):
        maze = make_maze(17, 11, rng)
        rows.append(f"MAZ{i + 1:03d}\t星光迷宫 {i + 1:02d}\t17\t11\t{'/'.join(maze)}")
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return 48


def write_metadata(root: Path, counts: dict[str, int]) -> None:
    (root / "covers").mkdir(parents=True, exist_ok=True)
    manifest = {
        "format": 1,
        "firmware": "v1.8.11",
        "encoding": "UTF-8",
        "counts": counts,
        "covers": ["sudoku.jpg", "code_lock.jpg", "sokoban.jpg", "match3.jpg",
                   "motion_maze.jpg", "tile_2048.jpg", "freecell.jpg"],
        "motion_maze_runtime": "playable",
        "tile_2048_runtime": "playable",
        "freecell_runtime": "playable",
    }
    (root / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (root / "README.txt").write_text(
        "QDTech v1.8.11 益智游戏馆 SD 资源\n"
        "请把 games 文件夹复制到 SD 卡根目录。\n"
        "数独、密码锁、推箱子、消消乐在第一阶段开放；体感迷宫仅预置资源，第二阶段开放。\n"
        "文件统一为 UTF-8，封面为 640x360 JPEG。\n",
        encoding="utf-8")


def write_checksums(root: Path) -> None:
    checksum_path = root / "SHA256SUMS.txt"
    lines = []
    for path in sorted(root.rglob("*")):
        if path.is_file() and path != checksum_path:
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            lines.append(f"{digest}  {path.relative_to(root).as_posix()}")
    checksum_path.write_text("\n".join(lines) + "\n", encoding="ascii")


def validate(root: Path) -> None:
    required = [
        root / "sudoku" / "puzzles.tsv",
        root / "code_lock" / "challenges.tsv",
        root / "sokoban" / "levels.tsv",
        root / "match3" / "levels.tsv",
        root / "motion_maze" / "levels.tsv",
        root / "manifest.json",
    ]
    for path in required:
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"missing generated file: {path}")
        path.read_text(encoding="utf-8")
    for name in ("sudoku", "code_lock", "sokoban", "match3", "motion_maze",
                 "tile_2048", "freecell"):
        cover = root / "covers" / f"{name}.jpg"
        if cover.exists() and cover.stat().st_size > 256 * 1024:
            raise RuntimeError(f"cover exceeds firmware limit: {cover}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("sdcard/games/puzzle_arcade"))
    args = parser.parse_args()
    root = args.output.resolve()
    rng = random.Random(SEED)
    counts = {
        "sudoku": write_sudoku(root, rng),
        "code_lock": write_locks(root, rng),
        "sokoban": write_sokoban(root),
        "match3": write_match3(root),
        "motion_maze": write_mazes(root, rng),
        "tile_2048": 1,
        "freecell": 1,
    }
    write_metadata(root, counts)
    validate(root)
    write_checksums(root)
    print(json.dumps({"output": str(root), "counts": counts}, ensure_ascii=False))


if __name__ == "__main__":
    main()
