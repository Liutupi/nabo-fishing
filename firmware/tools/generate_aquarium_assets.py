"""Pack aquarium backgrounds and ornaments as LVGL RGB565 image data."""
from pathlib import Path

from PIL import Image
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art"
MAIN = ROOT / "firmware/main"


def colors(image):
    rgb = np.asarray(image.convert("RGB"), dtype=np.uint16)
    return ((rgb[:, :, 0] >> 3) << 11 | (rgb[:, :, 1] >> 2) << 5 |
            (rgb[:, :, 2] >> 3)).astype("<u2").tobytes()


data = bytearray()
descriptors = []


def add(image, alpha=False):
    if len(data) % 4:
        data.extend(bytes(4 - len(data) % 4))
    offset = len(data)
    data.extend(colors(image))
    if alpha:
        data.extend(image.getchannel("A").tobytes())
    descriptors.append((image.width, image.height, offset, len(data) - offset))


# The default tank already ships full-size; the other themes use half-size plates.
for i in range(1, 4):
    with Image.open(ART / f"tanks/tank-{i}.png") as img:
        add(img.resize((160, 120), Image.Resampling.LANCZOS))
for i in range(10):
    with Image.open(ART / f"decor/decor-{i}.png") as img:
        add(img, alpha=True)

(MAIN / "aquarium.rgb565a").write_bytes(data)
(MAIN / "fishing_aquarium_assets.inc").write_text(
    "".join(f"    AQUARIUM_IMAGE({w}, {h}, {offset}, {length}, {'RGB565' if i < 3 else 'RGB565A8'}),\n"
            for i, (w, h, offset, length) in enumerate(descriptors)), encoding="ascii")
