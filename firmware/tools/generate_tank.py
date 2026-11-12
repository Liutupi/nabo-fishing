"""Paint the 320x240 landscape aquarium backdrops.

Writes art/tanks/tank-<n>.png for every aquarium type, plus the plain one as
art/tank-backdrop.png and firmware/main/tank.rgb565, which the released
firmware draws. Fish, ornaments, bubbles and labels are drawn live.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
W, H, S = 320, 240, 4          # S: supersampling for smooth edges
k = lambda *points: [(x * S, y * S) for x, y in points]

THEMES = (
    dict(name="clear", water=((42, 163, 164), (20, 118, 128), (11, 71, 86)), ray=(190, 255, 236, 18),
         rock=(14, 85, 96), sand=(216, 192, 140), grain=(196, 169, 115), weeds=((28, 138, 99), (47, 174, 116))),
    dict(name="reef", water=((70, 196, 204), (30, 150, 172), (18, 96, 130)), ray=(255, 250, 210, 22),
         rock=(24, 110, 132), sand=(238, 214, 170), grain=(222, 190, 140), weeds=((34, 160, 120), (70, 196, 140))),
    dict(name="wreck", water=((38, 110, 150), (22, 70, 112), (12, 38, 72)), ray=(170, 220, 255, 16),
         rock=(16, 52, 86), sand=(150, 150, 140), grain=(124, 126, 120), weeds=((24, 104, 96), (36, 132, 110))),
    dict(name="starry", water=((44, 40, 110), (26, 24, 78), (12, 12, 44)), ray=(200, 190, 255, 20),
         rock=(30, 26, 82), sand=(118, 110, 160), grain=(96, 90, 140), weeds=((70, 60, 160), (110, 90, 200))),
)

def reef(d):
    # Coral fans on the far rocks.
    for x, y, h, color in ((40, 168, 30, (232, 96, 110)), (58, 172, 22, (246, 150, 96)),
                           (246, 160, 34, (240, 112, 150)), (270, 166, 24, (250, 170, 90))):
        for lean in (-10, -4, 3, 9):
            d.line(k((x, y), (x + lean / 2, y - h / 2), (x + lean, y - h)), fill=color + (255,), width=3 * S)
            d.ellipse(k((x + lean - 2.5, y - h - 2.5), (x + lean + 2.5, y - h + 2.5)), fill=color + (255,))

def wreck(d):
    # A sunken hull leaning on the far rocks, with a broken mast.
    hull = (10, 34, 60, 255)
    d.polygon(k((176, 178), (188, 140), (292, 128), (312, 172)), fill=hull)
    d.polygon(k((230, 133), (236, 74), (241, 74), (240, 132)), fill=hull)
    d.polygon(k((241, 80), (268, 96), (241, 104)), fill=(14, 44, 74, 255))
    for x in (206, 232, 258):
        d.ellipse(k((x, 148), (x + 9, 157)), fill=(30, 84, 120, 255))

def starry(d):
    # Glowing plankton and a moon seen through the surface.
    rng = np.random.default_rng(7)
    for _ in range(70):
        x, y, r = rng.uniform(4, 316), rng.uniform(8, 176), rng.choice((0.6, 0.6, 1.0, 1.4))
        tint = (255, 244, 190) if rng.random() < 0.5 else (170, 220, 255)
        d.ellipse(k((x - r, y - r), (x + r, y + r)), fill=tint + (int(rng.uniform(120, 230)),))
    for r, a in ((34, 26), (24, 44), (15, 210)):
        d.ellipse(k((262 - r, 34 - r), (262 + r, 34 + r)), fill=(236, 232, 255, a))

EXTRAS = {"reef": reef, "wreck": wreck, "starry": starry}

def paint(theme):
    ys = np.linspace(0, 1, H * S)[:, None]
    stops = list(zip((0.0, 0.55, 1.0), theme["water"]))
    water = np.zeros((H * S, W * S, 3))
    for (a, ca), (b, cb) in zip(stops, stops[1:]):
        t = np.clip((ys - a) / (b - a), 0, 1)
        band = (ys >= a) & (ys <= b)
        water = np.where(band[..., None], np.array(ca) + (np.array(cb) - np.array(ca)) * t[..., None], water)
    image = Image.fromarray(water.astype(np.uint8)).convert("RGBA")

    def layer(painter):
        sheet = Image.new("RGBA", image.size, (0, 0, 0, 0))
        painter(ImageDraw.Draw(sheet))
        image.alpha_composite(sheet)
    def rays(d):
        for x, w in ((40, 26), (118, 40), (206, 30), (270, 22)):
            d.polygon(k((x, 0), (x + w, 0), (x + w - 46, 200), (x - 70, 200)), fill=theme["ray"])
    def rocks(d):
        for x, y, rx, ry in ((52, 178, 46, 30), (250, 174, 58, 36), (150, 186, 34, 16)):
            d.ellipse(k((x - rx, y - ry), (x + rx, y + ry)), fill=theme["rock"] + (255,))
    def weeds(d):
        for i, (x, h, lean) in enumerate(((22, 52, 6), (34, 70, -5), (140, 40, 4), (286, 64, -7), (300, 46, 5))):
            points = [(x + lean * (t / 10) ** 2, 204 - h * t / 10) for t in range(11)]
            d.line(k(*points), fill=theme["weeds"][i % 2] + (255,), width=4 * S, joint="curve")
    def floor(d):
        top = [(x, 186 + 7 * np.sin(x / 51 + 1.1) + 3 * np.sin(x / 17)) for x in range(0, W + 1, 4)]
        d.polygon(k(*top, (W, H), (0, H)), fill=theme["sand"] + (255,))
        for i in range(26):
            x, y = i * 47 % 316, 200 + i * 29 % 36
            d.rectangle(k((x, y), (x + 3, y + 2)), fill=theme["grain"] + (255,))
        d.ellipse(k((206, 176), (258, 204)), fill=(92, 111, 117, 255))
        d.ellipse(k((212, 179), (240, 193)), fill=(116, 136, 140, 255))
        d.ellipse(k((88, 195), (104, 205)), fill=(240, 165, 140, 255))
        d.rectangle(k((0, 0), (W, 3)), fill=(255, 255, 255, 56))
    painters = [rays, rocks, weeds, floor]
    if theme["name"] in EXTRAS:
        painters.insert(2, EXTRAS[theme["name"]])
    for painter in painters:
        layer(painter)
    return image.convert("RGB").resize((W, H), Image.Resampling.LANCZOS)

def main():
    (ROOT / "art" / "tanks").mkdir(exist_ok=True)
    for i, theme in enumerate(THEMES):
        image = paint(theme)
        image.save(ROOT / "art" / "tanks" / f"tank-{i}.png")
        if i:
            continue
        image.save(ROOT / "art" / "tank-backdrop.png")
        pixels = np.asarray(image, dtype=np.uint16)
        packed = (pixels[..., 0] >> 3) << 11 | (pixels[..., 1] >> 2) << 5 | pixels[..., 2] >> 3
        (ROOT / "firmware" / "main" / "tank.rgb565").write_bytes(packed.astype("<u2").tobytes())

main()
