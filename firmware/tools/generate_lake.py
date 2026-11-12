from pathlib import Path
from PIL import Image, ImageChops, ImageOps

root = Path(__file__).resolve().parents[2]

cards = []
for sheet in range(1, 8):
    atlas = Image.open(root / f"art/fish-{sheet:02}.png").convert("RGB")
    columns = 2 if sheet == 7 else 4
    for row in range(columns):
        for col in range(columns):
            cell = atlas.crop((col * atlas.width // columns, row * atlas.height // columns,
                               (col + 1) * atlas.width // columns, (row + 1) * atlas.height // columns))
            bg = cell.getpixel((1, 1))
            diff = ImageChops.difference(cell, Image.new("RGB", cell.size, bg))
            bounds = diff.convert("L").point(lambda v: 255 if v > 24 else 0).getbbox()
            if not bounds:
                raise ValueError(f"empty fish in sheet {sheet}, row {row}, column {col}")
            left, top, right, bottom = bounds
            cell = cell.crop((max(0, left - 5), max(0, top - 5),
                              min(cell.width, right + 5), min(cell.height, bottom + 5)))
            card = ImageOps.pad(cell, (84, 60), Image.Resampling.LANCZOS, color=bg)
            cards.append(card)

assert len(cards) == 100
preview = Image.new("RGB", (840, 600), "#10343b")
for i, card in enumerate(cards):
    preview.paste(card, (i % 10 * 84, i // 10 * 60))
preview.save(root / "art/fish-catalog-preview.png")
