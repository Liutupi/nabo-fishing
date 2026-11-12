"""Turn the native LVGL harness's raw 25 FPS frames into a preview GIF."""
import sys
from pathlib import Path

from PIL import Image

WIDTH, HEIGHT = 240, 320
FRAME = WIDTH * HEIGHT * 2
data = Path(sys.argv[1]).read_bytes()
if not data or len(data) % FRAME:
    raise ValueError("invalid RGB565 frame stream")
frames = [Image.frombytes("RGB", (WIDTH, HEIGHT), data[i:i + FRAME], "raw", "BGR;16")
          for i in range(0, len(data), FRAME)]
if len(sys.argv) > 3:
    frames = frames[:int(sys.argv[3])]
if len({f.tobytes() for f in frames}) < 30:
    raise ValueError("preview lacks motion")
# A shared palette keeps unchanged parts of the scene stable between frames.
sample = Image.new("RGB", (WIDTH * 4, HEIGHT * 3))
for n in range(12):
    sample.paste(frames[(len(frames) - 1) * n // 11], (n % 4 * WIDTH, n // 4 * HEIGHT))
palette = sample.quantize(colors=256)
indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
indexed[0].save(sys.argv[2], save_all=True, append_images=indexed[1:],
                duration=40, loop=0, optimize=True)
print(f"{len(frames)} native LVGL frames")
