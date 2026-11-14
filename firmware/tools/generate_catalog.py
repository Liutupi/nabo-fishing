import json

from fish_assets import RELEASED_COUNT, ROOT, species_names

initial_levels = [1, 1, 2, 2, 3, 4]
initial_values = [8, 14, 22, 38, 65, 100]
species = []
for i, name in enumerate(species_names()):
    level = initial_levels[i] if i < 6 else min(4, 1 + (i - 6) // 24)
    value = initial_values[i] if i < 6 else [8, 22, 45, 80][level - 1] + 2 * (i % 9)
    species.append(dict(name=name, value=value, level=level))

released, new = species[:RELEASED_COUNT], species[RELEASED_COUNT:]
def js(items):
    return json.dumps(items, ensure_ascii=False, separators=(",", ":"))

(ROOT / "prototype/species.js").write_text(
    "const catalogSpecies = " + js(released) + ";\n"
    "if (typeof module !== 'undefined') module.exports = catalogSpecies;\n", "utf-8")
(ROOT / "prototype/next/species.js").write_text(
    "const nextCatalogSpecies = [\n"
    "    ...(typeof module === 'undefined' ? catalogSpecies : require('../species.js')),\n"
    "    ..." + js(new) + ",\n];\n"
    "if (typeof module !== 'undefined') module.exports = nextCatalogSpecies;\n", "utf-8")

def c(items):
    return "".join(f'    {{"{s["name"]}", {s["value"]}, {s["level"]}}},\n' for s in items)

(ROOT / "firmware/main/fishing_species.inc").write_text(
    c(released) + "#ifndef FISH_LEGACY_CATALOG\n" + c(new) + "#endif\n", "utf-8")
