#!/usr/bin/env python3
# Render a line of text in a built-in 5x7 pixel font to a PCX image for a
# sprite asset folder. Magenta is the transparent background (pen 0 after
# convert_folder_to_shared_palette.py), with a one-pixel drop shadow.
#
# Usage: make_text_sprite.py "PRESS START" out.pcx [--scale 2] [--color FFFFFF] [--shadow 404040]
import argparse
from PIL import Image

FONT = {
    'A': ".###. #...# #...# ##### #...# #...# #...#",
    'B': "####. #...# #...# ####. #...# #...# ####.",
    'C': ".###. #...# #.... #.... #.... #...# .###.",
    'D': "####. #...# #...# #...# #...# #...# ####.",
    'E': "##### #.... #.... ####. #.... #.... #####",
    'F': "##### #.... #.... ####. #.... #.... #....",
    'G': ".###. #...# #.... #.### #...# #...# .####",
    'H': "#...# #...# #...# ##### #...# #...# #...#",
    'I': ".###. ..#.. ..#.. ..#.. ..#.. ..#.. .###.",
    'J': "..### ...#. ...#. ...#. ...#. #..#. .##..",
    'K': "#...# #..#. #.#.. ##... #.#.. #..#. #...#",
    'L': "#.... #.... #.... #.... #.... #.... #####",
    'M': "#...# ##.## #.#.# #.#.# #...# #...# #...#",
    'N': "#...# #...# ##..# #.#.# #..## #...# #...#",
    'O': ".###. #...# #...# #...# #...# #...# .###.",
    'P': "####. #...# #...# ####. #.... #.... #....",
    'Q': ".###. #...# #...# #...# #.#.# #..#. .##.#",
    'R': "####. #...# #...# ####. #.#.. #..#. #...#",
    'S': ".#### #.... #.... .###. ....# ....# ####.",
    'T': "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..",
    'U': "#...# #...# #...# #...# #...# #...# .###.",
    'V': "#...# #...# #...# #...# #...# .#.#. ..#..",
    'W': "#...# #...# #...# #.#.# #.#.# #.#.# .#.#.",
    'X': "#...# #...# .#.#. ..#.. .#.#. #...# #...#",
    'Y': "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#..",
    'Z': "##### ....# ...#. ..#.. .#... #.... #####",
    '0': ".###. #...# #..## #.#.# ##..# #...# .###.",
    '1': "..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.",
    '2': ".###. #...# ....# ...#. ..#.. .#... #####",
    '3': "####. ....# ....# .###. ....# ....# ####.",
    '4': "...#. ..##. .#.#. #..#. ##### ...#. ...#.",
    '5': "##### #.... ####. ....# ....# #...# .###.",
    '6': ".###. #.... #.... ####. #...# #...# .###.",
    '7': "##### ....# ...#. ..#.. .#... .#... .#...",
    '8': ".###. #...# #...# .###. #...# #...# .###.",
    '9': ".###. #...# #...# .#### ....# ....# .###.",
    '!': "..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#..",
    ' ': "..... ..... ..... ..... ..... ..... .....",
}
GLYPH_W = 5
GLYPH_H = 7
MAGENTA = (255, 0, 255)


def parse_color(text):
    return tuple(int(text[i:i + 2], 16) for i in (0, 2, 4))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("text")
    parser.add_argument("output")
    parser.add_argument("--scale", type=int, default=1)
    parser.add_argument("--color", default="FFFFFF")
    parser.add_argument("--shadow", default="404040")
    args = parser.parse_args()

    text = args.text.upper()
    for ch in text:
        if ch not in FONT:
            raise SystemExit("make_text_sprite.py: no glyph for '{}'".format(ch))

    scale = args.scale
    advance = (GLYPH_W + 1) * scale
    width = len(text) * advance - scale + 1
    height = GLYPH_H * scale + 1
    width += width & 1

    image = Image.new("RGB", (width, height), MAGENTA)
    color = parse_color(args.color)
    shadow = parse_color(args.shadow)
    for layer, fill in ((1, shadow), (0, color)):
        for i, ch in enumerate(text):
            rows = FONT[ch].split()
            for gy, row in enumerate(rows):
                for gx, bit in enumerate(row):
                    if bit != '#':
                        continue
                    for sy in range(scale):
                        for sx in range(scale):
                            x = i * advance + gx * scale + sx + layer
                            y = gy * scale + sy + layer
                            image.putpixel((x, y), fill)
    image.save(args.output)


if __name__ == "__main__":
    main()
