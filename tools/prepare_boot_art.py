"""Build the public, Callie-free lower boot background from top-screen pixels."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
top = (root / "romfs" / "background.bgr").read_bytes()
column_bytes = 240 * 3
if len(top) != 400 * column_bytes:
    raise ValueError("Expected a 400x240 BGR top-screen asset")
(root / "romfs" / "boot-bottom.bgr").write_bytes(
    top[40 * column_bytes : 360 * column_bytes]
)
