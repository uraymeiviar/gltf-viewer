# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.9 sky occlusion fixtures, written as small self-contained glTF files (shadow_fixtures.Builder: Lambert materials,
KHR_materials_specular 0, roughness 1, so under a uniform environment the IBL diffuse term is the only light and the
ratio of a pixel with and without sky occlusion is its cosine-weighted sky visibility).

  * open_box: a closed box with one open side, the top: a 2 x 2 m floor and four 2 m walls, 5 cm thick, opening 2 x 2 m;
  * occluder_box: the same box closed by a 2.1 x 2.1 m lid quad with ASOBO_material_environment_occluder (hidden in the
    image, blocks the sky in the bake);
  * thin_box: a closed 2 m box of zero-thickness double-sided quads, its lid an environment occluder (light leaking
    through walls thinner than a cell);
  * open_plane: a 20 x 20 m floor alone;
  * sphere: the A3.1 white Lambert sphere (validate_sun.lambert), radius 1.
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from shadow_fixtures import Builder  # noqa: E402

WALL = 0.05
HALF = 1.0     # inner half-width of the boxes
HEIGHT = 2.0   # inner height
OCCLUDER = {"ASOBO_material_environment_occluder": {"enabled": True}}


def _walls(b, m):
    b.box("Floor", (-HALF - WALL, -WALL, -HALF - WALL), (HALF + WALL, 0.0, HALF + WALL), m)
    b.box("Back", (-HALF - WALL, 0.0, -HALF - WALL), (HALF + WALL, HEIGHT, -HALF), m)
    b.box("Front", (-HALF - WALL, 0.0, HALF), (HALF + WALL, HEIGHT, HALF + WALL), m)
    b.box("Left", (-HALF - WALL, 0.0, -HALF), (-HALF, HEIGHT, HALF), m)
    b.box("Right", (HALF, 0.0, -HALF), (HALF + WALL, HEIGHT, HALF), m)


def open_box(path, albedo=0.8):
    b = Builder("A3.9 sky occlusion open box")
    _walls(b, b.material("Box", color=(albedo, albedo, albedo, 1.0)))
    return b.write(path)


def occluder_box(path, albedo=0.8):
    b = Builder("A3.9 sky occlusion occluder box")
    _walls(b, b.material("Box", color=(albedo, albedo, albedo, 1.0)))
    b.quad("Lid", (0.0, HEIGHT, 0.0), (2 * (HALF + WALL), 2 * (HALF + WALL)), b.material("Lid", double_sided=True, extensions=OCCLUDER))
    return b.write(path)


def thin_box(path, albedo=0.8):
    """Zero-thickness walls: every face a double-sided quad; the lid an environment occluder."""
    b = Builder("A3.9 sky occlusion thin box")
    m = b.material("Thin", color=(albedo, albedo, albedo, 1.0), double_sided=True)
    b.quad("Floor", (0.0, 0.0, 0.0), (2 * HALF, 2 * HALF), m)
    h, y = HALF, HEIGHT
    walls = {"Back": [(-h, 0, -h), (h, 0, -h), (h, y, -h), (-h, y, -h)],
             "Front": [(-h, 0, h), (-h, y, h), (h, y, h), (h, 0, h)],
             "Left": [(-h, 0, -h), (-h, y, -h), (-h, y, h), (-h, 0, h)],
             "Right": [(h, 0, -h), (h, 0, h), (h, y, h), (h, y, -h)]}
    for name, corners in walls.items():
        ax, ay, az = (corners[1][i] - corners[0][i] for i in range(3))
        bx, by, bz = (corners[3][i] - corners[0][i] for i in range(3))
        n = (ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx)
        length = math.sqrt(sum(c * c for c in n))
        normal = tuple(c / length for c in n)
        b.mesh(name, corners, [normal] * 4, [(0, 0), (1, 0), (1, 1), (0, 1)], [0, 1, 2, 0, 2, 3], m)
    b.quad("Lid", (0.0, HEIGHT, 0.0), (2 * HALF, 2 * HALF), b.material("Lid", double_sided=True, extensions=OCCLUDER))
    return b.write(path)


def open_plane(path, albedo=0.8):
    b = Builder("A3.9 sky occlusion open plane")
    b.quad("Plane", (0.0, 0.0, 0.0), (20.0, 20.0), b.material("Plane", color=(albedo, albedo, albedo, 1.0)))
    return b.write(path)


def opening_form_factor(half, distance):
    """Cosine-weighted fraction of the sky seen through a square opening of half-width `half` centred `distance` above
    a point (the point-to-parallel-rectangle form factor, four quadrants of half x half)."""
    a = half / distance
    s = math.sqrt(1.0 + a * a)
    quadrant = (1.0 / (2.0 * math.pi)) * 2.0 * (a / s) * math.atan(a / s)
    return 4.0 * quadrant
