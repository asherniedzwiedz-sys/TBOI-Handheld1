#!/usr/bin/env python3
"""Build the 8 KB CHR-ROM and tile index header from ASCII art.

Usage: mkchr.py gfx/tiles.txt build/chr.bin build/tiles.h

Art file format:
  [bg] / [spr]      select pattern table ($0000 background, $1000 sprites)
  @XX               next tile index (hex) in the current table
  == NAME WxH       an art block, W and H multiples of 8, followed by H rows
                    of W characters: '.'/'0' = colour 0, '1' '2' '3' = colours.
                    Tiles are stored row-major (16x16 -> TL, TR, BL, BR).
  # ...             comment

The 5x7 text font is placed at ASCII positions in the background table and a
2x "big" font for the title logo is placed at @A0. Both fonts are drawn here.
"""
import sys

FONT = {
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    '1': ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    '2': [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    '3': ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    '4': ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    '5': ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    '6': ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    'D': ["###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    'J': ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    '!': ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
    '-': [".....", ".....", ".....", "#####", ".....", ".....", "....."],
    '.': [".....", ".....", ".....", ".....", ".....", ".##..", ".##.."],
    ':': [".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."],
    '/': ["....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...."],
    '+': [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."],
    '?': [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    ',': [".....", ".....", ".....", ".....", ".##..", "..#..", ".#..."],
    "'": ["..#..", "..#..", ".#...", ".....", ".....", ".....", "....."],
}

BIG_LETTERS = "UNDERCOFT"   # letters used by the title logo
BIG_BASE = 0xA0
FONT_COLOR = 2


def blank_tile():
    return [[0] * 8 for _ in range(8)]


def encode(tile):
    lo, hi = [], []
    for row in tile:
        a = b = 0
        for x, c in enumerate(row):
            if c & 1:
                a |= 0x80 >> x
            if c & 2:
                b |= 0x80 >> x
        lo.append(a)
        hi.append(b)
    return bytes(lo + hi)


def split(pixels, w, h):
    tiles = []
    for ty in range(h // 8):
        for tx in range(w // 8):
            tiles.append([row[tx * 8:tx * 8 + 8] for row in pixels[ty * 8:ty * 8 + 8]])
    return tiles


def main(src, chr_out, hdr_out):
    tables = {'bg': [None] * 256, 'spr': [None] * 256}
    owner = {'bg': {}, 'spr': {}}
    defines = []

    def put(table, idx, tile, name):
        if idx > 255:
            sys.exit(f"{name}: tile index overflow in {table}")
        if tables[table][idx] is not None:
            sys.exit(f"{name}: tile {idx:#04x} in {table} already used by {owner[table][idx]}")
        tables[table][idx] = tile
        owner[table][idx] = name

    # text font at ASCII positions (space = blank)
    put('bg', 0x00, blank_tile(), 'BLANK')
    put('bg', 0x20, blank_tile(), 'SPACE')
    for ch, rows in FONT.items():
        t = blank_tile()
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c == '#':
                    t[y][x + 1] = FONT_COLOR
        put('bg', ord(ch), t, 'FONT_' + ch)

    # 2x big font with a colour-1 drop shadow, 16x16 per letter
    for n, ch in enumerate(BIG_LETTERS):
        px = [[0] * 16 for _ in range(16)]
        for y, r in enumerate(FONT[ch]):
            for x, c in enumerate(r):
                if c != '#':
                    continue
                for dy in range(2):
                    for dx in range(2):
                        sx, sy = 3 + x * 2 + dx + 1, 1 + y * 2 + dy + 1
                        if px[sy][sx] == 0:
                            px[sy][sx] = 1
                for dy in range(2):
                    for dx in range(2):
                        px[1 + y * 2 + dy][3 + x * 2 + dx] = 2
        base = BIG_BASE + n * 4
        for i, t in enumerate(split(px, 16, 16)):
            put('bg', base + i, t, 'BIG_' + ch)
        defines.append((f"BIG_{ch}", base))

    table, idx = 'bg', 0x60
    lines = open(src).read().splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        i += 1
        if not line or line.startswith('#'):
            continue
        if line in ('[bg]', '[spr]'):
            table = line[1:-1]
            continue
        if line.startswith('@'):
            idx = int(line[1:], 16)
            continue
        if line.startswith('=='):
            parts = line[2:].split()
            name, dims = parts[0], parts[1]
            w, h = (int(v) for v in dims.lower().split('x'))
            if w % 8 or h % 8:
                sys.exit(f"{name}: size {w}x{h} is not a multiple of 8")
            rows = lines[i:i + h]
            i += h
            pixels = []
            for ry, r in enumerate(rows):
                r = r.rstrip()
                if len(r) != w:
                    sys.exit(f"{name}: row {ry} has width {len(r)}, expected {w}: '{r}'")
                row = []
                for c in r:
                    if c in '.0':
                        row.append(0)
                    elif c in '123':
                        row.append(int(c))
                    else:
                        sys.exit(f"{name}: bad pixel char '{c}' in row {ry}")
                pixels.append(row)
            if len(pixels) != h:
                sys.exit(f"{name}: expected {h} rows, got {len(pixels)}")
            prefix = 'T_' if table == 'bg' else 'S_'
            defines.append((prefix + name, idx))
            for t in split(pixels, w, h):
                put(table, idx, t, name)
                idx += 1
            continue
        sys.exit(f"unrecognised line {i}: {line}")

    data = bytearray()
    for table in ('bg', 'spr'):
        for t in tables[table]:
            data += encode(t if t is not None else blank_tile())
    assert len(data) == 8192
    open(chr_out, 'wb').write(data)

    with open(hdr_out, 'w') as f:
        f.write("/* Generated by tools/mkchr.py from gfx/tiles.txt. Do not edit. */\n")
        f.write("#ifndef TILES_H\n#define TILES_H\n")
        for name, v in defines:
            f.write(f"#define {name} 0x{v:02X}\n")
        f.write("#endif\n")
    used = {t: sum(1 for x in tables[t] if x is not None) for t in tables}
    print(f"chr: {used['bg']} bg tiles, {used['spr']} sprite tiles")


if __name__ == '__main__':
    main(*sys.argv[1:4])
