from PIL import Image, ImageChops, ImageOps

from fish_assets import ART, CARD_W, CARD_H, append_from, atlas_size, card_box, species_names


def source_cell(index):
    # The seventh legacy sheet is 2×2. All new source sheets are 4×4.
    if index < 96:
        sheet, cell = divmod(index, 16)
        return sheet + 1, 4, cell
    if index < 100:
        return 7, 2, index - 96
    sheet, cell = divmod(index - 100, 16)
    return sheet + 8, 4, cell


def make_card(atlas, columns, cell, sheet):
    row, col = divmod(cell, columns)
    card = atlas.crop((col * atlas.width // columns, row * atlas.height // columns,
                       (col + 1) * atlas.width // columns, (row + 1) * atlas.height // columns))
    bg = card.getpixel((1, 1))
    diff = ImageChops.difference(card, Image.new("RGB", card.size, bg))
    bounds = diff.convert("L").point(lambda value: 255 if value > 24 else 0).getbbox()
    if not bounds:
        raise ValueError(f"empty fish in sheet {sheet}, row {row}, column {col}")
    left, top, right, bottom = bounds
    card = card.crop((max(0, left - 5), max(0, top - 5),
                      min(card.width, right + 5), min(card.height, bottom + 5)))
    return ImageOps.pad(card, (CARD_W, CARD_H), Image.Resampling.LANCZOS, color=bg)


count = len(species_names())
start = append_from(count)
output = ART / "fish-catalog-preview.png"
preview = Image.new("RGB", atlas_size(count), "#10343b")
if start:
    with Image.open(output) as previous:
        if previous.width != preview.width or previous.height < atlas_size(start)[1]:
            raise ValueError("existing fish preview cannot supply the requested prefix")
        for index in range(start):
            box = card_box(index)
            preview.paste(previous.crop(box), box[:2])

sheet = None
for index in range(start, count):
    number, columns, cell = source_cell(index)
    if number != sheet:
        with Image.open(ART / f"fish-{number:02}.png") as image:
            atlas = image.convert("RGB")
        sheet = number
    preview.paste(make_card(atlas, columns, cell, sheet), card_box(index)[:2])
preview.save(output)
