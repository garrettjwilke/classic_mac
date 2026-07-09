#!/usr/bin/env python3
"""Generate indexed black-and-white tile PNGs and tiles_generated.r for Classic Mac."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

TILE_SIZE = 16

WHITE = 0
BLACK = 1

# Indexed PNG palette: index 0 = black, index 1 = white (common editor default).
PALETTE = bytes([0, 0, 0, 255, 255, 255])

TILES = [
    ("covered.png", 128, "?"),
    ("flag.png", 129, "F"),
    ("mine.png", 130, "*"),
    ("mine_hit.png", 131, "X"),
    ("empty.png", 132, " "),
    ("1.png", 133, "1"),
    ("2.png", 134, "2"),
    ("3.png", 135, "3"),
    ("4.png", 136, "4"),
    ("5.png", 137, "5"),
    ("6.png", 138, "6"),
    ("7.png", 139, "7"),
    ("8.png", 140, "8"),
    ("face_normal.png", 141, "smile"),
    ("face_surprised.png", 142, "surprise"),
    ("face_dead.png", 143, "dead"),
    ("face_cool.png", 144, "cool"),
]

ROOT = Path(__file__).resolve().parent.parent
TILES_DIR = ROOT / "tiles"
OUTPUT_R = ROOT / "tiles_generated.r"


def pixel_is_foreground(label: str, x: int, y: int) -> bool:
    cx, cy = TILE_SIZE // 2, TILE_SIZE // 2
    dx = abs(x - cx)
    dy = abs(y - cy)

    if label == "?":
        return (4 <= dx <= 6 and 3 <= dy <= 10) or (dx <= 1 and dy == 11)
    if label == "F":
        return (7 <= x <= 12 and 3 <= y <= 13) or (4 <= x <= 6 and 5 <= y <= 8)
    if label == "*":
        return dx <= 1 or dy <= 1 or abs(dx - dy) <= 1
    if label == "X":
        return abs(x - y) <= 1 or abs(x + y - (TILE_SIZE - 1)) <= 1
    if label.isdigit():
        return dx <= 2 and dy <= 4
    if label == "smile":
        return (dx == 3 and dy in (5, 6)) or (dx == 1 and dy == 8)
    if label == "surprise":
        return (dx == 3 and dy in (5, 6)) or (dx <= 1 and dy == 9)
    if label == "dead":
        return (x in (4, 5, 10, 11) and y in (5, 6)) or (dx <= 1 and dy == 9)
    if label == "cool":
        return (dx == 3 and dy in (5, 6)) or (3 <= x <= 11 and y == 9)
    return False


def make_mono_tile(label: str) -> list[list[int]]:
    mono: list[list[int]] = []
    for y in range(TILE_SIZE):
        row: list[int] = []
        for x in range(TILE_SIZE):
            border = x == 0 or y == 0 or x == TILE_SIZE - 1 or y == TILE_SIZE - 1
            if border:
                row.append(BLACK)
            elif label != " " and pixel_is_foreground(label, x, y):
                row.append(BLACK)
            else:
                row.append(WHITE)
        mono.append(row)
    return mono


def png_chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def mono_to_index(mono: list[list[int]]) -> list[list[int]]:
    """Map mono tile to palette indices: 0=black, 1=white."""
    return [[0 if bit == BLACK else 1 for bit in row] for row in mono]


def write_png_indexed(path: Path, mono: list[list[int]]) -> None:
    indexed = mono_to_index(mono)
    raw = bytearray()
    for row in indexed:
        raw.append(0)
        raw.extend(row)

    compressed = zlib.compress(bytes(raw), 9)
    png = b"\x89PNG\r\n\x1a\n"
    png += png_chunk(b"IHDR", struct.pack(">IIBBBBB", TILE_SIZE, TILE_SIZE, 8, 3, 0, 0, 0))
    png += png_chunk(b"PLTE", PALETTE)
    png += png_chunk(b"IDAT", compressed)
    png += png_chunk(b"IEND", b"")
    path.write_bytes(png)


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
        return rows

    if color_type == 0 and bit_depth == 1:
        row_bytes = (width + 7) // 8
        for _ in range(height):
            offset += 1
            row = []
            for x in range(width):
                byte = inflated[offset + (x // 8)]
                bit = (byte >> (7 - (x % 8))) & 1
                row.append(BLACK if bit else WHITE)
            offset += row_bytes
            rows.append(row)
        return rows

    if color_type in (2, 6):
        prev = [0] * (width * (4 if color_type == 6 else 3))
        sample_size = 4 if color_type == 6 else 3
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
        return rows

    if color_type == 0 and bit_depth == 8:
        prev = [0] * width
        for _ in range(height):
            filter_type = inflated[offset]
            offset += 1
            row_data = list(inflated[offset : offset + width])
            offset += width
            row_data = apply_scanline_filter(filter_type, row_data, prev)
            prev = row_data
            rows.append([WHITE if v == 255 else BLACK for v in row_data])
        return rows

    raise ValueError(
        f"Unsupported PNG format in {path} "
        f"(type={color_type}, depth={bit_depth}; use indexed 8-bit B&W)"
    )


def pack_row_bits(row: list[int]) -> bytes:
    out = bytearray(2)
    for x, bit in enumerate(row):
        if bit:
            out[x // 8] |= 0x80 >> (x % 8)
    return bytes(out)


def mono_to_mac_bitmap(mono: list[list[int]]) -> tuple[bytes, bytes]:
    image = bytearray()
    mask = bytearray()
    for row in mono:
        image.extend(pack_row_bits(row))
        mask.extend(pack_row_bits([BLACK if v == WHITE else WHITE for v in row]))
    return bytes(image), bytes(mask)


def format_rez_data(resource_id: int, image: bytes, mask: bytes) -> str:
    combined = image + mask
    hex_pairs = " ".join(f"{b:02X}" for b in combined)
    return f"data 'TILE' ({resource_id}) {{\n    $\"{hex_pairs}\"\n}};\n"


def ensure_placeholders() -> None:
    TILES_DIR.mkdir(exist_ok=True)
    for filename, _, label in TILES:
        path = TILES_DIR / filename
        if not path.exists():
            write_png_indexed(path, make_mono_tile(label))


def main() -> None:
    ensure_placeholders()

    parts = ["/* Auto-generated by tools/generate_tiles.py. Do not edit. */\n"]
    for filename, resource_id, _label in TILES:
        path = TILES_DIR / filename
        rows = read_png_mono(path)
        if len(rows) != TILE_SIZE or len(rows[0]) != TILE_SIZE:
            raise ValueError(f"{filename} must be {TILE_SIZE}x{TILE_SIZE}")
        image, mask = mono_to_mac_bitmap(rows)
        parts.append(format_rez_data(resource_id, image, mask))

    OUTPUT_R.write_text("".join(parts))
    print(f"Wrote {len(TILES)} indexed tile resources to {OUTPUT_R}")


if __name__ == "__main__":
    main()
