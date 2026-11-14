"""Refresh the ornament contact sheet from the ten finished sprites."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CELL_W, ROW_H = 52, 64


def main():
    art = ROOT / "art"
    directory = art / "decor"
    preview = Image.new("RGB", (10 * CELL_W, 4 * ROW_H))
    for row in range(4):
        with Image.open(art / "tanks" / f"tank-{row}.png") as tank:
            backdrop = tank.crop((0, 129, 320, 193)).resize((preview.width, ROW_H))
        preview.paste(backdrop, (0, row * ROW_H))
    for index in range(10):
        with Image.open(directory / f"decor-{index}.png") as image:
            alpha = image.getchannel("A")
            bounds = alpha.getbbox()
            if max(image.size) > 48 or any(alpha.histogram()[1:255]) or not bounds:
                raise ValueError(f"invalid ornament sprite {index}")
            if (bounds[3] < image.height) != (index == 8):
                raise ValueError(f"invalid ornament sprite {index}")
            for row in range(4):
                base = (row + 1) * ROW_H - (14 if index == 8 else 2)
                x = index * CELL_W + (CELL_W - image.width) // 2
                preview.paste(image, (x, base - image.height), alpha)
    preview.save(directory / "decor-preview.png")


if __name__ == "__main__":
    main()
