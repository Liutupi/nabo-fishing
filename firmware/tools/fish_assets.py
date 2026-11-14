"""Shared layout and append settings for the fish asset generators."""
from argparse import ArgumentParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art"
CARD_W, CARD_H, COLUMNS = 84, 60, 10
RELEASED_COUNT, MAX_COUNT = 100, 128


def species_names():
    names = (ART / "species.txt").read_text("utf-8-sig").split()
    if not RELEASED_COUNT <= len(names) <= MAX_COUNT or len(names) != len(set(names)):
        raise ValueError("expected 100–128 unique fish names")
    return names


def card_box(index):
    x, y = index % COLUMNS * CARD_W, index // COLUMNS * CARD_H
    return x, y, x + CARD_W, y + CARD_H


def atlas_size(count):
    return COLUMNS * CARD_W, ((count + COLUMNS - 1) // COLUMNS) * CARD_H


def append_from(count):
    parser = ArgumentParser()
    parser.add_argument("--append-from", type=int, default=0, metavar="COUNT",
                        help="reuse the first COUNT records in the existing output")
    start = parser.parse_args().append_from
    if not 0 <= start <= count:
        parser.error("append count exceeds the catalog")
    return start
