#!/usr/bin/env python3
"""Generate deterministic, solver-scored Sokoban boards for the SD pack."""

from __future__ import annotations

import random
import argparse
from collections import deque
from pathlib import Path


def solve(rows: list[str], state_limit: int = 12_000) -> tuple[int, int, int] | None:
    height, width = len(rows), len(rows[0])
    walls: set[tuple[int, int]] = set()
    goals: set[tuple[int, int]] = set()
    boxes: set[tuple[int, int]] = set()
    player = (-1, -1)
    for y, row in enumerate(rows):
        for x, value in enumerate(row):
            pos = (x, y)
            if value == "#":
                walls.add(pos)
            elif value in ".*+":
                goals.add(pos)
            if value in "$*":
                boxes.add(pos)
            if value in "@+":
                player = pos
    if player == (-1, -1) or len(boxes) != len(goals):
        return None

    queue = deque([(player, frozenset(boxes), 0)])
    seen = {(player, frozenset(boxes))}
    explored = 0
    while queue and explored < state_limit:
        player, boxes_state, pushes = queue.popleft()
        explored += 1
        if set(boxes_state) == goals:
            return pushes, pushes, explored
        boxes_set = set(boxes_state)
        reachable = {player}
        flood = [player]
        while flood:
            px, py = flood.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nxt = (px + dx, py + dy)
                if nxt not in walls and nxt not in boxes_set and nxt not in reachable:
                    reachable.add(nxt)
                    flood.append(nxt)
        for box in boxes_set:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                stand = (box[0] - dx, box[1] - dy)
                beyond = (box[0] + dx, box[1] + dy)
                if stand not in reachable or beyond in walls or beyond in boxes_set:
                    continue
                # A non-goal corner is an irreversible deadlock.
                if beyond not in goals:
                    bx, by = beyond
                    horizontal = (bx - 1, by) in walls or (bx + 1, by) in walls
                    vertical = (bx, by - 1) in walls or (bx, by + 1) in walls
                    if horizontal and vertical:
                        continue
                moved = frozenset((boxes_set - {box}) | {beyond})
                state = (box, moved)
                if state not in seen:
                    seen.add(state)
                    queue.append((box, moved, pushes + 1))
    return None


def random_board(rng: random.Random, width: int, height: int, box_count: int) -> list[str]:
    grid = [["#" if x in (0, width - 1) or y in (0, height - 1) else " "
             for x in range(width)] for y in range(height)]
    interior = [(x, y) for y in range(1, height - 1) for x in range(1, width - 1)]
    for x, y in rng.sample(interior, rng.randint(3, 7)):
        grid[y][x] = "#"
    walls = {(x, y) for y in range(height) for x in range(width) if grid[y][x] == "#"}
    floors = [(x, y) for x, y in interior if (x, y) not in walls]
    chosen = rng.sample(floors, box_count + 1)
    goals = set(chosen[:box_count])
    boxes = set(goals)
    player = chosen[-1]

    for _ in range(18 + box_count * 9):
        reachable = {player}
        flood = [player]
        while flood:
            px, py = flood.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nxt = (px + dx, py + dy)
                if nxt not in walls and nxt not in boxes and nxt not in reachable:
                    reachable.add(nxt)
                    flood.append(nxt)
        pulls = []
        for box in boxes:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                stand = (box[0] - dx, box[1] - dy)
                previous_player = (box[0] - 2 * dx, box[1] - 2 * dy)
                if (stand in reachable and previous_player not in walls and
                        previous_player not in boxes):
                    pulls.append((box, stand, previous_player))
        if not pulls:
            break
        box, destination, previous_player = rng.choice(pulls)
        boxes.remove(box)
        boxes.add(destination)
        player = previous_player

    for x, y in goals:
        grid[y][x] = "."
    for x, y in boxes:
        grid[y][x] = "*" if (x, y) in goals else "$"
    px, py = player
    grid[py][px] = "+" if player in goals else "@"
    return ["".join(row) for row in grid]


def decode_boxoban(static, dynamic) -> list[str]:
    rows: list[str] = []
    for y in range(10):
        row: list[str] = []
        for x in range(10):
            floor = int(static[y, x])
            actor = int(dynamic[y, x])
            if floor == 1:
                value = "#"
            elif floor == 2 and actor == 4:
                value = "*"
            elif floor == 2 and actor == 3:
                value = "+"
            elif floor == 2:
                value = "."
            elif actor == 4:
                value = "$"
            elif actor == 3:
                value = "@"
            else:
                value = " "
            row.append(value)
        rows.append("".join(row))
    return rows


def write_boxoban_hard(source: Path, output: Path) -> None:
    import numpy as np

    data = np.load(source)
    candidates: list[tuple[int, int, int, list[str]]] = []
    for source_index in range(min(80, len(data))):
        rows = decode_boxoban(data[source_index, :, :, 0],
                              data[source_index, :, :, 1])
        result = solve(rows, 500_000)
        if result:
            _, pushes, explored = result
            candidates.append((pushes, explored, source_index, rows))
    selected = sorted(candidates, reverse=True)[:36]
    selected.sort(key=lambda item: (item[0], item[1]))
    if len(selected) != 36 or selected[0][0] < 15:
        raise RuntimeError("Boxoban Hard selection did not meet the difficulty gate")
    records = ["# id\tname\twidth\theight\trows"]
    for index, (pushes, explored, source_index, rows) in enumerate(selected, 1):
        records.append(
            f"SKB{index:03d}\t硬核 {index:02d}·{pushes}推\t10\t10\t{'/'.join(rows)}"
        )
        print(f"SKB{index:03d} source={source_index} pushes={pushes} states={explored}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(records) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument("--boxoban", type=Path)
    args = parser.parse_args()
    if args.boxoban:
        if not args.output:
            raise RuntimeError("--boxoban requires --output")
        write_boxoban_hard(args.boxoban, args.output)
        return
    rng = random.Random(1810)
    accepted: list[tuple[list[str], tuple[int, int, int]]] = []
    seen: set[str] = set()
    attempts = 0
    while len(accepted) < 36 and attempts < 60_000:
        attempts += 1
        tier = len(accepted) // 12
        box_count = 2 if tier == 0 else 3
        width = 8 + (tier > 0)
        height = 7 + (tier > 1)
        rows = random_board(rng, width, height, box_count)
        key = "/".join(rows)
        if key in seen:
            continue
        result = solve(rows)
        if not result:
            continue
        moves, pushes, explored = result
        minimum_pushes = (6, 9, 12)[tier]
        if pushes < minimum_pushes or explored < 35:
            continue
        seen.add(key)
        accepted.append((rows, result))

    if len(accepted) != 36:
        raise RuntimeError(f"only generated {len(accepted)} levels after {attempts} attempts")
    output_rows = ["# id\tname\twidth\theight\trows"]
    for index, (rows, result) in enumerate(accepted, 1):
        moves, pushes, explored = result
        record = (
            f"SKB{index:03d}\t挑战 {index:02d}·{pushes}推\t"
            f"{len(rows[0])}\t{len(rows)}\t{'/'.join(rows)}"
        )
        output_rows.append(record)
        print(f"{record}\t# pushes={pushes} states={explored}")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("\n".join(output_rows) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
