# SPDX-License-Identifier: GPL-2.0-or-later
"""Draws docs/header.png, the README banner: a lengthwise strip of the settings background
(launcher/assets/background.bmp, an AI-generated image supplied by the project owner), darkened,
with the "2017RECOMP" wordmark (Arial Black, as in the launcher) and the tagline (Segoe UI).
The two fonts come from Windows and are not shipped; only the finished picture is.
Run: python tools/art/make_header.py"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
W, H = 1600, 400
FONTS = Path("C:/Windows/Fonts")
RED, WHITE = (214, 36, 40), (255, 255, 255)


def main():
    bg = Image.open(ROOT / "launcher/assets/background.bmp").convert("RGB")
    # A wide strip: the full width of the picture, scaled to the banner width, cut around its middle.
    scale = W / bg.width
    big = bg.resize((W, round(bg.height * scale)), Image.LANCZOS)
    top = (big.height - H) // 2
    strip = big.crop((0, top, W, top + H))
    shade = Image.new("RGB", (W, H), (0, 0, 0))
    mask = Image.linear_gradient("L").rotate(90).resize((W, H))  # darker on the left, where the text is
    strip = Image.composite(shade, strip, mask.point(lambda v: 170 - v // 3))
    title_font = ImageFont.truetype(str(FONTS / "ariblk.ttf"), 128)
    sub_font = ImageFont.truetype(str(FONTS / "segoeuisl.ttf"), 44)
    x, y = 90, 100
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow)
    sdraw.text((x + 4, y + 5), "2017RECOMP", font=title_font, fill=(0, 0, 0, 200))
    sdraw.text((x + 3, y + 205), "The definitive way to play it on PC!", font=sub_font, fill=(0, 0, 0, 200))
    strip = Image.alpha_composite(strip.convert("RGBA"), shadow.filter(ImageFilter.GaussianBlur(5))).convert("RGB")
    draw = ImageDraw.Draw(strip)
    draw.text((x, y), "2017", font=title_font, fill=WHITE)
    draw.text((x + draw.textlength("2017", font=title_font), y), "RECOMP", font=title_font, fill=RED)
    draw.text((x + 2, y + 200), "The definitive way to play it on PC!", font=sub_font, fill=WHITE)
    draw.rectangle((x + 2, y + 178, x + 2 + 220, y + 182), fill=RED)
    out = ROOT / "docs" / "header.png"
    out.parent.mkdir(exist_ok=True)
    strip.save(out, optimize=True)
    print(f"wrote {out} ({out.stat().st_size // 1024} KiB)")


main()
