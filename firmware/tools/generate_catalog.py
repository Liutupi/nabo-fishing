from pathlib import Path
import json

root = Path(__file__).resolve().parents[2]
names = (root / "art/species.txt").read_text("utf-8-sig").split()
assert len(names) == len(set(names)) == 100
initial_levels = [1, 1, 2, 2, 3, 4]
initial_values = [8, 14, 22, 38, 65, 100]
species = []
for i, name in enumerate(names):
    level = initial_levels[i] if i < 6 else 1 + (i - 6) // 24
    value = initial_values[i] if i < 6 else [8, 22, 45, 80][level - 1] + 2 * (i % 9)
    species.append(dict(name=name, value=value, level=level))
data = json.dumps(species, ensure_ascii=False, separators=(",", ":"))
(root / "prototype/species.js").write_text(
    "const catalogSpecies = " + data + ";\n"
    "if (typeof module !== 'undefined') module.exports = catalogSpecies;\n", "utf-8")
(root / "firmware/main/fishing_species.inc").write_text(
    "".join(f'    {{"{s["name"]}", {s["value"]}, {s["level"]}}},\n' for s in species), "utf-8")
