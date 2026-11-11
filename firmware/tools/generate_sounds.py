from pathlib import Path
import math
import random
import struct
import wave

root = Path(__file__).resolve().parents[2]
out = root / "firmware/main"
rate = 16000
def save(name, samples):
    (out / f"{name}.pcm").write_bytes(struct.pack(f"<{len(samples)}h", *samples))

with wave.open(str(root / "art/boot-welcome.wav"), "rb") as f:
    assert (f.getframerate(), f.getnchannels(), f.getsampwidth()) == (rate, 1, 2)
    raw = f.readframes(f.getnframes())
    samples = list(struct.unpack(f"<{len(raw)//2}h", raw))
loud = [i for i, sample in enumerate(samples) if abs(sample) > 250]
assert loud and 8000 < loud[-1] - loud[0] < 64000
samples = samples[max(0, loud[0] - 480):loud[-1] + 800]
scale = 11500 / max(abs(sample) for sample in samples)
save("welcome", [int(sample * scale) for sample in samples])

rng = random.Random(23)
def effect(name, duration, notes, noise=0):
    samples = []
    smooth = 0
    for i in range(round(duration * rate)):
        t = i / rate
        value = 0
        for start, end, hz, slide, amp in notes:
            dt = t - start
            if not 0 <= dt < end:
                continue
            phase = math.tau * (hz * dt + slide * dt * dt / 2)
            env = min(1, dt * 180) * (1 - dt / end)**1.6
            value += amp * env * (math.sin(phase) + .14 * math.sin(2 * phase))
        smooth = .76 * smooth + .24 * rng.uniform(-1, 1)
        value += noise * smooth * math.exp(-18 * t) * min(1, t * 240)
        samples.append(max(-14000, min(14000, round(value))))
    save(name, samples)

effect("cast", .21, [(0,.18,430,-850,2900),(.10,.10,920,-4100,1700)], 5800)
effect("bite", .24, [(0,.12,650,2600,5300),(.105,.12,880,1800,4400)], 1700)
effect("land", .50, [(0,.20,523,0,4800),(.105,.21,659,0,4500),(.22,.28,784,0,4500)])
effect("miss", .27, [(0,.24,410,-970,3800)], 2200)
effect("upgrade", .60, [(0,.19,587,0,4300),(.12,.20,740,0,4700),(.24,.23,880,0,4900),(.37,.23,1175,0,4000)])
