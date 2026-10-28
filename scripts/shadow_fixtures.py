# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.2 shadow fixtures: a receiver plane and occluders, written as small self-contained glTF files.

Used by scripts/validate_shadows.py and scripts/review_shadows.py. Every material is a pure Lambert surface
(KHR_materials_specular factor 0, roughness 1) unless stated, so under a black environment the linear value
of a lit surface is exactly colour x sun intensity x N.L / pi and a shadowed one is 0.
"""

import base64
import json
import struct
from pathlib import Path


class Builder:
    def __init__(self, generator="A3.2 shadow fixture"):
        self.blob = bytearray()
        self.views, self.accessors, self.meshes, self.nodes = [], [], [], []
        self.materials, self.textures, self.images, self.samplers = [], [], [], []
        self.used = {"KHR_materials_specular"}
        self.generator = generator

    def _add(self, data, target, count, ctype, kind, minmax=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        self.views.append({"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data), "target": target})
        self.blob.extend(data)
        accessor = {"bufferView": len(self.views) - 1, "componentType": ctype, "count": count, "type": kind}
        if minmax:
            accessor["min"], accessor["max"] = minmax
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def material(self, name, color=(1, 1, 1, 1), alpha_mode=None, cutoff=None, double_sided=False, texture=None,
                 extensions=None):
        pbr = {"baseColorFactor": list(color), "metallicFactor": 0.0, "roughnessFactor": 1.0}
        if texture is not None:
            pbr["baseColorTexture"] = {"index": texture}
        material = {"name": name, "pbrMetallicRoughness": pbr,
                    "extensions": {"KHR_materials_specular": {"specularFactor": 0.0}}}
        if alpha_mode:
            material["alphaMode"] = alpha_mode
        if cutoff is not None:
            material["alphaCutoff"] = cutoff
        if double_sided:
            material["doubleSided"] = True
        for name_, value in (extensions or {}).items():
            material["extensions"][name_] = value
            self.used.add(name_)
        self.materials.append(material)
        return len(self.materials) - 1

    def checker_texture(self, size=64, cells=8):
        """RGBA PNG: opaque white cells alternating with alpha-0 cells (cells x cells), as a data URI."""
        import io
        from PIL import Image
        image = Image.new("RGBA", (size, size))
        cell = size // cells
        for y in range(size):
            for x in range(size):
                opaque = ((x // cell) + (y // cell)) % 2 == 0
                image.putpixel((x, y), (255, 255, 255, 255 if opaque else 0))
        out = io.BytesIO()
        image.save(out, "PNG")
        self.images.append({"uri": "data:image/png;base64," + base64.b64encode(out.getvalue()).decode("ascii")})
        if not self.samplers:
            self.samplers.append({"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071})
        self.textures.append({"source": len(self.images) - 1, "sampler": 0})
        return len(self.textures) - 1

    def quad(self, name, center, size, material, normal_up=True):
        """Horizontal quad (XZ plane) of size (sx, sz) at center, facing +Y (or -Y)."""
        cx, cy, cz = center
        sx, sz = size
        positions = [(cx - sx / 2, cy, cz - sz / 2), (cx + sx / 2, cy, cz - sz / 2),
                     (cx + sx / 2, cy, cz + sz / 2), (cx - sx / 2, cy, cz + sz / 2)]
        ny = 1.0 if normal_up else -1.0
        normals = [(0.0, ny, 0.0)] * 4
        uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        indices = [0, 2, 1, 0, 3, 2] if normal_up else [0, 1, 2, 0, 2, 3]
        return self.mesh(name, positions, normals, uvs, indices, material)

    def box(self, name, lo, hi, material):
        """Axis-aligned box with outward normals (24 vertices)."""
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        faces = [((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
                 ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
                 ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
                 ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
                 ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
                 ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)])]
        positions, normals, uvs, indices = [], [], [], []
        for normal, corners in faces:
            base = len(positions)
            positions += corners
            normals += [normal] * 4
            uvs += [(0, 0), (0, 1), (1, 1), (1, 0)]
            # Counter-clockwise seen from outside.
            indices += [base, base + 1, base + 2, base, base + 2, base + 3]
        return self.mesh(name, positions, normals, uvs, indices, material)

    def mesh(self, name, positions, normals, uvs, indices, material):
        pos = b"".join(struct.pack("<3f", *p) for p in positions)
        lo = [min(p[i] for p in positions) for i in range(3)]
        hi = [max(p[i] for p in positions) for i in range(3)]
        attributes = {"POSITION": self._add(pos, 34962, len(positions), 5126, "VEC3", (lo, hi)),
                      "NORMAL": self._add(b"".join(struct.pack("<3f", *n) for n in normals), 34962, len(normals), 5126, "VEC3"),
                      "TEXCOORD_0": self._add(b"".join(struct.pack("<2f", *t) for t in uvs), 34962, len(uvs), 5126, "VEC2")}
        index = self._add(b"".join(struct.pack("<I", i) for i in indices), 34963, len(indices), 5125, "SCALAR")
        self.meshes.append({"name": name, "primitives": [{"attributes": attributes, "indices": index, "material": material}]})
        self.nodes.append({"name": name, "mesh": len(self.meshes) - 1})
        return len(self.nodes) - 1

    def write(self, path):
        doc = {"asset": {"version": "2.0", "generator": self.generator}, "scene": 0,
               "scenes": [{"nodes": list(range(len(self.nodes)))}], "nodes": self.nodes, "meshes": self.meshes,
               "materials": self.materials, "accessors": self.accessors, "bufferViews": self.views,
               "buffers": [{"byteLength": len(self.blob),
                            "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(self.blob)).decode("ascii")}],
               "extensionsUsed": sorted(self.used)}
        if self.images:
            doc["images"], doc["textures"], doc["samplers"] = self.images, self.textures, self.samplers
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(doc), encoding="utf-8")
        return path


PLANE_SIZE = 16.0


def basic(path, height=1.5, size=2.0):
    """Receiver plane at y = 0 and a double-sided 2 x 2 m occluder quad at y = height, centred over the origin."""
    b = Builder()
    ground = b.material("Ground")
    b.quad("Ground", (0.0, 0.0, 0.0), (PLANE_SIZE, PLANE_SIZE), ground)
    b.quad("Occluder", (0.0, height, 0.0), (size, size), b.material("Occluder", double_sided=True))
    return b.write(path)


def contact(path):
    """Receiver plane and a 1 m cube standing on it (contact line at y = 0)."""
    b = Builder()
    b.quad("Ground", (0.0, 0.0, 0.0), (PLANE_SIZE, PLANE_SIZE), b.material("Ground"))
    b.box("Cube", (-0.5, 0.0, -0.5), (0.5, 1.0, 0.5), b.material("Cube"))
    return b.write(path)


def casters(path, height=1.5):
    """Receiver plane and four 1.5 x 1.5 m occluder quads at y = height (x = -3, -1, 1, 3 at z = 0):
    noCastShadow (ASOBO_material_shadow_options), BLEND, ASOBO_material_invisible, and a MASK checker
    (8 x 8 cells, half cut away). The plain opaque reference is basic()."""
    b = Builder()
    b.quad("Ground", (0.0, 0.0, 0.0), (PLANE_SIZE, PLANE_SIZE), b.material("Ground"))
    no_cast = b.material("NoCast", double_sided=True,
                         extensions={"ASOBO_material_shadow_options": {"noCastShadow": True}})
    blend = b.material("Blend", color=(1, 1, 1, 0.5), alpha_mode="BLEND", double_sided=True)
    invisible = b.material("Invisible", double_sided=True, extensions={"ASOBO_material_invisible": {"enabled": True}})
    checker = b.material("Mask", alpha_mode="MASK", cutoff=0.5, double_sided=True, texture=b.checker_texture())
    for name, material, x in (("NoCastOccluder", no_cast, -3.0), ("BlendOccluder", blend, -1.0),
                              ("InvisibleOccluder", invisible, 1.0), ("MaskOccluder", checker, 3.0)):
        b.quad(name, (x, height, 0.0), (1.5, 1.5), material)
    return b.write(path)
