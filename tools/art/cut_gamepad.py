# SPDX-License-Identifier: GPL-2.0-or-later
"""Cuts the gamepad out of the picture supplied by the project owner and writes
launcher/assets/controller_pad.bmp (260 x 325, 24-bit, on the settings page colour).

The source picture is a controller on a baked-in light checkerboard (fake transparency). The
checkerboard is two flat light grey values; the controller is dark, so the background is found by a
flood fill from the corners over the light pixels, the mask is shrunk a little to drop the blended
edge pixels, softened, and the controller is placed on the page colour.

Run from the repository root (needs Pillow, a developer tool only):
    python tools/art/cut_gamepad.py <path to the supplied picture>
The source picture itself is not stored in the repository.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

PAGE = (240, 240, 240)
OUT = Path(__file__).resolve().parents[2] / "launcher" / "assets" / "controller_pad.bmp"
LIGHT = 172  # above this a pixel is checkerboard, not controller


def main(src):
    im = Image.open(src).convert("RGB")
    light = im.convert("L").point(lambda v: 255 if v > LIGHT else 0)
    # flood the connected light region that touches the border: the checkerboard
    for corner in ((0, 0), (im.width - 1, 0), (0, im.height - 1), (im.width - 1, im.height - 1)):
        if light.getpixel(corner) == 255:
            ImageDraw.floodfill(light, corner, 128)
    background = light.point(lambda v: 255 if v == 128 else 0)
    keep = background.point(lambda v: 0 if v else 255)            # controller (and anything enclosed by it)
    keep = keep.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.GaussianBlur(1.4))
    box = keep.point(lambda v: 255 if v > 8 else 0).getbbox()
    pad = Image.composite(im, Image.new("RGB", im.size, PAGE), keep).crop(box)
    w, h = 260, round(pad.height * 260 / pad.width)
    pad = pad.resize((w, h), Image.LANCZOS)
    canvas = Image.new("RGB", (260, 325), PAGE)
    canvas.paste(pad, (0, (325 - h) // 2))
    canvas.save(OUT, format="BMP")
    print("wrote", OUT, "controller", (w, h))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
