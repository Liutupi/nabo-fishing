"""Pack aligned NABO poses and all 100 transparent fish for the ESP32 display."""
from pathlib import Path
import struct

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art"
OUTPUT = ROOT / "firmware" / "main"
CARD_W, CARD_H = 84, 60
ACTOR_W, ACTOR_H = 140, 148
ORIGIN_X, ORIGIN_Y = 0, 12
HIP_X, HIP_Y = 62, 110
SCALE = 0.257
# Hip and reel attachment points measured in the original 1536×1024 sheet.
POSES = (
    ((348, 365), (463, 272)),
    ((1087, 365), (1159, 183)),
    ((320, 870), (534, 744)),
    ((1090, 873), (1158, 705)),
)

def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def colors565(image):
    return b"".join(struct.pack("<H", rgb565(*pixel)) for pixel in image.convert("RGB").getdata())

def pack_actors():
    sheet = Image.open(ART / "nabo-poses.png")
    if sheet.mode != "RGBA" or sheet.size != (1536, 1024):
        raise ValueError("unexpected NABO pose sheet dimensions or alpha")
    anchors = []
    frames = []
    for i, (hip, grip) in enumerate(POSES):
        left, top = i % 2 * 768, i // 2 * 512
        source = sheet.crop((left, top, left + 768, top + 512))
        frame = source.transform(
            (ACTOR_W, ACTOR_H), Image.Transform.AFFINE,
            (1 / SCALE, 0, hip[0] - left + (ORIGIN_X - HIP_X) / SCALE,
             0, 1 / SCALE, hip[1] - top + (ORIGIN_Y - HIP_Y) / SCALE),
            resample=Image.Resampling.BICUBIC,
        )
        alpha = frame.getchannel("A")
        bounds = alpha.point(lambda a: 255 if a > 30 else 0).getbbox()
        if not bounds or bounds[0] < 2 or bounds[1] < 2 or bounds[2] >= ACTOR_W or bounds[3] >= ACTOR_H:
            raise ValueError(f"NABO pose {i} clipped: {bounds}")
        frames.append(frame)
        anchors.append((round(HIP_X + SCALE * (grip[0] - hip[0])),
                        round(HIP_Y + SCALE * (grip[1] - hip[1]))))
    (OUTPUT / "nabo_anim.rgb565a").write_bytes(
        b"".join(colors565(frame) + frame.getchannel("A").tobytes() for frame in frames))
    (OUTPUT / "fishing_anim_anchors.inc").write_text(
        "".join(f"    {{{x}, {y}}},\n" for x, y in anchors), encoding="ascii")
    return frames

def fish_indices(card):
    # The atlas has a low-contrast teal backdrop. Flood fill only connected
    # backdrop pixels so dark enclosed details in the fish keep their color.
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
    if not 0.12 <= len(exterior) / (w * h) <= 0.92:
        raise ValueError(f"unexpected background coverage: {len(exterior)} of {w*h}")
    paletted = card.quantize(colors=255, dither=Image.Dither.NONE)
    palette = paletted.getpalette()
    indexed = bytes(0 if i in exterior else color + 1
                    for i, color in enumerate(paletted.getdata()))
    palette_data = struct.pack("<256H", 0, *(rgb565(*palette[i:i + 3])
                                            for i in range(0, 255 * 3, 3)))
    preview = card.convert("RGBA")
    preview.putalpha(Image.frombytes("L", card.size,
                                     bytes(0 if i in exterior else 255 for i in range(w*h))))
    return palette_data + indexed, preview

def pack_fish():
    atlas = Image.open(ART / "fish-catalog-preview.png").convert("RGB")
    if atlas.size != (CARD_W * 10, CARD_H * 10):
        raise ValueError("unexpected fish atlas size")
    packed, previews = [], []
    for i in range(100):
        x, y = i % 10 * CARD_W, i // 10 * CARD_H
        data, preview = fish_indices(atlas.crop((x, y, x + CARD_W, y + CARD_H)))
        packed.append(data)
        previews.append(preview)
    if len(set(packed)) != 100:
        raise ValueError("duplicate fish sprite")
    (OUTPUT / "fish_anim.pal").write_bytes(b"".join(packed))
    return previews

def pack_intro(cover):
    sheet = Image.open(ART / "boot-nabo-poses.png").convert("RGBA")
    source_w, source_h = sheet.width // 2, sheet.height // 2
    x0, y0, width, height = 6, 42, 136, 220
    scale = 0.322
    boot_center_x, boot_floor_y = 78, 255
    # Measured in each quadrant. Align both boots to the dock across the wave.
    guides = ((330, 613, 458, 289), (274, 613, 403, 287),
              (330, 608, 458, 286), (274, 608, 403, 285))
    backdrop = cover.crop((x0, y0, x0 + width, y0 + height))
    data = bytearray()
    descriptions = []
    for i, (foot_x, foot_y, grip_x, grip_y) in enumerate(guides):
        left, top = i % 2 * source_w, i // 2 * source_h
        source = sheet.crop((left, top, left + source_w, top + source_h))
        frame = source.transform(
            (width, height), Image.Transform.AFFINE,
            (1 / scale, 0, foot_x + (x0 - boot_center_x) / scale,
             0, 1 / scale, foot_y + (y0 - boot_floor_y) / scale),
            resample=Image.Resampling.BICUBIC)
        mask = frame.getchannel("A").point(lambda a: 0 if a < 80 else a)
        bounds = mask.getbbox()
        if not bounds or bounds[0] < 2 or bounds[1] < 2 or bounds[2] > width - 2 or bounds[3] > height - 2:
            raise ValueError(f"intro pose {i} clipped: {bounds}")
        # Preserve the silhouette's softer edge against the fixed backdrop.
        colors = Image.composite(frame.convert("RGB"), backdrop, mask)
        palette = colors.quantize(colors=255, dither=Image.Dither.NONE)
        rgb = palette.getpalette()
        record = bytearray(struct.pack("<256H", 0, *(rgb565(*rgb[j:j + 3])
                                                      for j in range(0, 765, 3))))
        color_bytes = palette.tobytes()
        alpha = mask.tobytes()
        for y in range(height):
            row = y * width
            occupied = [x for x in range(width) if alpha[row + x]]
            if not occupied:
                record.extend((0, 0))
                continue
            lo, hi = occupied[0], occupied[-1] + 1
            record.extend((lo, hi - lo))
            record.extend(color_bytes[row + x] + 1 if alpha[row + x] else 0
                          for x in range(lo, hi))
        descriptions.append((len(data), len(record),
                             round(boot_center_x + scale * (grip_x - foot_x)),
                             round(boot_floor_y + scale * (grip_y - foot_y))))
        data.extend(record)
    (OUTPUT / "intro_poses.pal").write_bytes(data)
    (OUTPUT / "fishing_intro_poses.inc").write_text(
        "".join("    {" + ", ".join(map(str, row)) + "},\n" for row in descriptions),
        encoding="ascii")


dock = ImageOps.fit(Image.open(ART / "fishing-dock.png").convert("RGB"), (240, 160),
                    Image.Resampling.LANCZOS)
(OUTPUT / "dock.rgb565").write_bytes(colors565(dock))
cover = ImageOps.fit(Image.open(ART / "boot-lake-plate.png").convert("RGB"),
                     (240, 320), Image.Resampling.LANCZOS)
(OUTPUT / "welcome_cover.rgb565").write_bytes(colors565(cover))
pack_intro(cover)
actors = pack_actors()
fish = pack_fish()
preview = Image.new("RGB", (4 * 240, 160 + 120), "#194451")
for i, actor in enumerate(actors):
    scene = dock.convert("RGBA")
    scene.alpha_composite(actor, (ORIGIN_X, ORIGIN_Y))
    preview.paste(scene, (i * 240, 0))
for i, index in enumerate((0, 4, 5, 14, 34, 53, 80, 99)):
    card = fish[index]
    preview.paste(card, (i * 120 + 18, 187), card)
preview.save(ART / "motion-sheet-preview.png")

