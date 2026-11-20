# SPDX-License-Identifier: GPL-2.0-or-later
"""Draws docs/header.png, the README banner: a lengthwise strip of the settings background
(launcher/assets/background.bmp, an AI-generated image supplied by the project owner), darkened a little,
with one line of text in Microsoft Sans Serif (the face behind "MS Shell Dlg", which the settings window uses).
The font comes from Windows and is not shipped; only the finished picture is.
Run: python tools/art/make_header.py"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
W, H = 1600, 300
TEXT = "The definitive way to play it on PC!"


def main():
    bg = Image.open(ROOT / "launcher/assets/background.bmp").convert("RGB")
    big = bg.resize((W, round(bg.height * W / bg.width)), Image.LANCZOS)
    top = (big.height - H) // 2
    strip = big.crop((0, top, W, top + H))
    strip = Image.blend(strip, Image.new("RGB", (W, H), (0, 0, 0)), 0.45)
    font = ImageFont.truetype("C:/Windows/Fonts/micross.ttf", 84)
    box = ImageDraw.Draw(strip).textbbox((0, 0), TEXT, font=font)
    x, y = (W - (box[2] - box[0])) // 2 - box[0], (H - (box[3] - box[1])) // 2 - box[1]
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((x + 3, y + 4), TEXT, font=font, fill=(0, 0, 0, 220))
    strip = Image.alpha_composite(strip.convert("RGBA"), shadow.filter(ImageFilter.GaussianBlur(4))).convert("RGB")
    ImageDraw.Draw(strip).text((x, y), TEXT, font=font, fill=(255, 255, 255))
    out = ROOT / "docs" / "header.png"
    strip.save(out, optimize=True)
    print(f"wrote {out} ({out.stat().st_size // 1024} KiB)")


main()
