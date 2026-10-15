#!/usr/bin/env python3
"""Dump PE headers, sections, and imports from DXBALL.EXE."""
from __future__ import annotations

import struct
import sys
from pathlib import Path


def read_cstr(data: bytes, off: int) -> str:
    end = data.find(b"\x00", off)
    if end < 0:
        end = len(data)
    return data[off:end].decode("latin1", "replace")


def parse_pe(path: Path) -> str:
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise SystemExit("not MZ")
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        raise SystemExit("not PE")
    machine, nsec, ts, _, _, optsz, chars = struct.unpack_from(
        "<HHIIIHH", data, e_lfanew + 4
    )
    opt = e_lfanew + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10B:
        raise SystemExit(f"unexpected optional magic {magic:#x}")
    entry = struct.unpack_from("<I", data, opt + 16)[0]
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    subsys = struct.unpack_from("<H", data, opt + 68)[0]
    dd_off = opt + 96
    n_dd = struct.unpack_from("<I", data, opt + 92)[0]
    lines = [
        f"file={path}",
        f"size={len(data)}",
        f"machine={machine:#x}",
        f"sections={nsec}",
        f"timestamp={ts}",
        f"characteristics={chars:#x}",
        f"entry_rva={entry:#x}",
        f"entry_va={image_base + entry:#x}",
        f"image_base={image_base:#x}",
        f"subsystem={subsys}",
        "",
        "sections:",
    ]
    sec_off = opt + optsz
    sections = []
    for i in range(nsec):
        off = sec_off + i * 40
        name = data[off:off + 8].split(b"\x00", 1)[0].decode("latin1")
        vsz, va, rsz, raw = struct.unpack_from("<IIII", data, off + 8)
        sections.append((name, va, vsz, raw, rsz))
        lines.append(
            f"  {name:8} va={va:#08x} vsz={vsz:#x} raw={raw:#08x} rsz={rsz:#x}"
        )

    def rva_to_off(rva: int) -> int | None:
        for _name, va, vsz, raw, rsz in sections:
            span = max(vsz, rsz)
            if va <= rva < va + span:
                return raw + (rva - va)
        return None

    lines.append("")
    lines.append("imports:")
    if n_dd > 1:
        imp_rva, imp_sz = struct.unpack_from("<II", data, dd_off + 8)
        off = rva_to_off(imp_rva) if imp_rva else None
        if off is not None:
            while True:
                ilt, timedate, fwd, name_rva, iat = struct.unpack_from(
                    "<IIIII", data, off
                )
                if ilt == 0 and name_rva == 0 and iat == 0:
                    break
                dll_off = rva_to_off(name_rva)
                dll = read_cstr(data, dll_off) if dll_off is not None else "?"
                lines.append(f"  {dll}")
                thunk_rva = ilt or iat
                thunk_off = rva_to_off(thunk_rva)
                if thunk_off is not None:
                    t = thunk_off
                    while True:
                        thunk = struct.unpack_from("<I", data, t)[0]
                        if thunk == 0:
                            break
                        if thunk & 0x80000000:
                            lines.append(f"    ord={thunk & 0xFFFF}")
                        else:
                            hint_off = rva_to_off(thunk)
                            if hint_off is not None:
                                hint = struct.unpack_from("<H", data, hint_off)[0]
                                nm = read_cstr(data, hint_off + 2)
                                lines.append(f"    {nm} hint={hint}")
                        t += 4
                off += 20
    return "\n".join(lines) + "\n"


def main() -> None:
    path = Path(sys.argv[1] if len(sys.argv) > 1 else
                "/home/scp1/dev/dxball/original/Dxball/DXBALL.EXE")
    text = parse_pe(path)
    out = Path("/home/scp1/dev/dxball/notes/pe.txt")
    out.write_text(text)
    sys.stdout.write(text)


if __name__ == "__main__":
    main()
