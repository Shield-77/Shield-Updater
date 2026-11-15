"""Pack the 48x48 release icons into an offline RGB asset."""

from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
names = ("lite", "standard", "mp4", "photo", "online-art", "converter", "downloader", "radio", "theme-music", "checker", "theme-maker")
data = bytearray()
for name in names:
    image = Image.open(root / "assets" / f"icon-{name}.png").convert("RGB")
    if image.size != (48, 48):
        raise SystemExit(f"Wrong size for {name}: {image.size}")
    data.extend(image.tobytes())
(root / "romfs" / "app-icons.rgb").write_bytes(data)
