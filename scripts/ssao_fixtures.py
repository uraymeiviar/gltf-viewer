# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.5 SSAO fixtures, written as small self-contained glTF files (shadow_fixtures.Builder: Lambert materials,
KHR_materials_specular 0, roughness 1, so the IBL diffuse term is the only light under a uniform environment).

  * box_interior: an open box, 2 x 2 m floor and three 2 m walls (back, left, right), open top and front;
  * open_plane: a 40 x 40 m floor and a 1 m cube floating 3 m above it (convex faces towards the sky);
  * halo: a 1 m green cube in front of a red 12 x 8 m wall 0.6 m behind its back face (beyond the 0.5 m radius:
    the wall must not darken around the cube's silhouette);
  * floating_cube: the 1 m cube alone, against the sky.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from shadow_fixtures import Builder  # noqa: E402

WALL = 0.05


def box_interior(path, albedo=0.8):
    b = Builder("A3.5 SSAO box interior")
    m = b.material("Box", color=(albedo, albedo, albedo, 1.0))
    b.box("Floor", (-1.0, -WALL, -1.0), (1.0, 0.0, 1.0), m)
    b.box("Back", (-1.0, 0.0, -1.0 - WALL), (1.0, 2.0, -1.0), m)
    b.box("Left", (-1.0 - WALL, 0.0, -1.0), (-1.0, 2.0, 1.0), m)
    b.box("Right", (1.0, 0.0, -1.0), (1.0 + WALL, 2.0, 1.0), m)
    return b.write(path)


def open_plane(path, albedo=0.8):
    b = Builder("A3.5 SSAO open plane")
    m = b.material("Plane", color=(albedo, albedo, albedo, 1.0))
    b.quad("Plane", (0.0, 0.0, 0.0), (40.0, 40.0), m)
    b.box("Cube", (-0.5, 3.0, -0.5), (0.5, 4.0, 0.5), b.material("Cube", color=(albedo, albedo, albedo, 1.0)))
    return b.write(path)


def halo(path):
    b = Builder("A3.5 SSAO halo")
    b.box("Wall", (-6.0, -4.0, -1.2), (6.0, 4.0, -1.1), b.material("Wall", color=(1.0, 0.0, 0.0, 1.0)))
    b.box("Cube", (-0.5, -0.5, -0.5), (0.5, 0.5, 0.5), b.material("Cube", color=(0.0, 1.0, 0.0, 1.0)))
    return b.write(path)


def floating_cube(path):
    b = Builder("A3.5 SSAO floating cube")
    b.box("Cube", (-0.5, -0.5, -0.5), (0.5, 0.5, 0.5), b.material("Cube", color=(0.8, 0.8, 0.8, 1.0)))
    return b.write(path)
