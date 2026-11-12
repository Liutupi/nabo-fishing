"""Export art/fish-catalog-alpha.png: the 100 fish cards with a transparent backdrop.

Uses the same connected-backdrop flood fill as generate_motion.py, so the
browser tank shows the silhouettes the firmware sprites have.
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CARD_W, CARD_H = 84, 60

def cutout(card):
    w, h = card.size
    px = list(card.getdata())
    base = tuple(int(sum(px[i][c] for i in (0, w - 1, (h - 1) * w, h * w - 1)) / 4)
                 for c in range(3))
    exterior = set()
    pending = list(range(w)) + list(range((h - 1) * w, h * w))
    pending += [y * w + x for y in range(h) for x in (0, w - 1)]
    while pending:
        i = pending.pop()
        if i in exterior or sum(abs(a - b) for a, b in zip(px[i], base)) > 42:
            continue
        exterior.add(i)
        x, y = i % w, i // w
        if x > 0: pending.append(i - 1)
        if x < w - 1: pending.append(i + 1)
        if y > 0: pending.append(i - w)
        if y < h - 1: pending.append(i + w)
    result = card.convert("RGBA")
    result.putalpha(Image.frombytes("L", card.size,
                                    bytes(0 if i in exterior else 255 for i in range(w * h))))
    return result

atlas = Image.open(ROOT / "art/fish-catalog-preview.png").convert("RGB")
sheet = Image.new("RGBA", atlas.size)
for i in range(100):
    x, y = i % 10 * CARD_W, i // 10 * CARD_H
    sheet.paste(cutout(atlas.crop((x, y, x + CARD_W, y + CARD_H))), (x, y))
sheet.save(ROOT / "art/fish-catalog-alpha.png")
