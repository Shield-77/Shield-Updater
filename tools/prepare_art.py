"""Prepare artwork with Pillow: python tools/prepare_art.py input.png.

The source image is kept in assets/background-original.png. Runtime BGR data
matches the 3DS top-screen framebuffer's column-major, bottom-up layout.
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import shutil
import sys

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1])
(root / "assets").mkdir(exist_ok=True)
(root / "romfs").mkdir(exist_ok=True)
shutil.copy2(source, root / "assets" / "background-original.png")
image = Image.open(source).convert("RGB").resize((400, 240), Image.Resampling.LANCZOS)
raw = bytearray(400 * 240 * 3)
for x in range(400):
    for y in range(240):
        r, g, b = image.getpixel((x, y))
        offset = (x * 240 + (239 - y)) * 3
        raw[offset:offset + 3] = bytes((b, g, r))
(root / "romfs" / "background.bgr").write_bytes(raw)
image.crop((152, 20, 248, 116)).resize((48, 48), Image.Resampling.LANCZOS).save(root / "icon.png")
image.resize((256, 128), Image.Resampling.LANCZOS).save(root / "banner.png")

font_file = root / "assets" / "AtkinsonHyperlegible-Regular.ttf"
if not font_file.exists():
    raise SystemExit(f"Missing font source: {font_file}")
font = ImageFont.truetype(str(font_file), 14)
atlas = bytearray()
for codepoint in range(32, 127):
    cell = Image.new("L", (10, 16), 0)
    draw = ImageDraw.Draw(cell)
    draw.text((0, -2), chr(codepoint), font=font, fill=255)
    atlas.extend(cell.tobytes())
(root / "romfs" / "font.bin").write_bytes(atlas)
