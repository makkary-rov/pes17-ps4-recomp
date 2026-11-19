# SPDX-License-Identifier: GPL-2.0-or-later
"""Draws the settings window's gamepad picture and button icons (launcher/assets).

Every pixel is drawn by this script: a generic gamepad outline and small labelled chips. Nothing is
copied from a photo, a screenshot or any product artwork, and no console maker's colours or logos
are used. Labels use DejaVu Sans (gpu/third_party/fonts, see LICENSE-DejaVu.txt).

Run from the repository root (needs Pillow, a developer tool only):
    python tools/art/make_settings_art.py
Writes launcher/assets/controller_pad.bmp (260 x 325) and icon_00.bmp .. icon_23.bmp (40 x 26),
all 24-bit, on the settings page colour (240, 240, 240).
"""
import math
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

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


# ---- gamepad picture (260 x 325) ------------------------------------------------------------
def gamepad():
    W, H = 260 * SS, 325 * SS
    im = Image.new("RGB", (W, H), PAGE)

    def S(*v):
        return [x * SS for x in v]

    # body: a wide centre with two grips splayed outward
    body = Image.new("L", (W, H), 0)
    ImageDraw.Draw(body).rounded_rectangle(S(24, 98, 236, 206), radius=44 * SS, fill=255)
    grip = Image.new("L", (110 * SS, 164 * SS), 0)
    ImageDraw.Draw(grip).rounded_rectangle(S(10, 4, 100, 158), radius=44 * SS, fill=255)
    for angle, cx in ((-11, 66), (11, 194)):
        g = grip.rotate(angle, expand=True, resample=Image.BICUBIC)
        layer = Image.new("L", (W, H), 0)
        layer.paste(g, (int(cx * SS - g.width / 2), 140 * SS))
        body = ImageChops.lighter(body, layer)
    # triggers and bumpers: stacked pieces joined to the top of the body
    parts = Image.new("L", (W, H), 0)
    p = ImageDraw.Draw(parts)
    for x in (34, 152):
        p.rounded_rectangle(S(x + 14, 30, x + 62, 84), radius=20 * SS, fill=255)  # trigger
        p.rounded_rectangle(S(x + 4, 74, x + 72, 104), radius=14 * SS, fill=255)  # bumper

    dark, mid, light = (44, 48, 55), (64, 70, 80), (96, 103, 116)
    d = ImageDraw.Draw(im)
    im.paste(light, mask=parts)
    im.paste(mid, mask=parts.filter(ImageFilter.MinFilter(2 * SS + 1)))
    im.paste(light, mask=body)
    im.paste(dark, mask=body.filter(ImageFilter.MinFilter(2 * SS + 1)))
    d = ImageDraw.Draw(im, "RGBA")
    d.ellipse(S(40, 100, 220, 170), fill=(255, 255, 255, 14))  # top sheen

    # centre pad and two small buttons
    d.rounded_rectangle(S(100, 106, 160, 154), radius=10 * SS, fill=(30, 33, 38), outline=(84, 90, 100), width=SS)
    for x in (82, 172):
        d.rounded_rectangle(S(x, 110, x + 6, 130), radius=3 * SS, fill=(30, 33, 38), outline=(84, 90, 100), width=SS)
    # d-pad
    cx, cy = 54 * SS, 138 * SS
    d.rounded_rectangle((cx - 8 * SS, cy - 25 * SS, cx + 8 * SS, cy + 25 * SS), radius=4 * SS, fill=(26, 29, 34), outline=(84, 90, 100), width=SS)
    d.rounded_rectangle((cx - 25 * SS, cy - 8 * SS, cx + 25 * SS, cy + 8 * SS), radius=4 * SS, fill=(26, 29, 34), outline=(84, 90, 100), width=SS)
    d.rectangle((cx - 7 * SS, cy - 7 * SS, cx + 7 * SS, cy + 7 * SS), fill=(26, 29, 34))
    for dx, dy, pts in ((0, -17, [(0, -4), (4, 3), (-4, 3)]), (0, 17, [(0, 4), (4, -3), (-4, -3)]),
                        (-17, 0, [(-4, 0), (3, -4), (3, 4)]), (17, 0, [(4, 0), (-3, -4), (-3, 4)])):
        d.polygon([(cx + (dx + x) * SS, cy + (dy + y) * SS) for x, y in pts], fill=(150, 156, 168))
    # face buttons: four discs with plain light symbols (no colours)
    fx, fy, g = 206 * SS, 138 * SS, 19 * SS
    centres = {"tri": (fx, fy - g), "cross": (fx, fy + g), "sq": (fx - g, fy), "circ": (fx + g, fy)}
    for key, (x, y) in centres.items():
        d.ellipse((x - 12 * SS, y - 12 * SS, x + 12 * SS, y + 12 * SS), fill=(26, 29, 34), outline=(84, 90, 100), width=SS)
        sym, r, w = (170, 176, 188), 5 * SS, int(1.8 * SS)
        if key == "tri":
            d.polygon([(x, y - r - SS), (x + r + SS, y + r - SS), (x - r - SS, y + r - SS)], outline=sym, width=w)
        elif key == "cross":
            d.line((x - r, y - r, x + r, y + r), fill=sym, width=w)
            d.line((x - r, y + r, x + r, y - r), fill=sym, width=w)
        elif key == "sq":
            d.rectangle((x - r, y - r, x + r, y + r), outline=sym, width=w)
        else:
            d.ellipse((x - r, y - r, x + r, y + r), outline=sym, width=w)
    # sticks
    for x in (96, 164):
        d.ellipse((x * SS - 30 * SS, 224 * SS - 30 * SS, x * SS + 30 * SS, 224 * SS + 30 * SS), fill=(24, 27, 32), outline=(84, 90, 100), width=SS)
        d.ellipse((x * SS - 21 * SS, 224 * SS - 21 * SS, x * SS + 21 * SS, 224 * SS + 21 * SS), fill=(52, 57, 66), outline=(110, 116, 128), width=SS)
        d.ellipse((x * SS - 12 * SS, 224 * SS - 12 * SS, x * SS + 12 * SS, 224 * SS + 12 * SS), outline=(76, 82, 94), width=SS)
    return im.resize((260, 325), Image.LANCZOS).convert("RGB")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for i, chip in enumerate(ICONS):
        finish(chip, "icon_%02d.bmp" % i)
    gamepad().save(OUT / "controller_pad.bmp", format="BMP")
    print("wrote controller_pad.bmp and icon_00..icon_23.bmp to", OUT)


if __name__ == "__main__":
    main()
