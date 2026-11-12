"""Bake the 16 shore backdrops: 4 seasons x 4 periods of the day.

Writes firmware/main/shore.rgb565 (16 frames of 240x160, season-major) from the
empty dock art, and art/shore/shore-<season><period>.png with NABO for the
browser prototype.
"""
from pathlib import Path
import struct

import numpy as np
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art" / "shore"
W, H = 240, 160
SUN = (116, 28)

def to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    v = rgb.max(-1)
    c = v - rgb.min(-1)
    s = np.where(v > 0, c / np.maximum(v, 1e-6), 0)
    safe = np.maximum(c, 1e-6)
    h = np.where(v == r, (g - b) / safe % 6, np.where(v == g, (b - r) / safe + 2, (r - g) / safe + 4))
    return np.stack([np.where(c > 0, h * 60, 0), s, v], -1)

def to_rgb(hsv):
    h, s, v = hsv[..., 0] % 360 / 60, hsv[..., 1].clip(0, 1), hsv[..., 2].clip(0, 1)
    k = lambda n: v - v * s * np.clip(np.minimum((n + h) % 6, 4 - (n + h) % 6), 0, 1)
    return np.stack([k(5), k(3), k(1)], -1)

def band(x, low, high, soft):
    return np.clip((x - low) / soft, 0, 1) * np.clip((high - x) / soft, 0, 1)

def bake(base, season, period, open_sky):
    rng = np.random.default_rng(20261112)
    ys, xs = np.mgrid[0:H, 0:W]
    fx, fy = xs / W, ys / H
    hsv = to_hsv(base)
    hue, sat = hsv[..., 0], hsv[..., 1]
    base_hue, base_sat = hue.copy(), sat.copy()
    leaf = band(hue, 42, 152, 10) * np.clip((sat - 0.22) / 0.1, 0, 1)
    if season == 0:      # spring: fresh green and blossom specks in the canopy
        hsv[..., 2] *= 1 + 0.06 * leaf
        hsv[..., 0] -= 6 * leaf
    elif season == 1:    # summer: deep saturated green
        hsv[..., 1] *= 1 + 0.18 * leaf
        hsv[..., 2] *= 1 - 0.10 * leaf
        hsv[..., 0] += 8 * leaf
    elif season == 2:    # autumn: greens turn amber and red
        target = 14 + (hue - 42) * 0.32
        hsv[..., 0] = hue + (target - hue) * leaf
        hsv[..., 1] *= 1 + 0.12 * leaf
    else:                # winter: frosted foliage, cooler picture
        hsv[..., 1] *= 1 - 0.78 * leaf
        hsv[..., 2] = hsv[..., 2] + (0.96 - hsv[..., 2]) * 0.6 * leaf
        hsv[..., 1] *= 0.86
    rgb = to_rgb(hsv)
    if season == 0:
        canopy = (leaf > 0.6) & (fy < 0.5)
        specks = canopy & (rng.random((H, W)) < 0.10)
        rgb[specks] = (1.0, 0.72, 0.82)
    if season == 3:
        rgb = rgb * (0.95, 0.99, 1.04)

    # With NABO in the picture, keep the sky edit clear of his face and hat.
    sky = (fy < 0.325) if open_sky else (((fx > 0.40) & (fy < 0.31)) | ((fx < 0.115) & (fy < 0.2)))
    sky = sky.astype(float)
    hsv = to_hsv(rgb.clip(0, 1))
    hue, sat, val = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    warm_sky = sky * np.clip((base_sat - 0.08) / 0.1, 0, 1) * (((base_hue > 262) | (base_hue < 40)).astype(float)) * (1 - leaf)
    sun = np.hypot(xs - SUN[0], ys - SUN[1])
    if period == 1:      # day: blue sky, white clouds, the sun stays
        cloud = warm_sky * np.clip((48 - np.abs(base_hue - 20)) / 20, 0, 1) * (base_hue < 60)
        blue = warm_sky * (1 - cloud) * np.clip((sun - 9) / 6, 0, 1)
        day = np.stack([0.42 + 0.3 * fy * 3, 0.70 + 0.2 * fy * 3, np.full_like(fy, 0.96)], -1)
        white = np.stack([val * 0 + 1.0, val * 0 + 0.99, val * 0 + 0.97], -1) * (0.9 + 0.1 * val[..., None])
        rgb = rgb + (day - rgb) * blue[..., None] * 0.92
        rgb = rgb + (white - rgb) * (cloud * np.clip((sun - 9) / 6, 0, 1))[..., None] * 0.85
        water_glint = (fy > 0.3) * band(hue, 20, 75, 10) * (fx > 0.42) * np.clip((sat - 0.2) / 0.1, 0, 1)
        rgb = rgb + (np.array([0.75, 0.97, 0.95]) - rgb) * water_glint[..., None] * 0.7
        rgb = rgb * (1.03, 1.04, 1.05)
    elif period == 0:    # dawn: pale, misty, cool pink
        grey = rgb.mean(-1, keepdims=True)
        rgb = grey + (rgb - grey) * 0.62
        rgb = rgb * (0.97, 0.98, 1.08) + 0.05
        mist = np.exp(-((fy - 0.33) / 0.07) ** 2) * (1 if open_sky else np.clip((fx - 0.34) / 0.1, 0, 1)) * 0.6
        rgb = rgb + (np.array([1.0, 0.94, 0.97]) - rgb) * mist[..., None]
    elif period == 2:    # dusk: the base painting, a touch deeper
        grey = rgb.mean(-1, keepdims=True)
        rgb = (grey + (rgb - grey) * 1.12) * (1.02, 0.93, 0.90)
    else:                # night: moonlight, stars and a lantern by the tackle box
        lit = rgb.copy()
        rgb = rgb * (0.26, 0.36, 0.60)
        stars = (warm_sky > 0.5) & (rng.random((H, W)) < 0.03) & (sun > 16)
        rgb[stars] = (0.95, 0.95, 0.85)
        glow = np.clip(1 - sun / 20, 0, 1)[..., None] ** 2
        rgb = rgb + (np.array([0.80, 0.86, 1.0]) - rgb) * glow * 0.5 * sky[..., None]
        rgb[sun <= 6.2] = (0.98, 0.96, 0.82)
        rgb[np.hypot(xs - SUN[0] - 3, ys - SUN[1] + 2) <= 4.4] *= (0.93, 0.93, 0.95)
        lantern = np.clip(1 - np.hypot((xs - 34) / 1.25, ys - 104) / 62, 0, 1)[..., None] ** 1.6
        rgb = rgb + (lit * (1.0, 0.86, 0.66) - rgb) * lantern * 0.85
    return Image.fromarray((rgb.clip(0, 1) * 255 + 0.5).astype(np.uint8))

def rgb565(image):
    pixels = np.asarray(image, dtype=np.uint16)
    packed = (pixels[..., 0] >> 3) << 11 | (pixels[..., 1] >> 2) << 5 | pixels[..., 2] >> 3
    return packed.astype("<u2").tobytes()

def main():
    def load(name):
        art = ImageOps.fit(Image.open(ROOT / "art" / name).convert("RGB"), (W, H), Image.Resampling.LANCZOS)
        return np.asarray(art, dtype=float) / 255
    actor, dock = load("fishing-nabo.png"), load("fishing-dock.png")
    OUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGB", (W * 4, H * 4))
    frames = []
    for season in range(4):
        for period in range(4):
            image = bake(actor, season, period, False)
            image.save(OUT / f"shore-{season}{period}.png")
            sheet.paste(image, (period * W, season * H))
            frames.append(rgb565(bake(dock, season, period, True)))
    sheet.save(OUT / "shore-preview.png")
    (ROOT / "firmware" / "main" / "shore.rgb565").write_bytes(b"".join(frames))

main()
