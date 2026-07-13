#!/usr/bin/env python3
"""Generate QuickDraw Pattern tables from 8x8 PNGs in data/."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

WHITE = 0
BLACK = 1

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
OUT = ROOT / "patterns_generated.c"

PATTERNS = [
    ("missrows.png", "gPatMiss", "gInvertMiss"),
    ("wrongplacerows.png", "gPatWrongPlace", "gInvertWrongPlace"),
    ("correctrows.png", "gPatCorrect", "gInvertCorrect"),
    ("empty.png", "gPatEmpty", "gInvertEmpty"),
]


def apply_scanline_filter(filter_type: int, row: list[int], prev: list[int]) -> list[int]:
    if filter_type == 0:
        return row
    if filter_type == 1:
        out = row[:]
        for i in range(1, len(out)):
            out[i] = (out[i] + out[i - 1]) & 0xFF
        return out
    if filter_type == 2:
        return [(row[i] + prev[i]) & 0xFF for i in range(len(row))]
    if filter_type == 3:
        return [(row[i] + (prev[i] >> 1)) & 0xFF for i in range(len(row))]
    if filter_type == 4:
        out = row[:]
        for i in range(len(out)):
            left = out[i - 1] if i > 0 else 0
            up = prev[i]
            up_left = prev[i - 1] if i > 0 else 0
            out[i] = (out[i] + ((left + up - up_left) & 0xFF)) & 0xFF
        return out
    return row


def rgb_to_mono(r: int, g: int, b: int) -> int:
    return WHITE if (r + g + b) >= 384 else BLACK


def read_png_mono(path: Path) -> list[list[int]]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Not a PNG: {path}")

    pos = 8
    width = height = 0
    bit_depth = 0
    color_type = 0
    palette = b""
    raw = b""

    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        pos += 4
        ctype = data[pos : pos + 4]
        pos += 4
        chunk = data[pos : pos + length]
        pos += length + 4

        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"PLTE":
            palette = chunk
        elif ctype == b"IDAT":
            raw += chunk
        elif ctype == b"IEND":
            break

    if width < 8 or height < 8:
        raise ValueError(f"{path.name} must be at least 8x8 (got {width}x{height})")

    inflated = zlib.decompress(raw)
    offset = 0
    rows: list[list[int]] = []

    if color_type == 3:
        if not palette:
            raise ValueError(f"Indexed PNG missing PLTE: {path}")
        if bit_depth != 8:
            raise ValueError(f"Indexed PNG must be 8-bit in {path}")
        prev = [0] * width
        for _ in range(height):
            filter_type = inflated[offset]
            offset += 1
            row_data = list(inflated[offset : offset + width])
            offset += width
            row_data = apply_scanline_filter(filter_type, row_data, prev)
            prev = row_data
            row = []
            for index in row_data:
                base = index * 3
                if base + 2 >= len(palette):
                    raise ValueError(f"Palette index out of range in {path}")
                r, g, b = palette[base], palette[base + 1], palette[base + 2]
                row.append(rgb_to_mono(r, g, b))
            rows.append(row)
    elif color_type in (2, 6):
        sample_size = 4 if color_type == 6 else 3
        prev = [0] * (width * sample_size)
        for _ in range(height):
            filter_type = inflated[offset]
            offset += 1
            row_data = list(inflated[offset : offset + width * sample_size])
            offset += width * sample_size
            row_data = apply_scanline_filter(filter_type, row_data, prev)
            prev = row_data
            row = []
            for i in range(0, len(row_data), sample_size):
                row.append(rgb_to_mono(row_data[i], row_data[i + 1], row_data[i + 2]))
            rows.append(row)
    elif color_type == 0 and bit_depth == 8:
        prev = [0] * width
        for _ in range(height):
            filter_type = inflated[offset]
            offset += 1
            row_data = list(inflated[offset : offset + width])
            offset += width
            row_data = apply_scanline_filter(filter_type, row_data, prev)
            prev = row_data
            rows.append([WHITE if v >= 128 else BLACK for v in row_data])
    elif color_type == 0 and bit_depth == 1:
        row_bytes = (width + 7) // 8
        for _ in range(height):
            offset += 1
            row = []
            for x in range(width):
                byte = inflated[offset + (x // 8)]
                bit = (byte >> (7 - (x % 8))) & 1
                # In 1-bit grayscale PNG, 1 is typically white.
                row.append(WHITE if bit else BLACK)
            offset += row_bytes
            rows.append(row)
    else:
        raise ValueError(
            f"Unsupported PNG format in {path} "
            f"(type={color_type}, depth={bit_depth})"
        )

    return [row[:8] for row in rows[:8]]


def pack_pattern(mono: list[list[int]]) -> tuple[list[int], int]:
    bytes_out: list[int] = []
    black = 0
    for row in mono:
        byte = 0
        for col, bit in enumerate(row):
            if bit == BLACK:
                byte |= 0x80 >> col
                black += 1
        bytes_out.append(byte)
    invert = 1 if black >= 32 else 0
    return bytes_out, invert


def ascii_preview(mono: list[list[int]]) -> str:
    return "\n".join("".join("#" if v == BLACK else "." for v in row) for row in mono)


def main() -> None:
    lines = [
        "/* Auto-generated by tools/generate_patterns.py — do not edit. */",
        "/* Source PNGs: data/missrows.png, wrongplacerows.png, correctrows.png, empty.png */",
        "",
        "#include <Quickdraw.h>",
        "",
    ]

    for filename, pat_name, inv_name in PATTERNS:
        path = DATA / filename
        mono = read_png_mono(path)
        pat_bytes, invert = pack_pattern(mono)
        preview = ascii_preview(mono).replace("\n", " / ")
        hex_bytes = ", ".join(f"0x{b:02X}" for b in pat_bytes)
        lines.append(f"/* {filename}: {preview} */")
        lines.append(f"Pattern {pat_name} = {{ {{ {hex_bytes} }} }};")
        lines.append(f"short {inv_name} = {invert};")
        lines.append("")
        print(f"{filename}:")
        print(ascii_preview(mono))
        print(f"  -> invert={invert}")
        print()

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT.name}")


if __name__ == "__main__":
    main()
