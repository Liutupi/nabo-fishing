"""Write firmware/main/nabo_night.rgb565a: the four NABO poses in moonlight.

Run after generate_motion.py, which writes the daylight poses this reads.
"""
from pathlib import Path

import numpy as np

MAIN = Path(__file__).resolve().parents[1] / "main"
W, H, POSES = 140, 148, 4
TINT = (0.70, 0.74, 0.92)      # the dock lantern keeps NABO readable at night

source = (MAIN / "nabo_anim.rgb565a").read_bytes()
frame_bytes = W * H * 3
assert len(source) == POSES * frame_bytes, "unexpected pose sheet size"
night = bytearray()
for i in range(POSES):
    frame = source[i * frame_bytes:(i + 1) * frame_bytes]
    pixels = np.frombuffer(frame[:W * H * 2], "<u2")
    r = ((pixels >> 11) * TINT[0]).astype(np.uint16)
    g = ((pixels >> 5 & 63) * TINT[1]).astype(np.uint16)
    b = ((pixels & 31) * TINT[2]).astype(np.uint16)
    night += (r << 11 | g << 5 | b).astype("<u2").tobytes() + frame[W * H * 2:]
(MAIN / "nabo_night.rgb565a").write_bytes(night)
