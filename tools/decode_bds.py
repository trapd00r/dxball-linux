#!/usr/bin/env python3
"""Dump DEFAULT.BDS as 50 boards of 20x20 tiles."""
from __future__ import annotations

from pathlib import Path

COLS = 20
ROWS = 20
BOARDS = 50
TILES = COLS * ROWS


def glyph(tile: int) -> str:
    if tile == 0:
        return "."
    if tile < 16:
        return format(tile, "X")
    return chr(ord("a") + (tile - 16) % 26)


def main() -> None:
    path = Path("/home/scp1/dev/dxball/original/Dxball/DEFAULT.BDS")
    data = path.read_bytes()
    if len(data) != BOARDS * TILES:
        raise SystemExit(f"unexpected size {len(data)}")
    out = Path("/home/scp1/dev/dxball/notes/boards.txt")
    counts: dict[int, int] = {}
    lines: list[str] = []
    for b in range(BOARDS):
        chunk = data[b * TILES:(b + 1) * TILES]
        lines.append(f"board {b:02d}")
        for r in range(ROWS):
            row = chunk[r * COLS:(r + 1) * COLS]
            lines.append("".join(glyph(v) for v in row))
            for v in row:
                counts[v] = counts.get(v, 0) + 1
        lines.append("")
    lines.append("tile counts:")
    for k in sorted(counts):
        lines.append(f"  {k:3} {counts[k]:5} {glyph(k)}")
    out.write_text("\n".join(lines) + "\n")
    print(f"wrote {out} ({BOARDS} boards)")
    print("unique tiles:", len(counts))


if __name__ == "__main__":
    main()
