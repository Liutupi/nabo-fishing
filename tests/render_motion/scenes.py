"""Lay the raw stills from render_scenes out on one PNG sheet.

Usage: python scenes.py <directory with .rgb565 stills> <output.png>
"""
import re
import sys
from pathlib import Path

from PIL import Image

stills = []
for path in sorted(Path(sys.argv[1]).glob("*.rgb565")):
    width, height = map(int, re.search(r"-(\d+)x(\d+)\.rgb565$", path.name).groups())
    stills.append(Image.frombytes("RGB", (width, height), path.read_bytes(), "raw", "BGR;16"))
if not stills:
    raise SystemExit("no stills found")
columns = 5
rows = -(-len(stills) // columns)
sheet = Image.new("RGB", (columns * 330, rows * 330), "#e1eee9")
for i, still in enumerate(stills):
    sheet.paste(still, (i % columns * 330 + (330 - still.width) // 2,
                        i // columns * 330 + (330 - still.height) // 2))
sheet.save(sys.argv[2])
print(f"{len(stills)} stills")
