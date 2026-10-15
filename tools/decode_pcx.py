#!/usr/bin/env python3
"""Decode 8-bit PCX files from the original DX-Ball data."""
from __future__ import annotations

import struct
import sys
from pathlib import Path


def decode_pcx(path: Path) -> tuple[int, int, bytes, bytes]:
    data = path.read_bytes()
    if data[0] != 0x0A:
        raise SystemExit(f"{path}: not PCX")
    xmin, ymin, xmax, ymax = struct.unpack_from("<HHHH", data, 4)
    w = xmax - xmin + 1
    h = ymax - ymin + 1
    bits = data[3]
    planes = data[65]
    bpl = struct.unpack_from("<H", data, 66)[0]
    if bits != 8 or planes != 1:
        raise SystemExit(f"{path}: expected 8-bit 1-plane, got {bits}/{planes}")
    pal = data[-768:]
    src = data[128:-768]
    pixels = bytearray(w * h)
    i = 0
    o = 0
    for y in range(h):
        x = 0
        row = bytearray()
        while x < bpl:
            b = src[i]
            i += 1
            if (b & 0xC0) == 0xC0:
                n = b & 0x3F
                v = src[i]
                i += 1
                row.extend([v] * n)
                x += n
            else:
                row.append(b)
                x += 1
        pixels[o:o + w] = row[:w]
        o += w
    return w, h, bytes(pixels), pal


def write_ppm(path: Path, w: int, h: int, pixels: bytes, pal: bytes) -> None:
    out = bytearray()
    out += f"P6\n{w} {h}\n255\n".encode("ascii")
    for idx in pixels:
        p = idx * 3
        out.extend(pal[p:p + 3])
    path.write_bytes(out)


def main() -> None:
    src_dir = Path("/home/scp1/dev/dxball/original/Dxball")
    dst_dir = Path("/home/scp1/dev/dxball/notes/pcx")
    dst_dir.mkdir(parents=True, exist_ok=True)
    for path in sorted(src_dir.glob("*.PCX")):
        w, h, pixels, pal = decode_pcx(path)
        write_ppm(dst_dir / (path.stem.lower() + ".ppm"), w, h, pixels, pal)
        print(f"{path.name}: {w}x{h}")


if __name__ == "__main__":
    main()
