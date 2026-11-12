"""Paint the 320x240 landscape aquarium backdrop.

Writes art/tank-backdrop.png for the browser prototype and
firmware/main/tank.rgb565 for the badge. Fish, bubbles and labels are drawn live.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
W, H, S = 320, 240, 4          # S: supersampling for smooth edges

def main():
    ys = np.linspace(0, 1, H * S)[:, None]
    stops = [(0.0, (42, 163, 164)), (0.55, (20, 118, 128)), (1.0, (11, 71, 86))]
    water = np.zeros((H * S, W * S, 3))
    for (a, ca), (b, cb) in zip(stops, stops[1:]):
        t = np.clip((ys - a) / (b - a), 0, 1)
        band = (ys >= a) & (ys <= b)
        water = np.where(band[..., None], np.array(ca) + (np.array(cb) - np.array(ca)) * t[..., None], water)
    image = Image.fromarray(water.astype(np.uint8)).convert("RGBA")

    def layer(paint):
        sheet = Image.new("RGBA", image.size, (0, 0, 0, 0))
        paint(ImageDraw.Draw(sheet))
        image.alpha_composite(sheet)
    k = lambda *points: [(x * S, y * S) for x, y in points]
    def rays(d):
        for x, w in ((40, 26), (118, 40), (206, 30), (270, 22)):
            d.polygon(k((x, 0), (x + w, 0), (x + w - 46, 200), (x - 70, 200)), fill=(190, 255, 236, 18))
    def rocks(d):
        for x, y, rx, ry in ((52, 178, 46, 30), (250, 174, 58, 36), (150, 186, 34, 16)):
            d.ellipse(k((x - rx, y - ry), (x + rx, y + ry)), fill=(14, 85, 96, 255))
    def weeds(d):
        for i, (x, h, lean) in enumerate(((22, 52, 6), (34, 70, -5), (140, 40, 4), (286, 64, -7), (300, 46, 5))):
            points = [(x + lean * (t / 10) ** 2, 204 - h * t / 10) for t in range(11)]
            d.line(k(*points), fill=(47, 174, 116, 255) if i % 2 else (28, 138, 99, 255), width=4 * S, joint="curve")
    def floor(d):
        top = [(x, 186 + 7 * np.sin(x / 51 + 1.1) + 3 * np.sin(x / 17)) for x in range(0, W + 1, 4)]
        d.polygon(k(*top, (W, H), (0, H)), fill=(216, 192, 140, 255))
        for i in range(26):
            x, y = i * 47 % 316, 200 + i * 29 % 36
            d.rectangle(k((x, y), (x + 3, y + 2)), fill=(196, 169, 115, 255))
        d.ellipse(k((206, 176), (258, 204)), fill=(92, 111, 117, 255))
        d.ellipse(k((212, 179), (240, 193)), fill=(116, 136, 140, 255))
        d.ellipse(k((88, 195), (104, 205)), fill=(240, 165, 140, 255))
        d.rectangle(k((0, 0), (W, 3)), fill=(255, 255, 255, 56))
    for paint in (rays, rocks, weeds, floor):
        layer(paint)
    image = image.convert("RGB").resize((W, H), Image.Resampling.LANCZOS)
    image.save(ROOT / "art" / "tank-backdrop.png")
    pixels = np.asarray(image, dtype=np.uint16)
    packed = (pixels[..., 0] >> 3) << 11 | (pixels[..., 1] >> 2) << 5 | pixels[..., 2] >> 3
    (ROOT / "firmware" / "main" / "tank.rgb565").write_bytes(packed.astype("<u2").tobytes())

main()
