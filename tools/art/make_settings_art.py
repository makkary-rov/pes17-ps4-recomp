# SPDX-License-Identifier: GPL-2.0-or-later
"""Draws the settings window's button icons (launcher/assets/icon_00.bmp .. icon_23.bmp).

Every pixel is drawn by this script: small labelled chips. Nothing is copied from a photo, a
screenshot or any product artwork, and no console maker's colours or logos are used. Labels use
DejaVu Sans (gpu/third_party/fonts, see LICENSE-DejaVu.txt). The gamepad picture comes from
tools/art/cut_gamepad.py.

Run from the repository root (needs Pillow, a developer tool only):
    python tools/art/make_settings_art.py
Writes 24 icons (40 x 26, 24-bit) on the settings page colour (240, 240, 240).
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "launcher" / "assets"
FONT = ROOT / "gpu" / "third_party" / "fonts" / "DejaVuSans.ttf"
PAGE = (240, 240, 240)
SS = 4  # supersampling factor for smooth edges


def font(px):
    return ImageFont.truetype(str(FONT), int(px * SS))


def text_center(d, cx, cy, text, px, fill):
    f = font(px)
    # a small outline thickens DejaVu Sans into a bold look
    d.text((cx * SS, cy * SS), text, font=f, fill=fill, anchor="mm", stroke_width=SS // 2, stroke_fill=fill)


# ---- icon chips (40 x 26) -------------------------------------------------------------------
CHIP_FILL, CHIP_EDGE, GLYPH, DIM = (38, 40, 44), (96, 99, 106), (244, 244, 244), (104, 106, 112)


def new_chip():
    im = Image.new("RGB", (40 * SS, 26 * SS), PAGE)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((1 * SS, 1 * SS, 39 * SS, 25 * SS), radius=6 * SS, fill=CHIP_FILL, outline=CHIP_EDGE, width=SS)
    return im, d


def finish(im, name):
    im.resize((40, 26), Image.LANCZOS).convert("RGB").save(OUT / name, format="BMP")


def chip_shape(kind):
    im, d = new_chip()
    cx, cy, r, w = 20 * SS, 13 * SS, 7.5 * SS, int(2.2 * SS)
    if kind == "square":
        d.rectangle((cx - r, cy - r, cx + r, cy + r), outline=GLYPH, width=w)
    elif kind == "cross":
        d.line((cx - r, cy - r, cx + r, cy + r), fill=GLYPH, width=w)
        d.line((cx - r, cy + r, cx + r, cy - r), fill=GLYPH, width=w)
    elif kind == "circle":
        d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=GLYPH, width=w)
    else:  # triangle
        d.polygon([(cx, cy - r - 0.5 * SS), (cx + r + SS, cy + r - SS), (cx - r - SS, cy + r - SS)], outline=GLYPH, width=w)
    return im


def chip_text(label, px=13):
    im, d = new_chip()
    text_center(d, 20, 13.5, label, px, GLYPH)
    return im


def chip_dpad(direction):
    im, d = new_chip()
    cx, cy, a, t = 20 * SS, 13 * SS, 9 * SS, 3.2 * SS
    arms = {"up": (cx - t, cy - a, cx + t, cy), "down": (cx - t, cy, cx + t, cy + a),
            "left": (cx - a, cy - t, cx, cy + t), "right": (cx, cy - t, cx + a, cy + t)}
    for name, box in arms.items():
        d.rectangle(box, fill=GLYPH if name == direction else DIM)
    d.rectangle((cx - t, cy - t, cx + t, cy + t), fill=DIM)
    return im


def chip_stick(letter, direction):
    im, d = new_chip()
    text_center(d, 11, 13.5, letter, 12, GLYPH)
    cx, cy, s = 27 * SS, 13 * SS, 6.5 * SS
    tip = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}[direction]
    px_, py_ = -tip[1], tip[0]
    pts = [(cx + tip[0] * s, cy + tip[1] * s),
           (cx - tip[0] * s * 0.7 + px_ * s, cy - tip[1] * s * 0.7 + py_ * s),
           (cx - tip[0] * s * 0.7 - px_ * s, cy - tip[1] * s * 0.7 - py_ * s)]
    d.polygon(pts, fill=GLYPH)
    return im


ICONS = [None] * 24
ICONS[0], ICONS[1], ICONS[2], ICONS[3] = (chip_shape("square"), chip_shape("cross"), chip_shape("circle"), chip_shape("triangle"))
ICONS[4], ICONS[5], ICONS[6], ICONS[7] = chip_text("L2"), chip_text("R2"), chip_text("L1"), chip_text("R1")
ICONS[8], ICONS[9], ICONS[10], ICONS[11] = chip_text("OPT", 11), chip_text("PAD", 11), chip_text("L3"), chip_text("R3")
for i, direction in zip(range(12, 16), ("up", "down", "left", "right")):
    ICONS[i] = chip_dpad(direction)
for i, direction in zip(range(16, 20), ("up", "down", "left", "right")):
    ICONS[i] = chip_stick("L", direction)
for i, direction in zip(range(20, 24), ("up", "down", "left", "right")):
    ICONS[i] = chip_stick("R", direction)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for i, chip in enumerate(ICONS):
        finish(chip, "icon_%02d.bmp" % i)
    print("wrote icon_00..icon_23.bmp to", OUT)


if __name__ == "__main__":
    main()
