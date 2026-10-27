# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A4.2 Windshield layers validation.

Validates windshield layers on ASOBO_material_windshield_v3, ASOBO_material_windshield (v1)
and extras.ASOBO_material_code: "Windshield":
1. Each mask gated by its vertex-colour channel (R: details 1, G: secondary details, B: details 2, A: icing).
2. Parameter 0 = no change (detail1/2Rough/Opacity = 0, microScratchesStrength = 0).
3. Micro-scratches: scratchesNormalTexture perturb normal when strength > 0, no change when 0.
4. Icing: setting 0 is byte-identical to no icing; setting 1 applies frost only where G * vertex alpha > 0.
5. Windshield switch off is byte-identical to 99de26c.exe windshield-off rendering.
6. Non-windshield models (HelloCube) are byte-identical to 99de26c.exe.
7. Zero GL errors.

Run: python scripts/validate_asobo_windshield_layers.py
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

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASELINE = REPO / "build" / "baselines" / "99de26c.exe"
OUT = REPO / "build" / "asobo_windshield"
OUT.mkdir(parents=True, exist_ok=True)

CAMERA = ["--isolated", "--window-size", "800x600", "--aa", "none", "--clear-color", "0.05,0.05,0.05",
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


def make_png(width, height, rows):
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(
            ">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(channel for pixel in row for channel in pixel) for row in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def solid_png(color):
    return make_png(1, 1, [[color]])


class Fixture:
    def __init__(self, name):
        self.name = name
        self.images = []
        self.textures = []
        self.files = {}
        self.accessors = []
        self.buffer_views = []
        self.binary = bytearray()

    def add_png_texture(self, png):
        self.images.append({"uri": "data:image/png;base64," + base64.b64encode(png).decode("ascii")})
        texture = {"source": len(self.images) - 1, "sampler": 0}
        self.textures.append(texture)
        return len(self.textures) - 1

    def _add_quad(self, z, half, color_rgba):
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
        if color_rgba is not None:
            index_offset = offset + 192
            self.binary += b"".join(struct.pack("<4f", *color_rgba) for _ in positions)
            self.buffer_views.append({"buffer": 0, "byteOffset": offset + 128, "byteLength": 64, "target": 34962})
            self.accessors.append({"bufferView": base_view + 3, "componentType": 5126, "count": 4, "type": "VEC4"})
            attributes["COLOR_0"] = base_accessor + 3
        self.binary += struct.pack("<6H", *indices)
        self.buffer_views.append({"buffer": 0, "byteOffset": index_offset, "byteLength": 12, "target": 34963})
        self.accessors.append({"bufferView": len(self.buffer_views) - 1, "componentType": 5123, "count": 6, "type": "SCALAR"})
        return attributes, len(self.accessors) - 1

    def write(self, material, color_rgba=None, half=0.4, z=0.0):
        attributes, index_accessor = self._add_quad(z, half, color_rgba)
        meshes = [{"name": self.name, "primitives": [{"attributes": attributes, "indices": index_accessor, "material": 0}]}]
        nodes = [{"name": self.name, "mesh": 0}]
        materials = [{**material, "name": self.name}]
        samplers = [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}]
        buffers = [{"byteLength": len(self.binary), "uri": "data:application/octet-stream;base64," + base64.b64encode(self.binary).decode("ascii")}]

        used = ["ASOBO_material_windshield_v3", "ASOBO_material_detail_map"]
        if "ASOBO_material_windshield" in material.get("extensions", {}):
            used.append("ASOBO_material_windshield")

        document = {
            "asset": {"version": "2.0", "generator": "A4.2 windshield validation fixture"},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": nodes,
            "meshes": meshes,
            "materials": materials,
            "textures": self.textures,
            "images": self.images,
            "samplers": samplers,
            "buffers": buffers,
            "bufferViews": self.buffer_views,
            "accessors": self.accessors,
            "extensionsUsed": used,
        }

        path = OUT / f"{self.name}.gltf"
        path.write_text(json.dumps(document, indent=2), encoding="utf-8")
        return path


def capture(exe, model, label, view="shaded", extra=()):
    png = OUT / f"{label}.png"
    report = OUT / f"{label}.json"
    log_path = OUT / f"{label}.log"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)

    arguments = [str(exe), str(model), *CAMERA, *extra]
    if view is not None:
        arguments += ["--debug-view", view]
    arguments += ["--capture-viewport", str(png), "--capture-frame", "8",
                  "--profile-report", str(report), "--exit-after-profile"]
    result = subprocess.run(arguments, cwd=REPO, capture_output=True, text=True, timeout=120)
    log = result.stdout + result.stderr
    log_path.write_text(log, encoding="utf-8")
    if result.returncode != 0 or not png.is_file():
        check(False, f"{label}: capture failed ({result.returncode})")
        return None
    if "GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x", log):
        check(False, f"{label}: GL diagnostics failed")
        return None
    return png


def center(path):
    decoded = _read_png(path) if path is not None else None
    if decoded is None:
        return None
    width, height, channels, data = decoded
    offset = ((height // 2) * width + width // 2) * channels
    return tuple(data[offset:offset + 3])


def mean_abs_diff(p1, p2):
    d1 = _read_png(p1)
    d2 = _read_png(p2)
    if d1 is None or d2 is None:
        return 999.0
    w1, h1, c1, b1 = d1
    w2, h2, c2, b2 = d2
    if (w1, h1, c1) != (w2, h2, c2):
        return 999.0
    total = sum(abs(x - y) for x, y in zip(b1, b2))
    return total / len(b1)


def validate_light_scattering():
    """A4.2.1: isolate opacity scattering from roughness and specular changes."""
    import copy
    import numpy as np
    from PIL import Image

    for channel in (0, 2):
        rows = []
        for y in range(32):
            row = []
            for x in range(32):
                pixel = [0, 0, 0, 255]
                # Fine scratch and a low-valued fingerprint patch, respectively.
                pixel[channel] = (128 if x % 8 == 0 else 0) if channel == 0 else (56 if (x - 16)**2 + (y - 16)**2 < 64 else 0)
                row.append(tuple(pixel))
            rows.append(row)
        parameter = "detail1Opacity" if channel == 0 else "detail2Opacity"
        material = {
            "alphaMode": "BLEND",
            "pbrMetallicRoughness": {"baseColorFactor": [0.385, 0.385, 0.385, 0.01], "metallicFactor": 0, "roughnessFactor": 0.1},
            "extensions": {"ASOBO_material_windshield_v3": {parameter: 0.5},
                           "ASOBO_material_detail_map": {"detailColorTexture": {"index": 0}}}}
        vertex = [0, 0, 0, 0]
        vertex[channel] = 1
        models = {}
        for variant in ("on", "parameter_zero", "mask_zero", "vertex_zero", "absent"):
            f = Fixture(f"scatter_{channel}_{variant}")
            f.add_png_texture(solid_png((0, 0, 0, 255)) if variant == "mask_zero" else make_png(32, 32, rows))
            m = copy.deepcopy(material)
            if variant == "parameter_zero":
                m["extensions"]["ASOBO_material_windshield_v3"][parameter] = 0
            if variant == "absent":
                del m["extensions"]["ASOBO_material_detail_map"]
            models[variant] = f.write(m, [0, 0, 0, 0] if variant == "vertex_zero" else vertex)
        for background in ("0.02,0.02,0.02", "0.5,0.5,0.5"):
            name = "dark" if background.startswith("0.02") else "bright"
            caps = {v: capture(EXE, model, f"scatter_{channel}_{name}_{v}", extra=["--clear-color", background])
                    for v, model in models.items()}
            for variant in ("parameter_zero", "mask_zero", "vertex_zero"):
                check(same(caps[variant], caps["absent"]), f"detail {channel}: {variant} = absent byte-for-byte on {name}")
            if caps["on"] and caps["absent"]:
                delta = np.array(Image.open(caps["on"]).convert("RGB"), dtype=np.int16) - np.array(Image.open(caps["absent"]).convert("RGB"), dtype=np.int16)
                check(int(delta.max()) > 2 and int((delta.max(2) > 2).sum()) > 50,
                      f"detail {channel}: light lines/haze on {name}: peak {delta.max()}/255, {(delta.max(2) > 2).sum()} pixels >2")
                check(int(delta.min()) >= 0, f"detail {channel}: scattering never darkens {name} (min {delta.min()})")
            # The whole treatment still belongs to the per-viewport switch.
            off = capture(EXE, models["on"], f"scatter_{channel}_{name}_switch_off", extra=["--clear-color", background, "--asobo-disable", "ASOBO_material_windshield_v3"])
            zero_off = capture(EXE, models["parameter_zero"], f"scatter_{channel}_{name}_zero_switch_off", extra=["--clear-color", background, "--asobo-disable", "ASOBO_material_windshield_v3"])
            check(same(off, zero_off), f"detail {channel}: viewport switch disables scattering on {name}")


def main():
    print("=== A4.2 ASOBO Windshield Layers Validation ===")

    # Textures
    # Mask texture: R = 255 (details 1), G = 255 (icing), B = 255 (details 2), A = 255
    tex_masks = solid_png((255, 255, 255, 255))
    # Secondary details (dirt texture): RGBA
    tex_sec = solid_png((180, 140, 100, 200))
    # Detail reflection ORM: R = 128 (reflection), G = 200 (roughness offset), B = 128
    tex_orm = solid_png((128, 200, 128, 255))
    # Icing normal: tilted normal
    tex_norm = solid_png((180, 128, 200, 255))
    # Micro-scratches normal: perturbed normal
    tex_scratches = solid_png((200, 100, 200, 255))

    # 1. Mask gating by vertex colour
    print("\n[1. Mask gating by vertex colour]")
    # Details 1 (R): detail1Rough: 1.0, detail1Opacity: 0.5
    f_d1_on = Fixture("d1_gated_on")
    t_mask = f_d1_on.add_png_texture(tex_masks)
    m_d1 = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {"detail1Rough": 1.0, "detail1Opacity": 0.5},
            "ASOBO_material_detail_map": {"detailColorTexture": {"index": t_mask}, "UVScale": 1.0}
        }
    }
    p_d1_on = f_d1_on.write(m_d1, color_rgba=[1.0, 0.0, 0.0, 0.0])
    cap_d1_on = capture(EXE, p_d1_on, "d1_on")

    f_d1_off = Fixture("d1_gated_off")
    t_mask = f_d1_off.add_png_texture(tex_masks)
    p_d1_off = f_d1_off.write(m_d1, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_d1_off = capture(EXE, p_d1_off, "d1_off")

    f_d1_zero = Fixture("d1_param_zero")
    t_mask = f_d1_zero.add_png_texture(tex_masks)
    m_d1_zero = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {"detail1Rough": 0.0, "detail1Opacity": 0.0},
            "ASOBO_material_detail_map": {"detailColorTexture": {"index": t_mask}, "UVScale": 1.0}
        }
    }
    p_d1_zero = f_d1_zero.write(m_d1_zero, color_rgba=[1.0, 0.0, 0.0, 0.0])
    cap_d1_zero = capture(EXE, p_d1_zero, "d1_param_zero")

    diff_d1 = mean_abs_diff(cap_d1_on, cap_d1_off)
    check(diff_d1 > 1.0, f"Detail 1 active when vertex color R=1: diff {diff_d1:.2f} > 1.0")
    check(same(cap_d1_off, cap_d1_zero), "Detail 1 gated off (vertex R=0) equals detail1Rough/Opacity=0 byte-for-byte")

    # Details 2 (B): detail2Rough: 1.0, detail2Opacity: 0.5
    f_d2_on = Fixture("d2_gated_on")
    t_mask = f_d2_on.add_png_texture(tex_masks)
    m_d2 = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {"detail2Rough": 1.0, "detail2Opacity": 0.5},
            "ASOBO_material_detail_map": {"detailColorTexture": {"index": t_mask}, "UVScale": 1.0}
        }
    }
    p_d2_on = f_d2_on.write(m_d2, color_rgba=[0.0, 0.0, 1.0, 0.0])
    cap_d2_on = capture(EXE, p_d2_on, "d2_on")

    f_d2_off = Fixture("d2_gated_off")
    t_mask = f_d2_off.add_png_texture(tex_masks)
    p_d2_off = f_d2_off.write(m_d2, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_d2_off = capture(EXE, p_d2_off, "d2_off")

    diff_d2 = mean_abs_diff(cap_d2_on, cap_d2_off)
    check(diff_d2 > 1.0, f"Detail 2 active when vertex color B=1: diff {diff_d2:.2f} > 1.0")

    # Secondary Details (G): emissiveTexture
    f_sec_on = Fixture("sec_gated_on")
    t_sec = f_sec_on.add_png_texture(tex_sec)
    m_sec = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "emissiveTexture": {"index": t_sec},
        "extensions": {
            "ASOBO_material_windshield_v3": {}
        }
    }
    p_sec_on = f_sec_on.write(m_sec, color_rgba=[0.0, 1.0, 0.0, 0.0])
    cap_sec_on = capture(EXE, p_sec_on, "sec_on")

    f_sec_off = Fixture("sec_gated_off")
    t_sec = f_sec_off.add_png_texture(tex_sec)
    p_sec_off = f_sec_off.write(m_sec, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_sec_off = capture(EXE, p_sec_off, "sec_off")

    diff_sec = mean_abs_diff(cap_sec_on, cap_sec_off)
    check(diff_sec > 1.0, f"Secondary details active when vertex color G=1: diff {diff_sec:.2f} > 1.0")

    # 2. Parameters 0 = no change
    print("\n[2. Parameter 0 = no change]")
    f_d2_zero = Fixture("d2_param_zero")
    t_mask = f_d2_zero.add_png_texture(tex_masks)
    m_d2_zero = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {"detail2Rough": 0.0, "detail2Opacity": 0.0},
            "ASOBO_material_detail_map": {"detailColorTexture": {"index": t_mask}, "UVScale": 1.0}
        }
    }
    p_d2_zero = f_d2_zero.write(m_d2_zero, color_rgba=[0.0, 0.0, 1.0, 0.0])
    cap_d2_zero = capture(EXE, p_d2_zero, "d2_param_zero")
    check(same(cap_d2_off, cap_d2_zero), "detail2Rough/Opacity=0 with vertex B=1 is byte-identical to gated off (vertex B=0)")

    # 3. Micro-scratches
    print("\n[3. Micro-scratches]")
    f_scratch_on = Fixture("scratch_on")
    t_scr = f_scratch_on.add_png_texture(tex_scratches)
    m_scr = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {"microScratchesStrength": 1.0, "scratchesNormalTexture": {"index": t_scr}}
        }
    }
    p_scr_on = f_scratch_on.write(m_scr, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_scr_on = capture(EXE, p_scr_on, "scratch_on")

    f_scratch_zero = Fixture("scratch_zero")
    t_scr = f_scratch_zero.add_png_texture(tex_scratches)
    m_scr_zero = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {"microScratchesStrength": 0.0, "scratchesNormalTexture": {"index": t_scr}}
        }
    }
    p_scr_zero = f_scratch_zero.write(m_scr_zero, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_scr_zero = capture(EXE, p_scr_zero, "scratch_zero")

    f_scratch_none = Fixture("scratch_none")
    m_scr_none = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {}
        }
    }
    p_scr_none = f_scratch_none.write(m_scr_none, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_scr_none = capture(EXE, p_scr_none, "scratch_none")

    diff_scr = mean_abs_diff(cap_scr_on, cap_scr_zero)
    check(diff_scr > 0.5, f"Micro-scratches modify normal when strength=1: diff {diff_scr:.2f} > 0.5")
    check(same(cap_scr_zero, cap_scr_none), "microScratchesStrength=0 is byte-identical to no scratches texture")

    # 4. Windshield Icing
    print("\n[4. Windshield Icing]")
    f_ice = Fixture("ice_test")
    t_mask = f_ice.add_png_texture(tex_masks)
    t_norm = f_ice.add_png_texture(tex_norm)
    m_ice = {
        "alphaMode": "BLEND",
        "pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.1, 0.1, 0.05], "roughnessFactor": 0.0, "metallicFactor": 0.0},
        "extensions": {
            "ASOBO_material_windshield_v3": {},
            "ASOBO_material_detail_map": {"detailColorTexture": {"index": t_mask}, "detailNormalTexture": {"index": t_norm}, "UVScale": 1.0}
        }
    }
    p_ice_vertex_on = f_ice.write(m_ice, color_rgba=[0.0, 0.0, 0.0, 1.0])
    cap_ice_0 = capture(EXE, p_ice_vertex_on, "ice_icing0", extra=["--windshield-icing", "0.0"])
    cap_ice_default = capture(EXE, p_ice_vertex_on, "ice_icing_default")
    check(same(cap_ice_0, cap_ice_default), "Windshield icing 0 is byte-identical to default (no icing)")
    check(same(cap_ice_0, cap_scr_none), "Windshield icing 0 is byte-identical to icing textures absent")

    cap_ice_1 = capture(EXE, p_ice_vertex_on, "ice_icing1", extra=["--windshield-icing", "1.0"])
    diff_ice = mean_abs_diff(cap_ice_0, cap_ice_1)
    check(diff_ice > 5.0, f"Windshield icing 1 creates visible frost: diff {diff_ice:.2f} > 5.0")

    # Icing gated by vertex alpha
    f_ice_off = Fixture("ice_vertex_off")
    t_mask = f_ice_off.add_png_texture(tex_masks)
    t_norm = f_ice_off.add_png_texture(tex_norm)
    p_ice_vertex_off = f_ice_off.write(m_ice, color_rgba=[0.0, 0.0, 0.0, 0.0])
    cap_ice_v_off = capture(EXE, p_ice_vertex_off, "ice_vertex_off", extra=["--windshield-icing", "1.0"])
    check(same(cap_ice_0, cap_ice_v_off), "Icing=1 with vertex alpha=0 is byte-identical to icing=0 (coverage=0)")

    # 5. Windshield switch off matches baseline
    print("\n[5. Windshield switch off matches baseline]")
    if BASELINE.is_file():
        cap_v3_off_new = capture(EXE, p_d1_on, "v3_switch_off_new", extra=["--asobo-disable", "ASOBO_material_windshield_v3"])
        cap_v3_off_base = capture(BASELINE, p_d1_on, "v3_switch_off_base", extra=["--asobo-disable", "ASOBO_material_windshield_v3"])
        check(same(cap_v3_off_new, cap_v3_off_base), "windshield v3 switch off is byte-identical to 99de26c.exe windshield-off")
    else:
        print("  SKIP baseline comparison (baseline exe not found)")

    # 6. Non-windshield models byte-identical
    print("\n[6. Non-windshield models byte-identical]")
    sample_glb = REPO / "resources" / "sampleGltf" / "HelloCube.glb"
    if BASELINE.is_file() and sample_glb.is_file():
        cap_cube_new = capture(EXE, sample_glb, "hellocube_new")
        cap_cube_base = capture(BASELINE, sample_glb, "hellocube_base")
        check(same(cap_cube_new, cap_cube_base), "HelloCube is byte-identical to 99de26c.exe baseline")

    e195_ext = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Exterior-LOD00.gltf")
    if BASELINE.is_file() and e195_ext.is_file():
        import glob
        tex_dirs = glob.glob(str(e195_ext.parent.parent / "texture*"))
        search = []
        for td in tex_dirs:
            search += ["--search-path", td]
        cap_ext_new = capture(EXE, e195_ext, "e195_ext_new", extra=search)
        cap_ext_base = capture(BASELINE, e195_ext, "e195_ext_base", extra=search)
        check(same(cap_ext_new, cap_ext_base), "E195 exterior is byte-identical to 99de26c.exe baseline")

    print("\n[7. Light-scattering opacity on bright/dark backgrounds]")
    validate_light_scattering()

    print("\n" + "=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
