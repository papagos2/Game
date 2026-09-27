#!/usr/bin/env python3
"""Beaconhold map painter.

Paints the campaign maps procedurally (with a fixed seed) and writes them as ASCII rows into
Source/Beaconhold/Sim/BhMissionMaps.h. The generated file is committed and can also be
edited by hand. Legend (see BhMissions.cpp for the parser):

  .  grass        ,  meadow        :  dirt path     ;  sand        ~  water
  #  rock         %  blight        T  pine          Y  oak         D  dead tree
  B  beacon site (2x2)             S  sunstone (2x2)
  K  keep (4x4)   C  cottage (2x2) M  muster hall (3x3)  W watchtower (2x2)  R storehouse (2x2)
  H  gloam heart (4x4)  U  burrow (3x3)  X  hexroot (3x3)  F  thorn spire (2x2)
  w lamplighter  s shieldbearer  r ranger  k stag rider  e sage
  g gloomling    h thornback     x hexer   o bog titan
  1-9 wave spawn points

Usage: python3 Tools/MapGen/mapgen.py [--png out_dir]
"""
import math
import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_HEADER = os.path.join(ROOT, "Source", "Beaconhold", "Sim", "BhMissionMaps.h")


class Canvas:
    def __init__(self, w, h, seed):
        self.w, self.h = w, h
        self.g = [["." for _ in range(w)] for _ in range(h)]
        self.rng = random.Random(seed)
        self.locked = [[False] * w for _ in range(h)]

    def inb(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h

    def set(self, x, y, c, force=False):
        if self.inb(x, y) and (force or not self.locked[y][x]):
            self.g[y][x] = c

    def get(self, x, y):
        return self.g[y][x] if self.inb(x, y) else "#"

    def lock(self, x0, y0, x1, y1):
        for y in range(y0, y1):
            for x in range(x0, x1):
                if self.inb(x, y):
                    self.locked[y][x] = True

    def ellipse(self, cx, cy, rx, ry, c, noise=0.0, only=None):
        for y in range(int(cy - ry - 2), int(cy + ry + 3)):
            for x in range(int(cx - rx - 2), int(cx + rx + 3)):
                d = ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2
                if d <= 1.0 + self.rng.uniform(-noise, noise):
                    if only is None or self.get(x, y) in only:
                        self.set(x, y, c)

    def rect(self, x0, y0, x1, y1, c, only=None):
        for y in range(y0, y1):
            for x in range(x0, x1):
                if only is None or self.get(x, y) in only:
                    self.set(x, y, c)

    def path(self, pts, width, c, only=None, jitter=0.0):
        """Thick polyline."""
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            steps = int(max(abs(bx - ax), abs(by - ay)) * 3) + 1
            for i in range(steps + 1):
                t = i / steps
                x = ax + (bx - ax) * t
                y = ay + (by - ay) * t
                r = width / 2.0 + self.rng.uniform(-jitter, jitter)
                for yy in range(int(y - r - 1), int(y + r + 2)):
                    for xx in range(int(x - r - 1), int(x + r + 2)):
                        if (xx + 0.5 - x) ** 2 + (yy + 0.5 - y) ** 2 <= r * r:
                            if only is None or self.get(xx, yy) in only:
                                self.set(xx, yy, c)

    def forest(self, cx, cy, rx, ry, density=0.85, kinds="TTY", ground=".,"):
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                d = ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2
                p = density * (1.0 - max(0.0, d - 0.55) / 0.45) if d <= 1.0 else 0.0
                if self.rng.random() < p and self.get(x, y) in ground:
                    self.set(x, y, self.rng.choice(kinds))

    def scatter(self, c, count, area, ground=".", min_free=1):
        x0, y0, x1, y1 = area
        placed = 0
        tries = 0
        while placed < count and tries < count * 50:
            tries += 1
            x = self.rng.randrange(x0, x1)
            y = self.rng.randrange(y0, y1)
            if self.get(x, y) not in ground:
                continue
            ok = True
            for yy in range(y - min_free, y + min_free + 1):
                for xx in range(x - min_free, x + min_free + 1):
                    if self.get(xx, yy) in "KCMWRHUXFSB":
                        ok = False
            if ok:
                self.set(x, y, c)
                placed += 1

    def block(self, x, y, size, c, ground_clear=".", clear=1):
        """Places a multi-tile entity block and clears a ring around it."""
        for yy in range(y - clear, y + size + clear):
            for xx in range(x - clear, x + size + clear):
                if self.inb(xx, yy) and not (x <= xx < x + size and y <= yy < y + size):
                    if self.get(xx, yy) in "TYD#~":
                        self.set(xx, yy, ground_clear, force=True)
        for yy in range(y, y + size):
            for xx in range(x, x + size):
                self.set(xx, yy, c, force=True)
        self.lock(x - clear, y - clear, x + size + clear, y + size + clear)

    def units(self, spots, c):
        for (x, y) in spots:
            self.set(x, y, c, force=True)

    def meadows(self, count, area):
        x0, y0, x1, y1 = area
        for _ in range(count):
            cx = self.rng.uniform(x0, x1)
            cy = self.rng.uniform(y0, y1)
            self.ellipse(cx, cy, self.rng.uniform(1.5, 3.5), self.rng.uniform(1.2, 2.5), ",", noise=0.4, only=".")

    def rows(self):
        return ["".join(r) for r in self.g]


# ---------------------------------------------------------------------------------------------
# Mission 1: Kindling (tutorial) — 40 x 30
# ---------------------------------------------------------------------------------------------
def map_kindling():
    c = Canvas(40, 30, 101)
    c.meadows(10, (2, 2, 38, 28))
    # Stream from north to south with a sandy ford.
    c.path([(22, -1), (23, 6), (21, 12), (22, 18), (24, 24), (23, 31)], 2.2, "~", jitter=0.3)
    c.path([(19, 15), (27, 15)], 3.0, ";", only="~")
    c.path([(12, 19), (19, 15), (27, 15), (31, 10)], 1.4, ":", only=".,")
    # Forests
    c.forest(3, 14, 4, 12, 0.9)
    c.forest(12, 28, 12, 3.5, 0.85)
    c.forest(15, 3, 8, 3.5, 0.8)
    c.forest(33, 24, 7, 5, 0.8)
    c.forest(28, 3, 4, 3, 0.6)
    # Rocks
    c.ellipse(18, 9, 1.6, 1.2, "#", noise=0.3, only=".,")
    c.ellipse(36, 17, 1.5, 2.0, "#", noise=0.3, only=".,")
    # Gloam camp (north-east) on blight
    c.ellipse(33, 7, 7, 5.5, "%", noise=0.25)
    c.forest(38, 3, 3, 4, 0.7, kinds="D", ground="%")
    # Player base
    c.block(6, 16, 4, "K")
    c.block(14, 17, 2, "S", clear=1)
    c.block(9, 10, 2, "S", clear=1)
    c.units([(8, 21), (10, 21), (11, 18)], "w")
    # Enemy camp
    c.block(32, 5, 3, "U", ground_clear="%")
    c.units([(30, 6), (31, 9), (35, 9)], "g")
    return c


# ---------------------------------------------------------------------------------------------
# Mission 2: The Long Dusk (defense) — 48 x 40
# ---------------------------------------------------------------------------------------------
def map_long_dusk():
    c = Canvas(48, 40, 202)
    c.meadows(16, (2, 2, 46, 38))
    # River across the north with three fords.
    c.path([(-1, 9), (8, 11), (16, 9), (24, 12), (32, 10), (40, 12), (49, 10)], 2.4, "~", jitter=0.3)
    for fx in (7, 24, 41):
        c.path([(fx, 6), (fx, 15)], 3.0, ";", only="~")
    # Roads from fords to the keep
    c.path([(7, 3), (7, 14), (16, 22), (22, 26)], 1.3, ":", only=".,;")
    c.path([(24, 3), (24, 22)], 1.3, ":", only=".,;")
    c.path([(41, 3), (41, 14), (32, 22), (26, 26)], 1.3, ":", only=".,;")
    # Forests (south & flanks)
    c.forest(4, 30, 5, 9, 0.85)
    c.forest(44, 30, 5, 9, 0.85)
    c.forest(24, 38, 12, 3, 0.85)
    c.forest(14, 18, 3, 2.5, 0.7)
    c.forest(34, 18, 3, 2.5, 0.7)
    c.forest(12, 3, 3, 2.5, 0.6)
    c.forest(36, 3, 3, 2.5, 0.6)
    # Rocks funnel the approaches
    c.ellipse(17, 14, 1.5, 1.3, "#", noise=0.3, only=".,")
    c.ellipse(31, 14, 1.5, 1.3, "#", noise=0.3, only=".,")
    # North edge: blight where the Gloam come from
    c.rect(0, 0, 48, 3, "%", only=".,:")
    # Beacon sites (west and east) and a Gloam outpost guarding the east beacon
    c.block(8, 19, 2, "B", clear=1)
    c.block(38, 19, 2, "B", clear=1)
    c.ellipse(41, 22, 3.5, 3.0, "%", noise=0.2, only=".,")
    c.block(42, 23, 2, "F", ground_clear="%")
    c.units([(40, 22), (43, 21)], "g")
    # Player base
    c.block(22, 25, 4, "K")
    c.block(17, 29, 2, "S")
    c.block(29, 29, 2, "S")
    c.block(11, 23, 2, "S")
    c.units([(21, 30), (23, 30), (25, 30), (27, 30), (24, 31)], "w")
    c.units([(22, 23), (24, 23)], "s")
    c.units([(26, 23)], "r")
    # Spawn points (north corners and centre)
    c.set(2, 1, "1", force=True)
    c.set(24, 0, "2", force=True)
    c.set(45, 1, "3", force=True)
    return c


# ---------------------------------------------------------------------------------------------
# Mission 3: Heart of the Gloam (conquest) — 64 x 56
# ---------------------------------------------------------------------------------------------
def map_heart():
    c = Canvas(64, 56, 303)
    c.meadows(26, (2, 2, 62, 54))
    # Diagonal river NW -> SE with fords
    c.path([(-1, 12), (10, 18), (22, 24), (32, 27), (42, 33), (54, 40), (65, 44)], 2.6, "~", jitter=0.35)
    for (fx, fy) in ((12, 19), (32, 27), (51, 38)):
        c.ellipse(fx, fy, 2.2, 2.2, ";", only="~")
    # Roads
    c.path([(10, 46), (20, 38), (32, 27), (44, 17), (52, 10)], 1.3, ":", only=".,;")
    c.path([(10, 46), (12, 30), (12, 19), (18, 8)], 1.2, ":", only=".,;")
    c.path([(10, 46), (30, 48), (51, 38), (54, 22)], 1.2, ":", only=".,;")
    # Forests
    c.forest(3, 30, 4, 12, 0.85)
    c.forest(20, 53, 14, 3.5, 0.85)
    c.forest(60, 28, 4, 10, 0.8)
    c.forest(30, 3, 12, 3, 0.8)
    c.forest(22, 36, 4, 3, 0.75)
    c.forest(42, 22, 4, 3, 0.75)
    c.forest(6, 6, 5, 4, 0.7)
    c.forest(58, 50, 5, 4, 0.7)
    c.forest(36, 44, 3, 2.5, 0.7)
    c.forest(26, 13, 3, 2.5, 0.7)
    # Rocks
    c.ellipse(28, 34, 1.8, 1.4, "#", noise=0.3, only=".,")
    c.ellipse(38, 20, 1.6, 1.3, "#", noise=0.3, only=".,")
    c.ellipse(17, 27, 1.3, 1.6, "#", noise=0.3, only=".,")
    c.ellipse(47, 30, 1.3, 1.6, "#", noise=0.3, only=".,")
    # Gloam territory (north-east) with dead trees
    c.ellipse(51, 11, 13, 10, "%", noise=0.2)
    c.forest(60, 4, 4, 4, 0.7, kinds="D", ground="%")
    c.forest(44, 3, 4, 2.5, 0.6, kinds="D", ground="%")
    # Beacon sites
    c.block(31, 30, 2, "B", clear=1)
    c.block(14, 14, 2, "B", clear=1)
    c.block(47, 44, 2, "B", clear=1)
    # Expansions
    c.block(35, 33, 2, "S")
    c.block(18, 12, 2, "S")
    c.block(44, 47, 2, "S")
    # Player base (south-west)
    c.block(8, 43, 4, "K")
    c.block(15, 44, 2, "S")
    c.block(9, 37, 2, "S")
    c.units([(7, 48), (9, 48), (11, 48), (13, 48)], "w")
    c.units([(14, 41)], "s")
    # Gloam base
    c.block(52, 6, 4, "H", ground_clear="%")
    c.block(45, 9, 3, "U", ground_clear="%")
    c.block(54, 14, 3, "U", ground_clear="%")
    c.block(58, 10, 3, "X", ground_clear="%")
    c.block(46, 16, 2, "F", ground_clear="%")
    c.block(41, 12, 2, "F", ground_clear="%")
    c.block(51, 19, 2, "F", ground_clear="%")
    c.units([(49, 13), (50, 14), (52, 12), (56, 18)], "g")
    c.units([(48, 13)], "h")
    c.units([(57, 13)], "x")
    return c


MAPS = [("Kindling", map_kindling), ("LongDusk", map_long_dusk), ("Heart", map_heart)]


def write_header(maps):
    lines = [
        "// GENERATED by Tools/MapGen/mapgen.py — may be edited by hand.",
        "// Beaconhold simulation core — campaign map layouts (see legend in BhMissions.cpp).",
        "#pragma once",
        "",
        "namespace bh",
        "{",
        "namespace maps",
        "{",
    ]
    for name, canvas in maps:
        lines.append(f"inline const char* const {name}[] = {{")
        for r in canvas.rows():
            lines.append(f'\t"{r}",')
        lines.append("};")
        lines.append(f"inline constexpr int {name}Rows = {canvas.h};")
        lines.append("")
    lines.append("} // namespace maps")
    lines.append("} // namespace bh")
    with open(OUT_HEADER, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


COLORS = {
    ".": (118, 166, 82), ",": (140, 180, 90), ":": (170, 140, 96), ";": (218, 200, 140),
    "~": (70, 130, 190), "#": (120, 120, 128), "%": (96, 70, 120),
    "T": (40, 96, 60), "Y": (70, 120, 50), "D": (70, 55, 70),
    "B": (240, 220, 140), "S": (255, 200, 40),
    "K": (60, 110, 230), "C": (90, 140, 240), "M": (60, 90, 200), "W": (80, 120, 220), "R": (100, 150, 230),
    "H": (200, 40, 160), "U": (170, 50, 140), "X": (150, 60, 170), "F": (190, 70, 120),
}


def write_png(canvas, path, scale=8):
    try:
        from PIL import Image
    except ImportError:
        print("PIL not available; skipping PNG", file=sys.stderr)
        return
    img = Image.new("RGB", (canvas.w * scale, canvas.h * scale))
    px = img.load()
    for y, row in enumerate(canvas.rows()):
        for x, ch in enumerate(row):
            col = COLORS.get(ch)
            if col is None:
                col = (255, 255, 255) if ch in "wsrke" else (255, 60, 60) if ch in "ghxo" else (255, 140, 0)
            for yy in range(scale):
                for xx in range(scale):
                    px[x * scale + xx, y * scale + yy] = col
    img.save(path)


def main():
    maps = [(name, fn()) for name, fn in MAPS]
    write_header(maps)
    print(f"wrote {OUT_HEADER}")
    if len(sys.argv) > 2 and sys.argv[1] == "--png":
        os.makedirs(sys.argv[2], exist_ok=True)
        for name, canvas in maps:
            write_png(canvas, os.path.join(sys.argv[2], f"{name}.png"))
    for name, canvas in maps:
        print(f"\n{name} ({canvas.w}x{canvas.h})")
        for r in canvas.rows():
            print(r)


if __name__ == "__main__":
    main()
