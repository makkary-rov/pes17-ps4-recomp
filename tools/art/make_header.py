# SPDX-License-Identifier: GPL-2.0-or-later
"""Draws docs/header.png, the README banner: a lengthwise strip of the settings background
(launcher/assets/background.bmp, an AI-generated image supplied by the project owner), darkened a little,
with one line of text in Bahnschrift Bold, widened and thickened a little to match the "SETTINGS" lettering of
the settings background. The font comes from Windows and is not shipped; only the finished picture is.
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
    font = ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", 70)
    font.set_variation_by_name("Bold")
    # Draw the line on its own layer, widen it, then place it in the middle of the strip.
    box = font.getbbox(TEXT, stroke_width=2)
    layer = Image.new("L", (box[2] + 8, box[3] + 8), 0)
    ImageDraw.Draw(layer).text((2, 2), TEXT, font=font, fill=255, stroke_width=2, stroke_fill=255)
    layer = layer.crop(layer.getbbox())
    layer = layer.resize((round(layer.width * 1.22), layer.height), Image.LANCZOS)
    x, y = (W - layer.width) // 2, (H - layer.height) // 2
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 230), (x + 3, y + 4), layer)
    strip = Image.alpha_composite(strip.convert("RGBA"), shadow.filter(ImageFilter.GaussianBlur(4))).convert("RGB")
    strip.paste((255, 255, 255), (x, y), layer)
    out = ROOT / "docs" / "header.png"
    strip.save(out, optimize=True)
    print(f"wrote {out} ({out.stat().st_size // 1024} KiB)")


main()
