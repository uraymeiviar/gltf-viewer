# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A5.1 ASOBO_material_detail_map validation.

Every fixture is a single lit quad whose material uses one detail feature at a time, so the
checks are byte-exact where the design asks for it:

  neutral     detail colour with aD = 0, a flat BC5 SNORM detail normal and an 8-bit ORM
              texel one step below 0.5 with the base values at the clamp: byte-identical to
              the same material without the extension.
  colour      smoothstep(clamp(sqrt(B * D))) with aD = vA = 1.
  orm         detail OMR offsets added unscaled to the factor-scaled base metallic/roughness, and
              the AO offset added to the occlusion map.
  uv          UVScale tiling with a 4-band nearest-filtered texture; UVOffset rotates the
              bands by half the texture.
  normal      detailNormalTexture.scale, the A1.2 DirectX flip on a signed BC5 SNORM map,
              and scale 0 as a byte-exact neutral.
  blend       blendMaskTexture.r with blendThreshold: mask 0 keeps the base, mask 1 applies
              the detail, and a wider threshold edge blends at a mid vertex alpha.
  alpha       the detail colour alpha multiplies the material alpha (MASK cutoff).
  inert       only blendMaskTexture, or no texture at all: no shader variant, byte-identical.
  switch      --asobo-disable ASOBO_material_detail_map is byte-identical to no extension.
  windshield  windshield v3, v1 and material code "Windshield" ignore the detail map (the slots are
              windshield masks, detail reflection and icing normal there).
  decal       the decal accumulation pass evaluates the same material_info detail path.

Run: python scripts/validate_asobo_detail_map.py
"""

import base64
import hashlib
import json
import re
import struct
import sys
import zlib
from pathlib import Path

import harness_process as subprocess
from validate_asobo_normal import _read_png
from validate_texture_ktx2 import bc5_block, make_ktx2

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "asobo_detail_map"
DETAIL = "ASOBO_material_detail_map"
MSFT_DDS = "MSFT_texture_dds"
NEAREST = {"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}
CAMERA = ["--isolated", "--window-size", "1280x800", "--aa", "none", "--clear-color", "0,0,0",
          "--hide-grid", "--camera-projection", "ortho", "--camera-yaw", "0", "--camera-pitch", "0",
          "--camera-distance", "2", "--fixed-frame-time", "0.05", "--vsync", "off"]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label, flush=True)
    if not condition:
        failures.append(label)
    return condition


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() if path is not None and Path(path).is_file() else None


def same(a, b):
    return sha(a) is not None and sha(a) == sha(b)


def srgb(value):
    """The shader's linearTosRGB approximation (pow(v, 1/2.2)), as 8-bit."""
    return round(255.0 * (max(0.0, min(1.0, value)) ** (1.0 / 2.2)))


def close(actual, expected, tolerance=2):
    return actual is not None and all(abs(a - e) <= tolerance for a, e in zip(actual, expected))


def make_png(width, height, rows):
    """Compresses an 8-bit RGBA image from rows[y][x] = (r, g, b, a)."""
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(channel for pixel in row for channel in pixel) for row in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def solid_png(color):
    return make_png(1, 1, [[color]])


def flat_normal_png():
    """An 8-bit flat normal map; only used as the KTX2 fallback and the base normal map."""
    return solid_png((128, 128, 255, 255))


class Fixture:
    """One quad fixture; textures are added first, then the material references them."""

    def __init__(self, name):
        self.name = name
        self.images = []
        self.textures = []
        self.files = {}
        self.accessors = []
        self.buffer_views = []
        self.binary = bytearray()

    def add_png_texture(self, png, nearest=True):
        self.images.append({"uri": "data:image/png;base64," + base64.b64encode(png).decode("ascii")})
        return self._add_texture(len(self.images) - 1, nearest)

    def add_ktx2_texture(self, ktx2, nearest=True):
        filename = f"{self.name}_{len(self.files)}.PNG.KTX2"
        self.files[filename] = ktx2
        self.images.append({"uri": "fallback.png"})
        self.images.append({"uri": filename})
        texture = {"source": len(self.images) - 2,
                   "extensions": {MSFT_DDS: {"source": len(self.images) - 1}}}
        if nearest:
            texture["sampler"] = 0
        self.textures.append(texture)
        return len(self.textures) - 1

    def _add_texture(self, source, nearest):
        texture = {"source": source}
        if nearest:
            texture["sampler"] = 0
        self.textures.append(texture)
        return len(self.textures) - 1

    def _add_quad(self, z, half, color_alpha):
        positions = [(-half, -half, z), (half, -half, z), (half, half, z), (-half, half, z)]
        normals = [(0.0, 0.0, 1.0)] * 4
        uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        indices = [0, 1, 2, 0, 2, 3]
        base_view = len(self.buffer_views)
        base_accessor = len(self.accessors)
        offset = len(self.binary)
        self.binary += b"".join(struct.pack("<3f", *v) for v in positions)
        self.binary += b"".join(struct.pack("<3f", *v) for v in normals)
        self.binary += b"".join(struct.pack("<2f", *v) for v in uvs)
        self.buffer_views += [
            {"buffer": 0, "byteOffset": offset, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": offset + 48, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": offset + 96, "byteLength": 32, "target": 34962},
        ]
        self.accessors += [
            {"bufferView": base_view, "componentType": 5126, "count": 4, "type": "VEC3",
             "min": [-half, -half, z], "max": [half, half, z]},
            {"bufferView": base_view + 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": base_view + 2, "componentType": 5126, "count": 4, "type": "VEC2"},
        ]
        attributes = {"POSITION": base_accessor, "NORMAL": base_accessor + 1, "TEXCOORD_0": base_accessor + 2}
        index_offset = offset + 128
        if color_alpha is not None:
            index_offset = offset + 192
            self.binary += b"".join(struct.pack("<4f", 1.0, 1.0, 1.0, color_alpha) for _ in positions)
            self.buffer_views.append({"buffer": 0, "byteOffset": offset + 128, "byteLength": 64,
                                      "target": 34962})
            self.accessors.append({"bufferView": base_view + 3, "componentType": 5126, "count": 4,
                                   "type": "VEC4"})
            attributes["COLOR_0"] = base_accessor + 3
        self.binary += struct.pack("<6H", *indices)
        self.buffer_views.append({"buffer": 0, "byteOffset": index_offset, "byteLength": 12,
                                  "target": 34963})
        self.accessors.append({"bufferView": len(self.buffer_views) - 1, "componentType": 5123,
                               "count": 6, "type": "SCALAR"})
        return attributes, len(self.accessors) - 1

    def write(self, material, extensions_used=(), color_alpha=None, asset_convention=None,
              declare_detail=True, half=0.25, z=0.0, receiver=None):
        # The first quad is the material under test; `receiver` adds the opaque surface a
        # decal material accumulates onto, painted with that base colour.
        attributes, index_accessor = self._add_quad(z, half, color_alpha)
        meshes = [{"name": self.name, "primitives": [{
            "attributes": attributes, "indices": index_accessor, "material": 0}]}]
        nodes = [{"name": self.name, "mesh": 0}]
        materials = [{**material, "name": self.name}]
        if receiver is not None:
            receiver_attributes, receiver_indices = self._add_quad(0.0, 0.25, None)
            meshes.append({"name": "Receiver", "primitives": [{
                "attributes": receiver_attributes, "indices": receiver_indices, "material": 1}]})
            nodes.append({"name": "Receiver", "mesh": 1})
            materials.append({"name": "Receiver", "pbrMetallicRoughness": {
                "baseColorFactor": [*receiver, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.5}})
        used = sorted({*extensions_used})
        if declare_detail:
            used = sorted({*used, DETAIL})
        if asset_convention is not None:
            used = sorted({*used, "ASOBO_normal_map_convention"})
        document = {
            "asset": {"version": "2.0", "generator": "A5.1 detail map validation fixture"},
            "scene": 0,
            "scenes": [{"nodes": list(range(len(nodes)))}],
            "nodes": nodes,
            "meshes": meshes,
            "materials": materials,
            "images": self.images,
            "textures": self.textures,
            "samplers": [NEAREST],
            "accessors": self.accessors,
            "bufferViews": self.buffer_views,
            "buffers": [{"byteLength": len(self.binary), "uri": "data:application/octet-stream;base64,"
                                                             + base64.b64encode(bytes(self.binary)).decode("ascii")}],
            "extensionsUsed": used,
        }
        if asset_convention is not None:
            document["asset"]["extensions"] = {
                "ASOBO_normal_map_convention": {"tangent_space_convention": asset_convention}}
        path = OUT / f"{self.name}.gltf"
        path.write_text(json.dumps(document, indent=1), encoding="utf-8")
        for filename, data in self.files.items():
            (OUT / filename).write_bytes(data)
        (OUT / "fallback.png").write_bytes(flat_normal_png())
        return path


def detail_extension(color=None, normal=None, orm=None, blend_mask=None, uv_scale=None,
                     uv_offset=None, threshold=None, normal_scale=None):
    extension = {}
    if uv_scale is not None:
        extension["UVScale"] = uv_scale
    if threshold is not None:
        extension["blendThreshold"] = threshold
    if uv_offset is not None:
        extension["UVOffset"] = uv_offset
    if color is not None:
        extension["detailColorTexture"] = {"index": color}
    if normal is not None:
        reference = {"index": normal}
        if normal_scale is not None:
            reference["scale"] = normal_scale
        extension["detailNormalTexture"] = reference
    if orm is not None:
        extension["detailMetalRoughAOTexture"] = {"index": orm}
    if blend_mask is not None:
        extension["blendMaskTexture"] = {"index": blend_mask}
    return extension


def base_material(base_color=(0.5, 0.5, 0.5, 1.0), metallic=0.0, roughness=0.5, extra=None):
    material = {"pbrMetallicRoughness": {
        "baseColorFactor": list(base_color), "metallicFactor": metallic, "roughnessFactor": roughness}}
    if extra is not None:
        material.update(extra)
    return material


def capture(model, label, view="basecolor", extra=()):
    suffix = view if view is not None else "shaded"
    png = OUT / f"{label}_{suffix}.png"
    report = OUT / f"{label}_{suffix}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    arguments = [str(EXE), str(model), *CAMERA, *extra]
    if view is not None:
        arguments += ["--debug-view", view]
    arguments += ["--capture-viewport", str(png), "--capture-frame", "8",
                  "--profile-report", str(report), "--exit-after-profile"]
    result = subprocess.run(arguments, cwd=REPO, capture_output=True, text=True, timeout=300)
    log = result.stdout + result.stderr
    (OUT / f"{label}_{suffix}.log").write_text(log, encoding="utf-8")
    if result.returncode != 0 or not png.is_file():
        check(False, f"{label}_{suffix}: capture failed ({result.returncode}) {log[-200:]}")
        return None
    if "GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x", log):
        check(False, f"{label}_{suffix}: GL diagnostics failed; see the capture log")
        return None
    return png


def image(path):
    return _read_png(path) if path is not None else None


def center(path):
    decoded = image(path)
    if decoded is None:
        return None
    width, height, channels, data = decoded
    offset = ((height // 2) * width + width // 2) * channels
    return tuple(data[offset:offset + 3])


def quad_span(path):
    """The first and last non-background pixel of the centre row (background is black)."""
    decoded = image(path)
    if decoded is None:
        return None
    width, height, channels, data = decoded
    row = height // 2
    columns = [x for x in range(width) if data[(row * width + x) * channels:(row * width + x) * channels + 3]
               != b"\x00\x00\x00"]
    if not columns:
        return None
    return columns[0], columns[-1]


def band_colors(path, bands):
    """Colours at the centres of `bands` equal horizontal slices of the rendered quad."""
    decoded = image(path)
    span = quad_span(path)
    if decoded is None or span is None:
        return None
    width, height, channels, data = decoded
    row = height // 2
    start, end = span
    colors = []
    for band in range(bands):
        x = int(round(start + (end - start + 1) * (band + 0.5) / bands))
        offset = (row * width + min(x, width - 1)) * channels
        colors.append(tuple(data[offset:offset + 3]))
    return colors


def count_bands(path):
    """Number of distinct contiguous colour runs on the centre row inside the quad."""
    decoded = image(path)
    span = quad_span(path)
    if decoded is None or span is None:
        return None
    width, height, channels, data = decoded
    row = height // 2
    start, end = span
    # Skip the first and last pixels: a partially covered edge pixel is a run of its own.
    runs = 1
    previous = None
    for x in range(start + 1, end):
        offset = (row * width + x) * channels
        color = tuple(data[offset:offset + 3])
        if previous is not None and color != previous:
            runs += 1
        previous = color
    return runs


def detail_weight(vA, mask, threshold):
    """The shader's blend-mode detail weight: 1 - saturate((vA - low) / (high - low))."""
    low = min(max(mask - threshold, 0.0), 1.0)
    high = min(max(mask + threshold, 0.0), 1.0)
    coverage = min(max((vA - low) / max(high - low, 1.0e-5), 0.0), 1.0)
    return 1.0 - coverage


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    print("=== A5.1 ASOBO_material_detail_map ===", flush=True)

    # ---------------------------------------------------------------- neutral ---
    print("\n[neutral: aD = 0, flat BC5 SNORM normal, ORM one step below 0.5]", flush=True)
    # 127/255 is the closest 8-bit value below 0.5: the detail offset is negative, so a base
    # metallic/roughness/AO of 0 clamps back to exactly 0 and the operator is an identity.
    neutral_color_png = make_png(1, 1, [[(120, 60, 30, 0)]])
    neutral_orm_png = solid_png((127, 127, 127, 255))
    flat_normal = make_ktx2(142, 4, 4, [bc5_block(0.0, 0.0)])
    black = solid_png((0, 0, 0, 255))

    plain = Fixture("neutral_plain")
    plain_base = plain.add_png_texture(solid_png((200, 200, 200, 255)))
    plain_normal = plain.add_png_texture(flat_normal_png())
    plain_black = plain.add_png_texture(black)
    plain_material = {
        "pbrMetallicRoughness": {"baseColorTexture": {"index": plain_base},
                                 "metallicFactor": 1.0, "roughnessFactor": 1.0,
                                 "metallicRoughnessTexture": {"index": plain_black}},
        "normalTexture": {"index": plain_normal},
        "occlusionTexture": {"index": plain_black, "strength": 1.0},
    }
    plain_path = plain.write(plain_material, declare_detail=False)

    neutral = Fixture("neutral_detail")
    neutral_base = neutral.add_png_texture(solid_png((200, 200, 200, 255)))
    neutral_normal = neutral.add_png_texture(flat_normal_png())
    neutral_black = neutral.add_png_texture(black)
    neutral_color_texture = neutral.add_png_texture(neutral_color_png)
    neutral_orm_texture = neutral.add_png_texture(neutral_orm_png)
    neutral_detail_normal = neutral.add_ktx2_texture(flat_normal)
    neutral_material = {
        "pbrMetallicRoughness": {"baseColorTexture": {"index": neutral_base},
                                 "metallicFactor": 1.0, "roughnessFactor": 1.0,
                                 "metallicRoughnessTexture": {"index": neutral_black}},
        "normalTexture": {"index": neutral_normal},
        "occlusionTexture": {"index": neutral_black, "strength": 1.0},
        "extensions": {DETAIL: detail_extension(
            color=neutral_color_texture, normal=neutral_detail_normal, orm=neutral_orm_texture,
            uv_scale=1.0)},
    }
    neutral_path = neutral.write(neutral_material, [MSFT_DDS])
    check(same(capture(neutral_path, "neutral_detail"), capture(plain_path, "neutral_plain")),
          "neutral detail is byte-identical to the material without the extension")

    # ---------------------------------------------------------------- colour ---
    print("\n[colour: mix(B, smoothstep(clamp(sqrt(B * D))), aD * vA)]", flush=True)
    colour = Fixture("colour")
    colour_texture = colour.add_png_texture(solid_png((255, 255, 255, 255)))
    colour_white = base_material(base_color=(0.5, 0.5, 0.5, 1.0), extra={
        "extensions": {DETAIL: detail_extension(color=colour_texture, uv_scale=1.0)}})
    colour_path = colour.write(colour_white)
    expected = srgb(3.0 * 0.5 - 2.0 * (0.5 ** 1.5))
    check(close(center(capture(colour_path, "colour")), (expected,) * 3),
          f"detail colour centre is smoothstep(sqrt(0.5)) = {expected}")

    # ------------------------------------------------------------------- ORM ---
    print("\n[orm: unscaled offsets added to the factor-scaled base; AO added to the map]", flush=True)
    orm = Fixture("orm")
    orm_white = orm.add_png_texture(solid_png((255, 255, 255, 255)))    # MR: metallic/roughness 1
    orm_base_ao = orm.add_png_texture(solid_png((64, 64, 64, 255)))     # base AO 64/255
    orm_detail = orm.add_png_texture(solid_png((191, 191, 64, 255)))    # dORM r/g 0.749, b 0.251
    orm_material = {
        "pbrMetallicRoughness": {"baseColorFactor": [0.5, 0.5, 0.5, 1.0], "metallicFactor": 0.5,
                                 "roughnessFactor": 0.5, "metallicRoughnessTexture": {"index": orm_white}},
        "occlusionTexture": {"index": orm_base_ao, "strength": 1.0},
        "extensions": {DETAIL: detail_extension(orm=orm_detail, uv_scale=1.0)},
    }
    orm_path = orm.write(orm_material)
    check(close(center(capture(orm_path, "orm", "roughness")), (srgb(0.5 + 0.498),) * 3),
          "detail roughness offset: clamp(0.5 * 1 + 0.498) (offset not scaled by the factor)")
    check(close(center(capture(orm_path, "orm", "metallic")), (srgb(0.5 - 0.498),) * 3),
          "detail metallic offset: clamp(0.5 * 1 - 0.498) (offset not scaled by the factor)")
    check(close(center(capture(orm_path, "orm", "occlusion")), (srgb(191.0 / 255.0),) * 3),
          "detail AO offset: clamp(64/255 + 0.498) == 191/255")

    # --------------------------------------------------------------------- UV ---
    print("\n[uv: UVScale tiling and UVOffset]", flush=True)
    bands_png = make_png(4, 1, [[(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255), (255, 255, 255, 255)]])
    RED, GREEN, BLUE, WHITE = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)

    def uv_fixture(name, uv_scale, uv_offset=None):
        fixture = Fixture(name)
        texture = fixture.add_png_texture(bands_png)
        material = base_material(base_color=(1.0, 1.0, 1.0, 1.0), extra={
            "extensions": {DETAIL: detail_extension(color=texture, uv_scale=uv_scale, uv_offset=uv_offset)}})
        return fixture.write(material)

    tiled1 = capture(uv_fixture("uv_scale1", 1.0), "uv_scale1")
    tiled2 = capture(uv_fixture("uv_scale2", 2.0), "uv_scale2")
    offset = capture(uv_fixture("uv_offset", 1.0, [0.5, 0.0]), "uv_offset")
    check(band_colors(tiled1, 4) == [RED, GREEN, BLUE, WHITE],
          f"UVScale 1: four bands in texture order {band_colors(tiled1, 4)}")
    check(band_colors(tiled2, 8) == [RED, GREEN, BLUE, WHITE] * 2,
          f"UVScale 2: the four bands repeat twice {band_colors(tiled2, 8)}")
    check(band_colors(offset, 4) == [BLUE, WHITE, RED, GREEN],
          f"UVOffset 0.5 shifts the bands by half the texture {band_colors(offset, 4)}")

    # ----------------------------------------------------------------- normal ---
    print("\n[normal: strength, DirectX flip, scale 0 neutral]", flush=True)

    def normal_fixture(name, x, y, scale=None, convention=None):
        fixture = Fixture(name)
        detail_normal = fixture.add_ktx2_texture(make_ktx2(142, 4, 4, [bc5_block(x, y)]))
        material = base_material(base_color=(0.5, 0.5, 0.5, 1.0), extra={
            "extensions": {DETAIL: detail_extension(normal=detail_normal, uv_scale=1.0,
                                                    normal_scale=scale)}})
        return fixture.write(material, [MSFT_DDS], asset_convention=convention)

    def no_detail_normal_fixture():
        fixture = Fixture("normal_none")
        flat = fixture.add_png_texture(flat_normal_png())
        material = base_material(base_color=(0.5, 0.5, 0.5, 1.0), extra={
            "normalTexture": {"index": flat}})
        return fixture.write(material)

    def neutral_normal_fixture(name, scale=None):
        # A base normal map keeps both fixtures on the base-normal path, so the detail
        # normal's identity is what is compared.
        fixture = Fixture(name)
        flat = fixture.add_png_texture(flat_normal_png())
        detail_normal = fixture.add_ktx2_texture(make_ktx2(142, 4, 4, [bc5_block(0.6, 0.6)]))
        material = base_material(base_color=(0.5, 0.5, 0.5, 1.0), extra={
            "normalTexture": {"index": flat},
            "extensions": {DETAIL: detail_extension(normal=detail_normal, uv_scale=1.0,
                                                    normal_scale=scale)}})
        return fixture.write(material, [MSFT_DDS])

    directx = capture(normal_fixture("normal_directx", 0.6, 0.6, 1.0, "DirectX"), "normal", "shadingnormal")
    opengl = capture(normal_fixture("normal_opengl", 0.6, -0.6, 1.0, None), "normal_opengl", "shadingnormal")
    check(same(directx, opengl),
          "DirectX flip: signed +Y under DirectX equals -Y without the convention")
    neutral_zero = capture(neutral_normal_fixture("normal_scale0", 0.0), "normal_scale0", "shadingnormal")
    plain_normal = capture(no_detail_normal_fixture(), "normal_none", "shadingnormal")
    check(same(neutral_zero, plain_normal),
          "detailNormalTexture.scale 0 is byte-identical to no detail normal")
    full = capture(neutral_normal_fixture("normal_scale1", 1.0), "normal_scale1", "shadingnormal")
    half = capture(neutral_normal_fixture("normal_scale_half", 0.5), "normal_scale_half", "shadingnormal")
    reference = image(plain_normal)
    if reference is not None and image(full) is not None and image(half) is not None:
        def mean_distance(a_path):
            _, _, channels, data = image(a_path)
            reference_data = reference[3]
            total = 0
            count = 0
            for offset in range(0, len(data), channels):
                total += abs(data[offset] - reference_data[offset])
                count += 1
            return total / count
        check(mean_distance(full) > mean_distance(half) > 0.0,
              f"strength is monotonic: scale 1 distance {mean_distance(full):.2f} > "
              f"scale 0.5 distance {mean_distance(half):.2f} > 0")

    # ------------------------------------------------------------------ blend ---
    print("\n[blend: blendMaskTexture.r with blendThreshold]", flush=True)

    def blend_fixture(name, mask_value, threshold, alpha=0.5):
        fixture = Fixture(name)
        mask = fixture.add_png_texture(solid_png((mask_value, mask_value, mask_value, 255)))
        color = fixture.add_png_texture(solid_png((255, 0, 0, 255)))
        material = base_material(base_color=(1.0, 1.0, 1.0, 1.0), extra={
            "extensions": {DETAIL: detail_extension(color=color, blend_mask=mask, uv_scale=1.0,
                                                    threshold=threshold)}})
        return fixture.write(material, color_alpha=alpha)

    mask0 = center(capture(blend_fixture("blend_mask0", 0, 0.1), "blend_mask0"))
    mask1 = center(capture(blend_fixture("blend_mask1", 255, 0.1), "blend_mask1"))
    check(close(mask0, (255, 255, 255)), f"mask 0 keeps the base colour {mask0}")
    check(close(mask1, (255, 0, 0)), f"mask 1 applies the detail colour {mask1}")

    vA = 0.5                      # COLOR_0 alpha, stored as a float
    mask_mid = 153.0 / 255.0      # 0.6 exactly
    narrow = detail_weight(vA, mask_mid, 0.1)
    wide = detail_weight(vA, mask_mid, 0.5)
    narrow_expected = (255, srgb(1.0 - narrow), srgb(1.0 - narrow))
    wide_expected = (255, srgb(1.0 - wide), srgb(1.0 - wide))
    narrow_center = center(capture(blend_fixture("blend_edge_narrow", 153, 0.1), "blend_edge_narrow"))
    wide_center = center(capture(blend_fixture("blend_edge_wide", 153, 0.5), "blend_edge_wide"))
    check(close(narrow_center, narrow_expected),
          f"blendThreshold 0.1 edge: {narrow_center} == {narrow_expected}")
    check(close(wide_center, wide_expected),
          f"blendThreshold 0.5 edge: {wide_center} == {wide_expected}")
    check(not close(narrow_center, wide_center, tolerance=3),
          "the threshold edge width changes the blended coverage")

    # ------------------------------------------------------------------ alpha ---
    print("\n[alpha: detailColorTexture.a multiplies the material alpha]", flush=True)

    def alpha_fixture(name, alpha_byte):
        fixture = Fixture(name)
        color = fixture.add_png_texture(solid_png((255, 0, 0, alpha_byte)))
        material = base_material(base_color=(1.0, 1.0, 1.0, 1.0), extra={
            "alphaMode": "MASK", "alphaCutoff": 0.5,
            "extensions": {DETAIL: detail_extension(color=color, uv_scale=1.0)}})
        return fixture.write(material)

    below = center(capture(alpha_fixture("alpha_below", 100), "alpha_below", None))
    above = center(capture(alpha_fixture("alpha_above", 200), "alpha_above", None))
    check(close(below, (0, 0, 0)), f"detail alpha 0.392 < cutoff 0.5 discards the MASK fragment {below}")
    check(above is not None and above != (0, 0, 0),
          f"detail alpha 0.784 > cutoff 0.5 keeps the MASK fragment {above}")

    # ------------------------------------------------------------------ inert ---
    print("\n[inert: no detail layer means no shader variant]", flush=True)
    inert_plain = Fixture("inert_plain")
    inert_plain_path = inert_plain.write(base_material(), declare_detail=False)
    inert_mask_only = Fixture("inert_mask_only")
    only_mask = inert_mask_only.add_png_texture(solid_png((0, 0, 0, 255)))
    inert_mask_only_path = inert_mask_only.write(base_material(extra={
        "extensions": {DETAIL: detail_extension(blend_mask=only_mask, uv_scale=2.0)}}))
    check(same(capture(inert_mask_only_path, "inert_mask_only"),
               capture(inert_plain_path, "inert_plain")),
          "a blendMaskTexture alone leaves the material byte-identical")

    # ----------------------------------------------------------------- switch ---
    print("\n[switch: --asobo-disable ASOBO_material_detail_map]", flush=True)
    switch = Fixture("switch")
    switch_texture = switch.add_png_texture(bands_png)
    switch_path = switch.write(base_material(base_color=(1.0, 1.0, 1.0, 1.0), extra={
        "extensions": {DETAIL: detail_extension(color=switch_texture, uv_scale=2.0)}}))
    switch_plain = Fixture("switch_plain")
    switch_plain.add_png_texture(bands_png)
    switch_plain_path = switch_plain.write(base_material(base_color=(1.0, 1.0, 1.0, 1.0)),
                                          declare_detail=False)
    switched_off = capture(switch_path, "switch_off", extra=("--asobo-disable", DETAIL))
    plain_reference = capture(switch_plain_path, "switch_plain")
    check(same(switched_off, plain_reference),
          "the extension switched off is byte-identical to the material without it")
    check(not same(capture(switch_path, "switch_on"), plain_reference),
          "with the switch on the detail layer changes the frame")

    # ------------------------------------------------------------- windshield ---
    # On windshield materials the detail slots are the windshield masks, detail reflection
    # and icing normal (MSFS SDK), so the generic detail map must not apply to them.
    print("\n[windshield: the detail map is not applied to windshield materials]", flush=True)

    def windshield_fixture(name, windshield_ext, with_detail, extras=None):
        fixture = Fixture(name)
        texture = fixture.add_png_texture(bands_png)
        extensions = dict(windshield_ext)
        if with_detail:
            extensions[DETAIL] = detail_extension(color=texture, uv_scale=5.0)
        material = base_material(base_color=(1.0, 1.0, 1.0, 1.0), extra={"extensions": extensions})
        if extras is not None:
            material["extras"] = extras
        return fixture.write(material, extensions_used=list(windshield_ext))

    for label, ext, extras in (("v3", {"ASOBO_material_windshield_v3": {"UVScale": 5.0}}, None),
                               ("v1", {"ASOBO_material_windshield": {}}, None),
                               ("code", {}, {"ASOBO_material_code": "Windshield"})):
        with_detail = capture(windshield_fixture(f"windshield_{label}_detail", ext, True, extras), f"windshield_{label}_detail")
        without = capture(windshield_fixture(f"windshield_{label}_plain", ext, False, extras), f"windshield_{label}_plain")
        check(same(with_detail, without),
              f"windshield {label}: a detail map on a windshield material renders byte-identically to none")

    # ----------------------------------------------------------------- decal ---
    print("\n[decal: the accumulation pass evaluates the same material_info path]", flush=True)

    def decal_fixture(name, with_detail):
        fixture = Fixture(name)
        color = fixture.add_png_texture(solid_png((0, 255, 0, 255)))
        material = {"alphaMode": "BLEND", "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.5},
            "extensions": {"ASOBO_material_geometry_decal": {}}}
        if with_detail:
            material["extensions"][DETAIL] = detail_extension(color=color, uv_scale=1.0)
        return fixture.write(material, extensions_used=["ASOBO_material_geometry_decal"],
                             declare_detail=with_detail, half=0.15, z=0.01, receiver=(0.1, 0.1, 0.1))

    detail_decal = center(capture(decal_fixture("decal_detail", True), "decal_detail"))
    plain_decal = center(capture(decal_fixture("decal_plain", False), "decal_plain"))
    check(close(detail_decal, (0, 255, 0)),
          f"decal accumulation applies the detail colour: {detail_decal}")
    check(close(plain_decal, (255, 255, 255)),
          f"the same decal without the detail map keeps its white base: {plain_decal}")


    print("\n--- Summary ---", flush=True)
    if failures:
        print(f"FAILED ({len(failures)} failures):", flush=True)
        for failure in failures:
            print("  - " + failure, flush=True)
        sys.exit(1)
    print("ALL TESTS PASSED!", flush=True)


if __name__ == "__main__":
    main()
