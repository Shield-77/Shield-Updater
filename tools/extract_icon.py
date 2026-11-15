"""Extract a CIA's 48x48 SMDH icon to PNG for offline UI assets.

Usage: python tools/extract_icon.py app.cia assets/icon-name.png
Only unencrypted CIA files with an embedded SMDH are supported.
"""

from pathlib import Path
from PIL import Image
import sys


def morton(x: int, y: int) -> int:
    index = 0
    for bit in range(3):
        index |= ((x >> bit) & 1) << (2 * bit)
        index |= ((y >> bit) & 1) << (2 * bit + 1)
    return index


def main() -> None:
    source = Path(sys.argv[1]).read_bytes()
    smdh = source.find(b"SMDH")
    if smdh < 0 or smdh + 0x36C0 > len(source):
        raise SystemExit("Embedded SMDH icon not found")
    pixels = source[smdh + 0x24C0 : smdh + 0x36C0]
    if len(pixels) != 48 * 48 * 2:
        raise SystemExit("Incomplete SMDH icon")
    image = Image.new("RGB", (48, 48))
    for y in range(48):
        for x in range(48):
            tile = (y // 8) * 6 + (x // 8)
            offset = (tile * 64 + morton(x % 8, y % 8)) * 2
            value = int.from_bytes(pixels[offset : offset + 2], "little")
            r = ((value >> 11) & 31) * 255 // 31
            g = ((value >> 5) & 63) * 255 // 63
            b = (value & 31) * 255 // 31
            image.putpixel((x, y), (r, g, b))
    image.save(sys.argv[2])


if __name__ == "__main__":
    main()
