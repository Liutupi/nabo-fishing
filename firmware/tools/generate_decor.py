"""Draw the aquarium ornaments as small outlined sprites.

Writes art/decor/decor-<n>.png (transparent) and art/decor/decor-preview.png.
The order matches `decors` in the prototype rules. These are stand-in drawings:
replace a PNG with nicer art of the same size and nothing else needs to change.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
S = 4
OUTLINE = (16, 47, 56, 255)

def sprite(width, height, painter):
    sheet = Image.new("RGBA", (width * S, height * S), (0, 0, 0, 0))
    painter(ImageDraw.Draw(sheet), lambda *points: [(x * S, y * S) for x, y in points])
    small = sheet.resize((width, height), Image.Resampling.LANCZOS)
    alpha = small.getchannel("A").point(lambda a: 255 if a > 110 else 0)
    small.putalpha(alpha)
    edge = alpha.filter(ImageFilter.MaxFilter(3))
    framed = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    framed.paste(Image.new("RGBA", (width, height), OUTLINE), mask=edge)
    framed.alpha_composite(small)
    return framed

def weeds(d, k):
    for x, top, lean, color in ((8, 6, -4, (36, 150, 104)), (15, 2, 3, (58, 186, 124)),
                                (22, 10, 5, (30, 132, 96)), (12, 16, -6, (70, 200, 136))):
        d.polygon(k((x - 3, 39), (x + lean - 1, top), (x + lean + 1, top), (x + 3, 39)), fill=color + (255,))

def shells(d, k):
    for x, y, r, color in ((9, 15, 7, (255, 214, 190)), (22, 16, 6, (246, 170, 160)), (15, 9, 5, (255, 236, 214))):
        d.pieslice(k((x - r, y - r), (x + r, y + r)), 180, 360, fill=color + (255,))
        d.rectangle(k((x - r, y), (x + r, y + 2)), fill=color + (255,))
        for a in (-r * 0.5, 0, r * 0.5):
            d.line(k((x, y + 1), (x + a, y - r + 1)), fill=(214, 130, 120, 255), width=S)

def coral(d, k):
    red, pink = (232, 84, 96, 255), (250, 150, 160, 255)
    d.line(k((18, 40), (18, 22)), fill=red, width=5 * S)
    for (ax, ay), (bx, by) in (((18, 30), (7, 14)), ((18, 26), (29, 10)), ((18, 22), (16, 4)),
                               ((10, 19), (3, 8)), ((26, 15), (33, 18))):
        d.line(k((ax, ay), (bx, by)), fill=red, width=4 * S)
        d.ellipse(k((bx - 3, by - 3), (bx + 3, by + 3)), fill=pink)

def jar(d, k):
    clay, dark, band = (196, 120, 78, 255), (120, 66, 44, 255), (236, 184, 120, 255)
    d.ellipse(k((3, 10), (25, 36)), fill=clay)
    d.rectangle(k((9, 3), (19, 12)), fill=clay)
    d.ellipse(k((7, 1), (21, 7)), fill=dark)
    d.rectangle(k((4, 20), (24, 23)), fill=band)
    d.arc(k((0, 9), (9, 22)), 90, 270, fill=dark, width=2 * S)

def bubbler(d, k):
    d.ellipse(k((2, 12), (26, 30)), fill=(120, 136, 142, 255))
    d.ellipse(k((6, 14), (18, 22)), fill=(150, 166, 170, 255))
    for x, y in ((10, 24), (19, 20), (15, 16)):
        d.ellipse(k((x - 2, y - 2), (x + 2, y + 2)), fill=(70, 84, 92, 255))
    for x, y, r in ((12, 8, 2.5), (19, 4, 2), (7, 3, 1.5)):
        d.ellipse(k((x - r, y - r), (x + r, y + r)), fill=(200, 246, 255, 255))

def castle(d, k):
    stone, shade, roof = (206, 196, 180, 255), (150, 142, 132, 255), (214, 86, 80, 255)
    d.rectangle(k((10, 22), (34, 44)), fill=stone)
    for x in (4, 30):
        d.rectangle(k((x, 14), (x + 10, 44)), fill=stone)
        d.polygon(k((x - 2, 15), (x + 5, 3), (x + 12, 15)), fill=roof)
        d.rectangle(k((x + 3, 22), (x + 7, 28)), fill=(60, 80, 96, 255))
    for x in (11, 17, 23, 29):
        d.rectangle(k((x, 18), (x + 4, 23)), fill=stone)
    d.pieslice(k((16, 30), (28, 54)), 180, 360, fill=(74, 52, 44, 255))
    d.line(k((10, 36), (34, 36)), fill=shade, width=S)
    d.line(k((35, 3), (35, -3)), fill=shade, width=S)

def chest(d, k):
    wood, trim, gold = (150, 90, 52, 255), (240, 190, 80, 255), (255, 222, 110, 255)
    d.polygon(k((3, 10), (8, 2), (30, 2), (33, 10)), fill=(120, 70, 40, 255))
    for x, y in ((10, 9), (16, 7), (22, 9), (27, 8), (13, 6), (20, 5)):
        d.ellipse(k((x - 3, y - 3), (x + 3, y + 3)), fill=gold)
    d.rectangle(k((3, 11), (33, 28)), fill=wood)
    d.rectangle(k((3, 11), (33, 14)), fill=trim)
    d.rectangle(k((15, 11), (21, 21)), fill=trim)
    d.ellipse(k((16.5, 15), (19.5, 18)), fill=(90, 56, 30, 255))
    for x in (3, 30):
        d.rectangle(k((x, 11), (x + 3, 28)), fill=trim)

def helmet(d, k):
    brass, dark, glass = (214, 160, 70, 255), (150, 104, 40, 255), (150, 220, 232, 255)
    d.ellipse(k((3, 2), (29, 28)), fill=brass)
    d.rectangle(k((1, 26), (31, 33)), fill=dark)
    d.ellipse(k((9, 8), (23, 22)), fill=dark)
    d.ellipse(k((11, 10), (21, 20)), fill=glass)
    d.line(k((16, 10), (16, 20)), fill=dark, width=S)
    d.line(k((11, 15), (21, 15)), fill=dark, width=S)
    for x in (5, 13, 21):
        d.ellipse(k((x, 28), (x + 3, 31)), fill=brass)

def jellyfish(d, k):
    bell, glow = (255, 190, 226, 255), (255, 236, 250, 255)
    d.pieslice(k((3, 2), (29, 30)), 180, 360, fill=bell)
    d.pieslice(k((8, 6), (24, 24)), 180, 360, fill=glow)
    d.rectangle(k((3, 15), (29, 18)), fill=bell)
    for x, h in ((7, 14), (13, 19), (19, 15), (25, 18)):
        d.line(k((x, 18), (x - 2, 18 + h / 2), (x + 1, 18 + h)), fill=(246, 150, 206, 255), width=2 * S, joint="curve")

def statue(d, k):
    gold, deep, light = (242, 190, 60, 255), (190, 136, 30, 255), (255, 232, 140, 255)
    d.rectangle(k((4, 38), (28, 45)), fill=deep)
    d.rectangle(k((7, 35), (25, 39)), fill=gold)
    d.rectangle(k((11, 24), (21, 36)), fill=gold)                       # body
    d.ellipse(k((7, 8), (25, 26)), fill=gold)                            # head
    d.pieslice(k((6, 3), (26, 21)), 180, 360, fill=light)                # cap
    d.rectangle(k((5, 11), (29, 14)), fill=light)                        # cap brim
    for x in (11, 18):
        d.ellipse(k((x, 14), (x + 4, 18)), outline=deep, width=S)        # glasses
    d.line(k((21, 28), (30, 16)), fill=deep, width=2 * S)                # fishing rod
    d.line(k((30, 16), (30, 26)), fill=deep, width=S)

ITEMS = ((30, 40, weeds), (30, 20, shells), (36, 42, coral), (28, 38, jar), (28, 32, bubbler),
         (44, 46, castle), (36, 30, chest), (32, 34, helmet), (32, 40, jellyfish), (32, 46, statue))

def main():
    out = ROOT / "art" / "decor"
    out.mkdir(exist_ok=True)
    preview = Image.new("RGBA", (len(ITEMS) * 48, 56), (20, 118, 128, 255))
    for i, (w, h, painter) in enumerate(ITEMS):
        image = sprite(w, h, painter)
        image.save(out / f"decor-{i}.png")
        preview.alpha_composite(image, (i * 48 + (48 - w) // 2, 52 - h))
    preview.save(out / "decor-preview.png")

main()
