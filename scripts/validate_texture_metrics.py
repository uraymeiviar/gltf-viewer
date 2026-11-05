# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""The Texture Distortion and Texture Density debug views (docs/TEXTURE_METRICS.md).

The views show the FSS Blender heatmap's measures (fss_blender_tools/uv_heatmap.py). The fixture is a grid of quads in
the XY plane, looked at along -Z, each with hand-computed answers:
  * shear (conformal ratio sigma1 / sigma2): 1, 1.5, 2, 3; a golden-ratio-squared 2.618 triangle; both views' colours
    (white to blue, full at 2.0);
  * area (the face against its UV island, folded): 1.5 and 1.333 on an island welded across split vertices; 1 when
    the same triangles are cut by a UV seam (two islands);
  * density (UV area / 3D area over the material's area-weighted median, read against 0.9 to 1.1, log2 scale): 0.278,
    0.833, 1, 1, 1.364, 2.727, and pixels per metre from the texture size (square and not);
  * magenta for a face without UV area, grey for a primitive without UVs;
  * TEXCOORD_n of the material's base colour, KHR_texture_transform and ASOBO_material_UV_options scale, node scale
    (uniform: density only; non-uniform: shear too), one mesh under nodes of different scale (draws must not merge),
    triangle strips, several primitives in one mesh (table offsets), EXT_mesh_gpu_instancing (first instance's scale),
    a skinned mesh (joint scale, the node's own transform ignored);
  * a packaged ASOBO_asset_optimized file: half-float UVs, a shared index accessor sliced by ASOBO_primitive.
The UV grid and cell labels, with the add-on's cell layout: every font pixel of each label read back at its computed
place (A1 at the image's lower left, two-letter columns, two-digit rows, tiled UVs, KHR_texture_transform, TEXCOORD_1),
the colours of both kinds of line and of each label, 8 to 64 cells, labels off, labels gone below 22 pixel cells, heat and grid opacity, no grid without UVs.
Also: both views byte-identical between the CLI flag and the pipe; the table is built only while a view shows and is
freed after; get_state.textureMetrics; texture_distortion_view; refused names; zero GL errors; shaded and another debug
view byte-identical to the baseline binary (--baseline-exe) and after a visit to the views.

Run: python scripts/validate_texture_metrics.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import atexit
import base64
import io
import json
import math
import os
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
from PIL import Image  # noqa: E402
from validate_asobo_common import COMMON, EXE, REPO, check, finish, render, same  # noqa: E402
from validate_sun import Camera  # noqa: E402

os.environ.setdefault("GLIDER_NO_FOCUS", "1")
OUT = REPO / "build" / "texture_metrics"
LOOK = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--aa", "none",
        "--camera-target", "17.5,14,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "60"]
NO_UV_GRID = ["--texture-grid", "off"]  # the colours' known answers read the faces without the grid's lines and labels
HEAT = LOOK + NO_UV_GRID
GRID_BASE = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--aa", "none",
             "--camera-target", "3,1.5,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "8"]
GRID_LOOK = GRID_BASE + ["--texture-grid-opacity", "1"]  # the known answers at full strength; the default is checked apart
TOLERANCE = 2  # 8-bit levels; the table holds half floats

NEUTRAL = (0.92, 0.92, 0.92)
BLUE = (0.10, 0.35, 0.95)
RED = (0.95, 0.15, 0.15)
GREEN = (0.15, 0.80, 0.30)
CYAN = (0.10, 0.80, 0.95)
ERROR = (1.0, 0.0, 1.0)
NO_DATA = (0.30, 0.30, 0.30)


# The labels' font, as in pbr.frag: seven rows of five bits, top row first.
FONT = {
    "0": (0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E),
    "1": (0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "2": (0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F),
    "3": (0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E),
    "4": (0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02),
    "5": (0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E),
    "6": (0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E),
    "7": (0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08),
    "8": (0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E),
    "9": (0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C),
    "A": (0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11),
    "B": (0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E),
    "C": (0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E),
    "D": (0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C),
    "E": (0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F),
    "F": (0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10),
    "G": (0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F),
    "H": (0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    "I": (0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "J": (0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C),
    "K": (0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11),
    "L": (0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F),
    "M": (0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11),
    "N": (0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11),
    "O": (0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    "P": (0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10),
    "Q": (0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D),
    "R": (0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11),
    "S": (0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E),
    "T": (0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04),
    "U": (0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    "V": (0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04),
    "W": (0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A),
    "X": (0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11),
    "Y": (0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04),
    "Z": (0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F),
}
LINE_U = (0.32, 0.00, 0.60)  # lines of constant U
LINE_V = (0.90, 0.42, 0.00)  # lines of constant V
INKS = ((0.03, 0.03, 0.03), (0.00, 0.30, 0.32), (0.34, 0.22, 0.00), (0.50, 0.00, 0.24))  # labels, by (col + 2 row) % 4
GLYPH_HEIGHT = 0.26  # share of the cell, the add-on's
GLYPH_WIDTH = GLYPH_HEIGHT * 0.62
GLYPH_MARGIN = 0.07


def cell_label(col, row):
    letters = chr(65 + col) if col < 26 else chr(65 + col // 26 - 1) + chr(65 + col % 26)
    return letters + str(row + 1)


def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def surface(colour):
    """The heatmap's surface at the default opacity over the grey base; these faces look straight at the camera."""
    return tuple(255.0 * c for c in lerp((0.8, 0.8, 0.8), colour, 0.75))


def distortion(shear, area, view):
    s = min(max(shear - 1.0, 0.0), 1.0) if view != "area" else 0.0
    a = min(max(area - 1.0, 0.0), 1.0) if view != "shear" else 0.0
    return surface(tuple((1 - s) * (1 - a) * n + s * (1 - a) * b + (1 - s) * a * g + s * a * c
                         for n, b, g, c in zip(NEUTRAL, BLUE, GREEN, CYAN)))


def density(relative):
    t = min(max(math.log2(relative), -1.0), 1.0)
    return surface(lerp(NEUTRAL, BLUE, t) if t >= 0 else lerp(NEUTRAL, RED, -t))


def png(width, height):
    image = Image.new("RGB", (width, height), (200, 180, 160))
    data = io.BytesIO()
    image.save(data, "PNG")
    return "data:image/png;base64," + base64.b64encode(data.getvalue()).decode()


class Fixture:
    def __init__(self):
        self.gltf = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": []}], "nodes": [], "meshes": [],
                     "materials": [], "accessors": [], "bufferViews": [], "buffers": [], "extensionsUsed": []}
        self.blob = bytearray()
        self.faces = []  # (name, point, shear, area, relative density or None, special colour or None)

    def accessor(self, data, component, kind, count, target=34962, **extra):
        while len(self.blob) % 4:
            self.blob.append(0)
        view = {"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        self.gltf["bufferViews"].append(view)
        self.blob += data
        self.gltf["accessors"].append({"bufferView": len(self.gltf["bufferViews"]) - 1, "componentType": component,
                                       "type": kind, "count": count, **extra})
        return len(self.gltf["accessors"]) - 1

    def floats(self, rows, kind):
        flat = [v for row in rows for v in row]
        extra = {}
        if kind == "VEC3":
            extra = {"min": [min(r[i] for r in rows) for i in range(3)], "max": [max(r[i] for r in rows) for i in range(3)]}
        return self.accessor(struct.pack(f"<{len(flat)}f", *flat), 5126, kind, len(rows), **extra)

    def material(self, name, texture=None, tex_coord=None, transform=None, extensions=None):
        material = {"name": name, "doubleSided": True, "pbrMetallicRoughness": {"metallicFactor": 0.0}}
        if texture is not None:
            gltf = self.gltf
            gltf.setdefault("images", []).append({"uri": png(*texture)})
            gltf.setdefault("textures", []).append({"source": len(gltf["images"]) - 1})
            info = {"index": len(gltf["textures"]) - 1}
            if tex_coord is not None:
                info["texCoord"] = tex_coord
            if transform is not None:
                info["extensions"] = {"KHR_texture_transform": transform}
                self.declare("KHR_texture_transform")
            material["pbrMetallicRoughness"]["baseColorTexture"] = info
        if extensions:
            material["extensions"] = extensions
            for name_ in extensions:
                self.declare(name_)
        self.gltf["materials"].append(material)
        return len(self.gltf["materials"]) - 1

    def declare(self, name):
        if name not in self.gltf["extensionsUsed"]:
            self.gltf["extensionsUsed"].append(name)

    def primitive(self, positions, uvs, material, indices=None, mode=None, uv1=None, extra_attributes=None):
        attributes = {"POSITION": self.floats(positions, "VEC3")}
        if uvs is not None:
            attributes["TEXCOORD_0"] = self.floats(uvs, "VEC2")
        if uv1 is not None:
            attributes["TEXCOORD_1"] = self.floats(uv1, "VEC2")
        attributes.update(extra_attributes or {})
        primitive = {"attributes": attributes, "material": material}
        if indices is not None:
            primitive["indices"] = self.accessor(struct.pack(f"<{len(indices)}H", *indices), 5123, "SCALAR", len(indices), target=34963)
        if mode is not None:
            primitive["mode"] = mode
        return primitive

    def mesh(self, *primitives):
        self.gltf["meshes"].append({"primitives": list(primitives)})
        return len(self.gltf["meshes"]) - 1

    def node(self, name, mesh=None, translation=(0, 0, 0), scale=None, root=True, **extra):
        node = {"name": name, "translation": list(translation), **extra}
        if mesh is not None:
            node["mesh"] = mesh
        if scale is not None:
            node["scale"] = list(scale)
        self.gltf["nodes"].append(node)
        index = len(self.gltf["nodes"]) - 1
        if root:
            self.gltf["scenes"][0]["nodes"].append(index)
        return index

    def write(self, path):
        self.gltf["buffers"] = [{"byteLength": len(self.blob), "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(self.blob)).decode()}]
        if not self.gltf["extensionsUsed"]:
            del self.gltf["extensionsUsed"]
        path.write_text(json.dumps(self.gltf), encoding="utf-8")


def quad(half_x, half_y):
    return [(-half_x, -half_y, 0), (half_x, -half_y, 0), (half_x, half_y, 0), (-half_x, half_y, 0)]


def rect(a, b):
    return [(0, 0), (a, 0), (a, b), (0, b)]


QUAD = [0, 1, 2, 0, 2, 3]


def cell(col, row):
    return (col * 7.0, row * 7.0, 0.0)


def add(f, name, col, row, shear=1.0, area=1.0, relative=None, colour=None, offset=(0.0, 0.0)):
    x, y, _ = cell(col, row)
    f.faces.append((name, (x + offset[0], y + offset[1], 0.0), shear, area, relative, colour))


def build_main(path):
    f = Fixture()
    # Row 0, material "density": 6 x 6 m reference at UV 1 x 1 (1/36 UV per square metre, 36 m2 of 60 m2, so it is the
    # median), small quads of 4 m2 at k x the reference. Each quad is its own island.
    m1 = f.material("density", texture=(256, 256))
    f.node("density_reference", f.mesh(f.primitive(quad(3, 3), rect(1, 1), m1, QUAD)), cell(5, 4))
    add(f, "density reference", 5, 4, relative=1.0)
    for col, k in enumerate((0.25, 0.75, 0.95, 1.05, 1.5, 3.0)):
        side = math.sqrt(k) / 3.0
        f.node(f"density_{k}", f.mesh(f.primitive(quad(1, 1), rect(side, side), m1, QUAD)), cell(col, 0))
        relative = k / 0.9 if k < 0.9 else k / 1.1 if k > 1.1 else 1.0
        add(f, f"density k={k}", col, 0, relative=relative)

    # Row 1, material "shear": 2 x 2 m quads; an island of two triangles sharing a diagonal through split vertices.
    m2 = f.material("shear")
    for col, (a, b, shear) in enumerate(((0.2, 0.2, 1.0), (0.3, 0.2, 1.5), (0.4, 0.2, 2.0), (0.6, 0.2, 3.0))):
        f.node(f"shear_{shear}", f.mesh(f.primitive(quad(1, 1), rect(a, b), m2, QUAD)), cell(col, 1))
        add(f, f"shear {shear}", col, 1, shear=shear)
    q = quad(1, 1)
    welded = [q[0], q[1], q[2], q[0], q[2], q[3]]
    golden = 1.5 + 0.5 * math.sqrt(5.0)
    f.node("island", f.mesh(f.primitive(welded, [(0, 0), (.2, 0), (.2, .2), (0, 0), (.2, .2), (0, .4)], m2)), cell(4, 1))
    add(f, "island triangle 1", 4, 1, shear=1.0, area=1.5, offset=(0.5, -0.5))
    add(f, "island triangle 2", 4, 1, shear=golden, area=4.0 / 3.0, offset=(-0.5, 0.5))
    f.node("seam", f.mesh(f.primitive(welded, [(0, 0), (.2, 0), (.2, .2), (.5, .5), (.7, .7), (.5, .9)], m2)), cell(5, 1))
    add(f, "seam triangle 1", 5, 1, shear=1.0, area=1.0, offset=(0.5, -0.5))
    add(f, "seam triangle 2", 5, 1, shear=golden, area=1.0, offset=(-0.5, 0.5))

    # Row 2: no UV area, no UVs, the texture's own UV set and scale.
    f.node("error", f.mesh(f.primitive(quad(1, 1), [(0, 0), (.5, 0), (1, 0), (.25, 0)], m2, QUAD)), cell(0, 2))
    add(f, "no UV area", 0, 2, colour=surface(ERROR))
    m_plain = f.material("plain")
    f.node("no_uvs", f.mesh(f.primitive(quad(1, 1), None, m_plain, QUAD)), cell(1, 2))
    add(f, "no UVs", 1, 2, colour=surface(NO_DATA))
    m_khr = f.material("khr", texture=(64, 64), transform={"scale": [2.0, 1.0]})
    f.node("khr_scale", f.mesh(f.primitive(quad(1, 1), rect(.2, .2), m_khr, QUAD)), cell(2, 2))
    add(f, "KHR_texture_transform scale 2 x 1", 2, 2, shear=2.0)
    m_uv1 = f.material("uv1", texture=(64, 64), tex_coord=1)
    f.node("uv1", f.mesh(f.primitive(quad(1, 1), rect(.4, .2), m_uv1, QUAD, uv1=rect(.2, .2))), cell(3, 2))
    add(f, "TEXCOORD_1 of the base colour (TEXCOORD_0 stretched)", 3, 2, shear=1.0)
    m_uv0 = f.material("uv0", texture=(64, 64))
    f.node("uv0", f.mesh(f.primitive(quad(1, 1), rect(.4, .2), m_uv0, QUAD, uv1=rect(.2, .2))), cell(4, 2))
    add(f, "TEXCOORD_0 of the base colour (TEXCOORD_1 square)", 4, 2, shear=2.0)
    m_asobo = f.material("asobo_uv", texture=(64, 64), extensions={"ASOBO_material_UV_options": {"UVTilingU": 2.0}})
    f.node("asobo_uv", f.mesh(f.primitive(quad(1, 1), rect(.2, .2), m_asobo, QUAD)), cell(5, 2))
    add(f, "ASOBO_material_UV_options tiling 2 x 1", 5, 2, shear=2.0)

    # Row 3: one mesh under nodes of different scale. Four times the area at scale 2, so that node sets the median.
    m_scale = f.material("scale", texture=(128, 128))
    shared = f.mesh(f.primitive(quad(1, 1), rect(.2, .2), m_scale, QUAD))
    f.node("scale_1", shared, cell(0, 3))
    add(f, "scale 1 under a median at scale 2", 0, 3, relative=4.0 / 1.1)
    f.node("scale_2", shared, cell(1, 3), scale=(2, 2, 1))
    add(f, "uniform scale 2", 1, 3, relative=1.0)
    m_aniso = f.material("aniso")
    shared2 = f.mesh(f.primitive(quad(1, 1), rect(.2, .2), m_aniso, QUAD))
    f.node("aniso_1", shared2, cell(2, 3))
    add(f, "same mesh, scale 1", 2, 3, shear=1.0)
    f.node("aniso_2", shared2, cell(3, 3), scale=(1.5, 1, 1))
    add(f, "same mesh, non-uniform scale 1.5 x 1", 3, 3, shear=1.5)
    # A triangle strip: (p00, p10, p01) then (p10, p01, p11), one island.
    strip = f.primitive([q[0], q[1], q[3], q[2]], [(0, 0), (.2, 0), (0, .2), (.6, .2)], m2, mode=5)
    f.node("strip", f.mesh(strip), cell(4, 3))
    add(f, "strip triangle 1", 4, 3, shear=1.0, area=2.0, offset=(-0.5, -0.5))
    add(f, "strip triangle 2", 4, 3, shear=4.443, area=1.5, offset=(0.5, 0.5))
    # Two primitives of one mesh, different materials: table offsets.
    left = [(x - 0.5, y, z) for x, y, z in quad(0.5, 1)]
    right = [(x + 0.5, y, z) for x, y, z in quad(0.5, 1)]
    f.node("two_primitives", f.mesh(f.primitive(left, rect(.1, .2), m2, QUAD),
                                    f.primitive(right, [(.5 + u, v) for u, v in rect(.2, .2)], m_aniso, QUAD)), cell(5, 3))
    add(f, "first primitive", 5, 3, shear=1.0, offset=(-0.5, 0))
    add(f, "second primitive", 5, 3, shear=2.0, offset=(0.5, 0))

    # Row 4: EXT_mesh_gpu_instancing and a skin, for the reference and pixels per metre.
    m_gpu = f.material("gpu_instanced", texture=(512, 128))
    gpu = f.mesh(f.primitive(quad(.5, .5), rect(1, 1), m_gpu, QUAD))
    translations = f.accessor(struct.pack("<6f", -1.2, 0, 0, 1.2, 0, 0), 5126, "VEC3", 2, target=None)
    scales = f.accessor(struct.pack("<6f", 2, 2, 1, 2, 2, 1), 5126, "VEC3", 2, target=None)
    f.declare("EXT_mesh_gpu_instancing")
    f.node("gpu", gpu, cell(0, 4), extensions={"EXT_mesh_gpu_instancing": {"attributes": {"TRANSLATION": translations, "SCALE": scales}}})
    add(f, "EXT_mesh_gpu_instancing", 0, 4, offset=(-1.2, 0))

    m_skin = f.material("skinned", texture=(256, 256))
    joints = f.accessor(bytes([0, 0, 0, 0] * 4), 5121, "VEC4", 4)
    weights = f.floats([(1, 0, 0, 0)] * 4, "VEC4")
    skinned = f.mesh(f.primitive(quad(.5, .5), rect(1, 1), m_skin, QUAD, extra_attributes={"JOINTS_0": joints, "WEIGHTS_0": weights}))
    identity = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]
    ibm = f.accessor(struct.pack("<16f", *identity), 5126, "MAT4", 1, target=None)
    joint = f.node("joint", translation=cell(2, 4), scale=(2, 2, 1))
    f.gltf.setdefault("skins", []).append({"joints": [joint], "inverseBindMatrices": ibm})
    f.node("skinned", skinned, (100, 100, 0), skin=0)  # glTF: a skinned mesh ignores its node's transform
    add(f, "skinned", 2, 4)
    f.write(path)
    return f


def grid_uv(col, row, cells, scale=1.0, shift=(0.0, 0.0)):
    """UVs of the quad's corners that show one cell: the grid counts in Blender's UVs, V up, so glTF's V is 1 - V."""
    corners = [(col, row), (col + 1, row), (col + 1, row + 1), (col, row + 1)]
    return [((c / cells + shift[0]) / scale, (1.0 - r / cells + shift[1]) / scale) for c, r in corners]


GRID_SPOTS = [(0, 0), (3, 0), (6, 0), (0, 3), (3, 3), (6, 3)]  # quad centres: (x, y) in metres


def build_grid(path, cells, entries):
    """One 2 x 2 m quad per cell shown, big enough to read a label's font pixels. entries: (col, row, how)."""
    f = Fixture()
    spots = []
    for index, (col, row, how) in enumerate(entries):
        x, y = GRID_SPOTS[index]
        if how == "khr":
            material = f.material(f"grid_{index}", texture=(64, 64), transform={"scale": [2.0, 2.0]})
            primitive = f.primitive(quad(1, 1), grid_uv(col, row, cells, scale=2.0), material, QUAD)
        elif how == "uv1":
            material = f.material(f"grid_{index}", texture=(64, 64), tex_coord=1)
            primitive = f.primitive(quad(1, 1), rect(.1, .1), material, QUAD, uv1=grid_uv(col, row, cells))
        elif how == "tiled":
            material = f.material(f"grid_{index}")
            primitive = f.primitive(quad(1, 1), grid_uv(col, row, cells, shift=(2.0, -3.0)), material, QUAD)
        else:
            material = f.material(f"grid_{index}")
            primitive = f.primitive(quad(1, 1), grid_uv(col, row, cells), material, QUAD)
        f.node(f"grid_{index}", f.mesh(primitive), (x, y, 0))
        spots.append((x, y, col, row, how))
    f.write(path)
    return spots


def label_answers(col, row):
    """(cell fraction x, y, lit) at the middle of each font pixel of the cell's label, Blender's V up."""
    text = cell_label(col, row)
    for slot in range(4):
        rows = FONT[text[slot]] if slot < len(text) else (0,) * 7
        for y in range(7):
            for x in range(5):
                qx = slot + (x + 0.5) / 6.0
                qy = 1.0 - (y + 1.0) / 8.0
                yield (GLYPH_MARGIN + GLYPH_WIDTH * qx, 1.0 - GLYPH_MARGIN - GLYPH_HEIGHT + GLYPH_HEIGHT * qy,
                       bool((rows[y] >> (4 - x)) & 1))


def read_labels(image, camera, spots, base, strength=1.0, labels=True, what=""):
    for x, y, col, row, how in spots:
        lit_colour = lerp(base, tuple(255 * c for c in INKS[(col + 2 * row) % 4]), 0.9 * strength)
        wrong = 0
        lit = 0
        for fx, fy, on in label_answers(col, row):
            expected = lit_colour if on and labels else base
            lit += bool(on)
            got = pixel(image, camera, (x - 1 + 2 * fx, y - 1 + 2 * fy, 0))
            wrong += not all(abs(g - e) <= TOLERANCE for g, e in zip(got, expected))
        text = cell_label(col, row)
        check(wrong == 0 and lit > 0, f"{what}cell {text}{' (' + how + ')' if how else ''}: {'its label reads ' + text if labels else 'no label'}, all 140 font pixels ({wrong} wrong)")


def check_line(image, camera, centre, base, strength=1.0, what=""):
    """A quad two cells across has a line of constant U and one of constant V through its middle, one pixel wide. Each is
    read away from their crossing and the labels: the darkest of the three pixels across the line, then the surface beside it."""
    x, y = centre
    for name, colour, point, step in (("constant U", LINE_U, (x, y - 0.5, 0), (1, 0)), ("constant V", LINE_V, (x + 0.5, y, 0), (0, 1))):
        line = lerp(base, tuple(255 * c for c in colour), 0.85 * strength)
        cx, cy = camera.project(point)
        cx, cy = int(round(cx)), int(round(cy))
        across = [image.getpixel((cx + d * step[0], cy + d * step[1]))[:3] for d in (-1, 0, 1)]
        got = min(across, key=lambda p: sum(abs(g - e) for g, e in zip(p, line)))
        check(all(abs(g - e) <= TOLERANCE for g, e in zip(got, line)), f"{what}line of {name}: {tuple(round(e) for e in line)} (got {got})")
        clear = image.getpixel((cx + 12 * step[0], cy + 12 * step[1]))[:3]
        check(all(abs(g - e) <= TOLERANCE for g, e in zip(clear, base)), f"{what}beside it, the surface: {tuple(round(e) for e in base)} (got {clear})")


def blended(under, colour, alpha):
    return tuple(u + (255.0 * c - u) * alpha for u, c in zip(under, colour))


def check_overlay(eight, spots, plain_look):
    """The overlay blends over whatever the view drew: each answer is computed from the same pixel with the overlay off."""
    print("UV grid overlay in other views")
    camera = Camera(json.loads((OUT / "grid_8.json").read_text(encoding="utf-8")))
    views = (("shaded post", []), ("shaded", ["--render-mode", "shaded"]), ("wireframe", ["--render-mode", "wireframe"]),
             ("normals", ["--debug-view", "geometrynormal"]), ("UVs", ["--debug-view", "texturecoordinates0"]))
    for name, flags in views:
        safe = name.replace(" ", "_")
        off_path = render(EXE, eight, f"overlay_off_{safe}", plain_look + flags, OUT)
        on_path = render(EXE, eight, f"overlay_8_{safe}", plain_look + flags + ["--uv-grid-overlay", "--texture-grid-cells", "8"], OUT)
        line_path = render(EXE, eight, f"overlay_16_{safe}", plain_look + flags + ["--uv-grid-overlay"], OUT)
        if not check(off_path is not None and on_path is not None and line_path is not None, f"{name}: captured"):
            continue
        off = Image.open(off_path).convert("RGB")
        on = Image.open(on_path).convert("RGB")
        wrong = 0
        total = 0
        for x, y, col, row, how in spots[:2]:
            ink = INKS[(col + 2 * row) % 4]
            for fx, fy, lit in label_answers(col, row):
                point = (x - 1 + 2 * fx, y - 1 + 2 * fy, 0)
                expected = blended(pixel(off, camera, point), ink, 0.9 * 0.5) if lit else pixel(off, camera, point)
                got = pixel(on, camera, point)
                total += 1
                wrong += not all(abs(g - e) <= TOLERANCE for g, e in zip(got, expected))
        check(wrong == 0, f"{name}: both labels at the default opacity, over the view's own pixels, {total} font pixels ({wrong} wrong)")
        lines = Image.open(line_path).convert("RGB")
        for label, colour, point, step in (("constant U", LINE_U, (0, -0.5, 0), (1, 0)), ("constant V", LINE_V, (0.5, 0, 0), (0, 1))):
            cx, cy = camera.project(point)
            cx, cy = int(round(cx)), int(round(cy))
            best = None
            for d in (-1, 0, 1):
                where = (cx + d * step[0], cy + d * step[1])
                expected = blended(off.getpixel(where)[:3], colour, 0.85 * 0.5)
                error = max(abs(g - e) for g, e in zip(lines.getpixel(where)[:3], expected))
                best = error if best is None else min(best, error)
            check(best <= TOLERANCE, f"{name}: the line of {label} at the default opacity (off by {best:.1f})")
    plain = render(EXE, eight, "overlay_view_plain", plain_look + ["--debug-view", "texturedensity"], OUT)
    flagged = render(EXE, eight, "overlay_view_flagged", plain_look + ["--debug-view", "texturedensity", "--uv-grid-overlay"], OUT)
    check(same(plain, flagged), "the texture views keep their own grid: the overlay adds nothing to them")

    main_model = OUT / "texture_metrics.gltf"
    off_path = render(EXE, main_model, "overlay_uvless_off", LOOK, OUT)
    on_path = render(EXE, main_model, "overlay_uvless_on", LOOK + ["--uv-grid-overlay", "--texture-grid-cells", "8"], OUT)
    main_camera = Camera(json.loads((OUT / "overlay_uvless_on.json").read_text(encoding="utf-8")))
    off, on = Image.open(off_path).convert("RGB"), Image.open(on_path).convert("RGB")
    x, y, _ = cell(1, 2)
    points = [(x + fx, y + fy, 0.0) for fx in (-0.9, 0.0, 0.9) for fy in (-0.9, 0.0, 0.9)]
    check(all(pixel(off, main_camera, p) == pixel(on, main_camera, p) for p in points), "a primitive without UVs has no overlay")

    print("Overlay switches and defaults")
    session = Session(eight, plain_look)
    state = session.call("get_viewport_setting", key="uv_grid_overlay")
    check(state.get("value") is False, f"uv_grid_overlay is off by default ({state})")
    opacity = session.call("get_viewport_setting", key="texture_grid_opacity")
    check(abs(opacity.get("value") - 0.5) < 1e-6, f"grid opacity 0.5 by default ({opacity})")
    colour = session.call("get_viewport_setting", key="wireframe_color")
    check(abs(colour.get("value")[3] - 0.5) < 1e-6, f"wireframe overlay opacity 0.5 by default ({colour})")
    session.set("uv_grid_overlay", True)
    check(same(session.capture("pipe_overlay_on"), OUT / "overlay_16_shaded_post.png"), "uv_grid_overlay on equals the flag")
    check(session.metrics()["built"] is False, "the overlay builds no table")
    session.set("uv_grid_overlay", False)
    check(same(session.capture("pipe_overlay_off"), OUT / "overlay_off_shaded_post.png"), "off again equals a run without it")
    errors = session.call("get_state").get("glErrors")
    check(errors == 0, f"zero GL errors ({errors})")
    session.close()

    wire_default = render(EXE, eight, "wire_default", plain_look + ["--wireframe-overlay"], OUT)
    wire_half = render(EXE, eight, "wire_half", plain_look + ["--wireframe-overlay", "--set", "wireframe_color=1,1,1,0.5"], OUT)
    wire_full = render(EXE, eight, "wire_full", plain_look + ["--wireframe-overlay", "--set", "wireframe_color=1,1,1,1"], OUT)
    check(same(wire_default, wire_half) and not same(wire_default, wire_full), "the wireframe overlay's default opacity is 0.5")

    toggled = OUT / "overlay_ui_on.png"
    toggled.unlink(missing_ok=True)
    script = OUT / "overlay_ui.txt"
    script.write_text("\n".join(["wait 24", "clickp vp.toolbar.uv_grid 1", "wait 8", f"capture {toggled.as_posix()}", "wait 2"]) + "\n", encoding="utf-8")
    report = OUT / "overlay_ui.json"
    report.unlink(missing_ok=True)
    subprocess.run([str(EXE), str(eight)] + COMMON + plain_look + ["--ui-script", str(script), "--ui-report", str(report)], cwd=REPO,
                   capture_output=True, text=True, timeout=240)
    ui = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {"failed": True, "errors": ["no report"]}
    check(not ui["failed"] and same(toggled, OUT / "overlay_16_shaded_post.png"), f"the toolbar button equals the flag ({ui['errors']})")


def check_grid():
    base = surface(NEUTRAL)
    eight = OUT / "grid_8.gltf"
    spots_8 = build_grid(eight, 8, [(0, 0, ""), (2, 1, ""), (7, 7, ""), (0, 0, "tiled"), (2, 1, "khr"), (3, 2, "uv1")])
    sixty_four = OUT / "grid_64.gltf"
    spots_64 = build_grid(sixty_four, 64, [(26, 9, ""), (63, 63, ""), (25, 0, ""), (51, 11, ""), (8, 9, ""), (10, 9, "")])
    # 16 cells on the 8-cell quads: two cells across each, a line through the middle.
    for col in (0, 1):
        for row in (0, 1):
            around = {(col + c + 2 * (row + r)) % 4 for c in (-1, 0, 1) for r in (-1, 0, 1) if (c, r) != (0, 0)}
            check((col + 2 * row) % 4 not in around, f"cell {cell_label(col, row)}: no neighbouring label has its colour")
    print("UV grid and cell labels")
    png_8 = render(EXE, eight, "grid_8", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid-cells", "8"], OUT)
    if not check(png_8 is not None, "--texture-grid-cells 8: captured"):
        return
    camera = Camera(json.loads((OUT / "grid_8.json").read_text(encoding="utf-8")))
    left, _ = camera.project((-1, 0, 0))
    right, _ = camera.project((1, 0, 0))
    check(right - left >= 160, f"a cell is at least 160 pixels ({right - left:.0f}), so each font pixel is at least 4")
    read_labels(Image.open(png_8).convert("RGB"), camera, spots_8, base)
    png_density = render(EXE, eight, "grid_8_density", GRID_LOOK + ["--debug-view", "texturedensity", "--texture-grid-cells", "8"], OUT)
    read_labels(Image.open(png_density).convert("RGB"), camera, spots_8[:2], base, what="density view, ")
    png_64 = render(EXE, sixty_four, "grid_64", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid-cells", "64"], OUT)
    read_labels(Image.open(png_64).convert("RGB"), camera, spots_64, base)

    default = render(EXE, eight, "grid_default", GRID_BASE + ["--debug-view", "texturedistortion"], OUT)
    explicit = render(EXE, eight, "grid_explicit", GRID_BASE + ["--debug-view", "texturedistortion", "--texture-grid", "on", "--texture-grid-cells", "16",
                                                                "--texture-grid-labels", "on", "--texture-heatmap-opacity", "0.75", "--texture-grid-opacity", "0.5"], OUT)
    check(same(default, explicit), "the defaults: grid on, 16 cells, labels on, heat opacity 0.75 (the add-on's), grid opacity 0.5")
    check_line(Image.open(default).convert("RGB"), camera, (0, 0), base, strength=0.5, what="16 cells at the default opacity, ")
    full_16 = render(EXE, eight, "grid_full_16", GRID_LOOK + ["--debug-view", "texturedistortion"], OUT)
    check_line(Image.open(full_16).convert("RGB"), camera, (0, 0), base, what="16 cells, ")

    off_labels = render(EXE, eight, "grid_no_labels", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid-cells", "8", "--texture-grid-labels", "off"], OUT)
    read_labels(Image.open(off_labels).convert("RGB"), camera, spots_8[:2], base, labels=False, what="labels off, ")
    half = render(EXE, eight, "grid_half", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid-cells", "8", "--texture-grid-opacity", "0.5"], OUT)
    read_labels(Image.open(half).convert("RGB"), camera, spots_8[:2], base, strength=0.5, what="grid opacity 0.5, ")
    half_line = render(EXE, eight, "grid_half_line", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid-opacity", "0.5"], OUT)
    check_line(Image.open(half_line).convert("RGB"), camera, (0, 0), base, strength=0.5, what="grid opacity 0.5, ")
    none = render(EXE, eight, "grid_zero", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid-opacity", "0"], OUT)
    off = render(EXE, eight, "grid_off", GRID_LOOK + ["--debug-view", "texturedistortion", "--texture-grid", "off"], OUT)
    check(same(none, off), "grid opacity 0 equals the grid off")
    check(not same(off, default), "the grid changes the image")
    flat = Image.open(off).convert("RGB")
    check(flat.getpixel(tuple(int(round(v)) for v in camera.project((0, 0, 0))))[:3] == tuple(round(v) for v in base) or
          all(abs(g - e) <= TOLERANCE for g, e in zip(flat.getpixel(tuple(int(round(v)) for v in camera.project((0, 0, 0))))[:3], base)),
          "grid off: the middle of a quad is the plain surface")

    far = GRID_BASE[:-1] + ["250", "--texture-grid-opacity", "1", "--debug-view", "texturedistortion", "--texture-grid-cells", "8"]
    far_on = render(EXE, eight, "grid_far_on", far, OUT)
    far_off = render(EXE, eight, "grid_far_off", far + ["--texture-grid-labels", "off"], OUT)
    check(same(far_on, far_off), "cells under 22 pixels have no labels, as the add-on's")

    print("Heat opacity")
    main_model = OUT / "texture_metrics.gltf"
    for opacity in (1.0, 0.3):
        png_path = render(EXE, main_model, f"opacity_{opacity}", HEAT + ["--debug-view", "texturedistortion", "--texture-heatmap-opacity", str(opacity)], OUT)
        main_camera = Camera(json.loads((OUT / f"opacity_{opacity}.json").read_text(encoding="utf-8")))
        expected = tuple(255.0 * c for c in lerp((0.8, 0.8, 0.8), BLUE, opacity))
        compare(Image.open(png_path).convert("RGB"), main_camera, cell(2, 1), expected, f"opacity {opacity}, shear 2")
        grey = tuple(255.0 * c for c in lerp((0.8, 0.8, 0.8), NO_DATA, opacity))
        compare(Image.open(png_path).convert("RGB"), main_camera, cell(1, 2), grey, f"opacity {opacity}, no UVs")

    print("No grid without UVs")
    png_path = render(EXE, main_model, "grid_no_uvs", LOOK + ["--debug-view", "texturedistortion", "--texture-grid-cells", "8"], OUT)
    main_camera = Camera(json.loads((OUT / "grid_no_uvs.json").read_text(encoding="utf-8")))
    image = Image.open(png_path).convert("RGB")
    x, y, _ = cell(1, 2)
    for fx in (-0.9, -0.45, 0.0, 0.45, 0.9):
        for fy in (-0.9, 0.0, 0.9):
            compare(image, main_camera, (x + fx, y + fy, 0.0), surface(NO_DATA), f"no UVs, plain at ({fx}, {fy})")

    print("Legend overlay")
    close = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--aa", "none",
             "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "1.5",
             "--debug-view", "texturedistortion"]
    plain = render(EXE, eight, "legend_grid_off_flag", close + ["--texture-grid", "off"], OUT)
    shots = {name: OUT / f"legend_{name}.png" for name in ("closed", "statistics", "legend", "settings")}
    captured = OUT / "legend_ui_grid_off.png"
    for path in list(shots.values()) + [captured]:
        path.unlink(missing_ok=True)
    script = OUT / "legend_ui.txt"
    script.write_text("\n".join([
        "wait 24", f"shot {shots['closed'].as_posix()}", "snap closed",
        "clickp texture_legend.statistics 0", "wait 6", f"shot {shots['statistics'].as_posix()}", "snap statistics",
        "clickp texture_legend.statistics 0", "wait 4", "clickp texture_legend.legend 0", "wait 6", f"shot {shots['legend'].as_posix()}",
        "clickp texture_legend.legend 0", "wait 4", "clickp texture_legend.settings 0", "wait 6", f"shot {shots['settings'].as_posix()}",
        "clickp texture_legend.texture_grid 0", "wait 8", f"capture {captured.as_posix()}", "snap after_controls",
        "click 800 400", "wait 6", "snap picked", "wait 2"]) + "\n", encoding="utf-8")
    report = OUT / "legend_ui.json"
    report.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(eight)] + COMMON + close + ["--ui-script", str(script), "--ui-report", str(report)], cwd=REPO,
                            capture_output=True, text=True, timeout=240)
    ui = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {"failed": True, "errors": [result.stderr[-200:]], "snaps": []}
    check(not ui["failed"], f"every section and the grid switch respond to clicks ({ui['errors']})")
    if not ui["failed"]:
        snaps = {snap["label"]: snap for snap in ui["snaps"]}
        check(same(captured, plain), "UV Grid in the legend's Settings equals --texture-grid off")
        check("type=0|" in snaps["after_controls"]["selection"], "clicks on the legend pick nothing under it")
        check("type=0|" not in snaps["picked"]["selection"], "a click beside it still picks (the same surface)")
        check(snaps["after_controls"]["revisions"]["syncs"] == snaps["closed"]["revisions"]["syncs"], "the sections and the grid switch run no renderer sync")
        check(snaps["after_controls"].get("glErrors") == 0 and snaps["after_controls"].get("idConflict") == 0, "no GL errors, no ImGui ID conflicts")

    check_overlay(eight, spots_8, plain_look=GRID_BASE)

    print("Pipe route")
    session = Session(eight, GRID_LOOK)
    session.set("render_mode", "texturedistortion")
    session.set("texture_grid_cells", "8")
    check(same(session.capture("pipe_grid_8"), png_8), "texture_grid_cells 8 equals the flag")
    session.set("texture_grid_labels", False)
    check(same(session.capture("pipe_grid_labels"), off_labels), "texture_grid_labels off equals the flag")
    session.set("texture_grid_labels", True)
    session.set("texture_grid_opacity", 0.5)
    check(same(session.capture("pipe_grid_half"), half), "texture_grid_opacity 0.5 equals the flag")
    session.set("texture_grid_opacity", 1.0)
    session.set("texture_grid", False)
    session.set("texture_grid_cells", "16")
    check(same(session.capture("pipe_grid_off"), off), "texture_grid off equals the flag")
    session.set("texture_grid", True)
    check(same(session.capture("pipe_grid_on"), full_16), "and on again")
    state = session.metrics()
    check(state["builds"] == 1, f"grid settings do not rebuild the table (builds {state['builds']})")
    for key, bad in (("texture_grid_cells", "7"), ("texture_heatmap_opacity", 0.1), ("texture_grid_opacity", 1.5)):
        try:
            session.set(key, bad)
            check(False, f"{key} {bad} refused")
        except glider_ctl.GliderError:
            check(True, f"{key} {bad} refused")
    errors = session.call("get_state").get("glErrors")
    check(errors == 0, f"zero GL errors ({errors})")
    session.close()


def build_packaged(path):
    """ASOBO_asset_optimized: half-float TEXCOORD_0 in USHORT, one index accessor sliced by ASOBO_primitive."""
    f = Fixture()
    f.declare("ASOBO_asset_optimized")
    plain = f.material("packaged_a")
    other = f.material("packaged_b")
    positions = [(x - 2, y, z) for x, y, z in quad(1, 1)] + [(x + 2, y, z) for x, y, z in quad(1, 1)]
    uvs = rect(.25, .25) + rect(.5, .25)
    half = struct.pack(f"<{len(uvs) * 2}e", *[v for uv in uvs for v in uv])
    position = f.floats(positions, "VEC3")
    uv = f.accessor(half, 5123, "VEC2", len(uvs))
    indices = f.accessor(struct.pack("<12H", *(QUAD + QUAD)), 5123, "SCALAR", 12, target=34963)
    mesh = {"primitives": []}
    for material, start, base in ((plain, 0, 0), (other, 6, 4)):
        mesh["primitives"].append({"attributes": {"POSITION": position, "TEXCOORD_0": uv}, "indices": indices, "material": material,
                                   "extras": {"ASOBO_primitive": {"StartIndex": start, "PrimitiveCount": 2, "BaseVertexIndex": base}}})
    f.gltf["meshes"].append(mesh)
    f.node("packaged", 0)
    f.write(path)


def pixel(image, camera, point):
    x, y = camera.project(point)
    return image.getpixel((int(round(x)), int(round(y))))[:3]


def compare(image, camera, point, expected, label):
    got = pixel(image, camera, point)
    ok = all(abs(g - e) <= TOLERANCE for g, e in zip(got, expected))
    check(ok, f"{label}: {tuple(round(e) for e in expected)} (got {got})")


def check_faces(fixture, image, camera, view):
    for name, point, shear, area, relative, special in fixture.faces:
        if special is not None:
            compare(image, camera, point, special, f"{view}, {name}")
        elif view == "density":
            if relative is not None:
                compare(image, camera, point, density(relative), f"density, {name}, {relative:.3f}")
        else:
            compare(image, camera, point, distortion(shear, area, view), f"{view}, {name}, shear {shear:.3f}, area {area:.3f}")


class Session:
    """A --control-pipe Glider; killed whenever it fails or the script exits."""

    def __init__(self, model, flags):
        self.process = subprocess.Popen([str(EXE), str(model)] + COMMON + list(flags) + ["--control-pipe"], cwd=REPO,
                                        stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        atexit.register(self.close)
        self.pipe = None
        deadline = time.time() + 120
        while self.pipe is None:
            if self.process.poll() is not None or time.time() > deadline:
                raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
            try:
                self.pipe = glider_ctl.GliderPipe(self.process.pid)
            except Exception:  # noqa: BLE001 - not listening yet
                time.sleep(0.2)

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def set(self, key, value):
        return self.call("set_viewport_setting", key=key, value=value)

    def capture(self, name):
        path = OUT / f"{name}.png"
        path.unlink(missing_ok=True)
        self.call("capture", path=str(path))
        return path

    def metrics(self):
        return self.call("get_state")["textureMetrics"]

    def close(self):
        if self.pipe is not None:
            try:
                self.pipe.close()
            except Exception:  # noqa: BLE001
                pass
            self.pipe = None
        if self.process.poll() is None:
            self.process.kill()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    model = OUT / "texture_metrics.gltf"
    fixture = build_main(model)
    packaged = OUT / "packaged.gltf"
    build_packaged(packaged)

    print("Known answers through the CLI route")
    images = {}
    camera = None
    for name, flags in (("both", ["--debug-view", "texturedistortion"]),
                        ("shear", ["--debug-view", "texture-distortion", "--texture-distortion-view", "shear"]),
                        ("area", ["--debug-view", "Texture Distortion", "--texture-distortion-view", "area"]),
                        ("density", ["--debug-view", "texturedensity"])):
        png_path = render(EXE, model, f"cli_{name}", HEAT + flags, OUT)
        if not check(png_path is not None, f"--debug-view {name}: captured"):
            return finish()
        report = json.loads((OUT / f"cli_{name}.json").read_text(encoding="utf-8"))
        camera = Camera(report)
        images[name] = png_path
        check_faces(fixture, Image.open(png_path).convert("RGB"), camera, name)

    print("Pipe route, state and lifetime")
    session = Session(model, HEAT)
    state = session.metrics()
    check(state["built"] is False and state["builds"] == 0, "default view: no table is built")
    session.set("render_mode", "texturedensity")
    pipe_density = session.capture("pipe_density")
    check(same(pipe_density, images["density"]), "set_viewport_setting render_mode texturedensity equals --debug-view")
    session.set("render_mode", "texturedistortion")
    session.set("texture_distortion_view", "area")
    check(same(session.capture("pipe_area"), images["area"]), "texture_distortion_view area equals the flag")
    session.set("texture_distortion_view", "both")
    check(same(session.capture("pipe_both"), images["both"]), "texture_distortion_view both equals the flag")
    state = session.metrics()
    check(state["built"] is True and state["builds"] == 1, "one build across both views and view changes (one sync-free table)")
    print("  INFO", json.dumps({k: v for k, v in state.items() if k != "materials"}))
    materials = {m["name"]: m for m in state["materials"]}

    def near(value, expected, label, relative=1e-5):
        check(abs(value - expected) <= relative * max(abs(expected), 1e-9), f"{label}: {expected:.6g} (got {value:.6g})")

    near(materials["density"]["referenceUvPerSquareMetre"], 1.0 / 36.0, "density: the median is the 36 m2 quad's UV per m2")
    near(materials["density"]["pixelsPerMetre"], 256.0 / 6.0, "density: pixels per metre, 256 px texture on 6 m")
    check(materials["density"]["texture"] == "baseColorTexture" and materials["density"]["textureWidth"] == 256, "density: reads the base colour's size")
    near(materials["gpu_instanced"]["pixelsPerMetre"], math.sqrt(0.25 * 512 * 128), "EXT_mesh_gpu_instancing: scale 2, 512 x 128 texture")
    check(materials["gpu_instanced"]["triangles"] == 4, "EXT_mesh_gpu_instancing: both instances count")
    near(materials["skinned"]["pixelsPerMetre"], 128.0, "skin: joint scale 2 (the node's transform does not apply)")
    near(materials["scale"]["pixelsPerMetre"], math.sqrt(0.04 / 16.0) * 128, "node scale 2 sets the median by area")
    check(materials["uv1"]["uvSet"] == 1 and materials["uv0"]["uvSet"] == 0, "UV set of the base colour")
    check(materials["shear"]["pixelsPerMetre"] == 0 and materials["shear"]["texture"] == "", "no texture: no pixels per metre")
    check(state["degenerateTriangles"] == 2, "two triangles without UV area")
    check("no_uvs" in state["skippedNoUvs"], "the node without UVs is listed")
    check(state["islands"]["count"] > 0, "islands counted")
    check(state["triangles"] == 56, f"56 triangles measured, instances counted, the no-UV primitive not ({state['triangles']})")
    check(state["shear"]["over125"] > 0.0 and state["density"]["under"] > 0.0 and state["density"]["over"] > 0.0, "shares are reported")

    session.set("render_mode", "shaded_post")
    after_views = session.capture("pipe_shaded_after")
    state = session.metrics()
    check(state["built"] is False, "leaving the views frees the table")
    for bad in ("sideways",):
        try:
            session.set("texture_distortion_view", bad)
            check(False, f"texture_distortion_view {bad} refused")
        except glider_ctl.GliderError:
            check(True, f"texture_distortion_view {bad} refused")
    errors = session.call("get_state").get("glErrors")
    check(errors == 0, f"zero GL errors ({errors})")
    session.close()

    print("Packaged asset (ASOBO_asset_optimized)")
    camera_flags = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--aa", "none",
                    "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "10",
                    "--debug-view", "texturedistortion"] + NO_UV_GRID
    png_path = render(EXE, packaged, "packaged", camera_flags, OUT)
    if check(png_path is not None, "packaged: captured"):
        camera = Camera(json.loads((OUT / "packaged.json").read_text(encoding="utf-8")))
        image = Image.open(png_path).convert("RGB")
        compare(image, camera, (-2, 0, 0), distortion(1.0, 1.0, "both"), "half-float UV, first ASOBO_primitive slice, shear 1")
        compare(image, camera, (2, 0, 0), distortion(2.0, 1.0, "both"), "half-float UV, second slice with a base vertex, shear 2")

    check_grid()

    print("Everything else is unchanged")
    shaded = render(EXE, model, "shaded", LOOK, OUT)
    check(shaded is not None and same(after_views, shaded), "shaded after a visit to the views equals a fresh shaded run")
    if args.baseline_exe:
        baseline = REPO / args.baseline_exe
        for name, flags in (("shaded", []), ("texcoord", ["--debug-view", "texturecoordinates0"]), ("normal", ["--debug-view", "geometrynormal"]), ("wireframe", ["--render-mode", "wireframe"])):
            ours = render(EXE, model, f"same_{name}", LOOK + flags, OUT)
            theirs = render(baseline, model, f"baseline_{name}", LOOK + flags, OUT)
            check(ours is not None and theirs is not None and same(ours, theirs), f"{name}: byte-identical to the baseline")
    return finish()


if __name__ == "__main__":
    main()
