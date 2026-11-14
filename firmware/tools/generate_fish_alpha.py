"""Export art/fish-catalog-alpha.png: the fish cards with a transparent backdrop.

Uses the same connected-backdrop flood fill as generate_motion.py, so the
browser tank shows the silhouettes the firmware sprites have.
"""
from PIL import Image

from fish_assets import ART, append_from, atlas_size, card_box, species_names

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

count = len(species_names())
start = append_from(count)
atlas = Image.open(ART / "fish-catalog-preview.png").convert("RGB")
if atlas.size != atlas_size(count):
    raise ValueError("unexpected fish atlas size")
output = ART / "fish-catalog-alpha.png"
sheet = Image.new("RGBA", atlas.size)
if start:
    with Image.open(output) as previous:
        if previous.width != sheet.width or previous.height < atlas_size(start)[1]:
            raise ValueError("existing alpha atlas cannot supply the requested prefix")
        for i in range(start):
            box = card_box(i)
            sheet.paste(previous.crop(box), box[:2])
for i in range(start, count):
    box = card_box(i)
    sheet.paste(cutout(atlas.crop(box)), box[:2])
sheet.save(output)
