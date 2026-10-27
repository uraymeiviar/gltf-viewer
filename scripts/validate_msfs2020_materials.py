"""Legacy MSFS glass mappings and reproducible PMDG ablations; no simulator config reads.

Run without arguments for synthetic mapping checks; --pmdg for aircraft captures.
All aircraft inputs are read-only. Derived files are confined to build/review/pmdg.
"""
import copy
import base64
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image
import harness_process as subprocess
import validate_asobo_glass as glass

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/pmdg"
EXE = REPO / "build/bin/Release/Glider.exe"
BASE = REPO / "build/baselines/8e932ca.exe"
LEGACY = ("ASOBO_material_glass", "ASOBO_material_kitty_glass", "ASOBO_material_windshield")
OVERHEAD = ["--camera-target", "0,1.8,15.5", "--camera-yaw", "0", "--camera-pitch", "-65",
            "--camera-distance", ".9", "--exposure", "2"]


def mappings():
    OUT.mkdir(parents=True, exist_ok=True)
    glass.OUT = OUT
    reference = glass.fixture("legacy_reference", normal=(.985, 0, .174))
    plain = glass.fixture("legacy_plain", None, normal=(.985, 0, .174))
    plain_png = glass.render(plain, "legacy_plain")
    glass.check(glass.same(plain_png, glass.render(plain, "legacy_plain_baseline", exe=BASE)),
                "no legacy declarations: byte-identical to 8e932ca")
    for ext in LEGACY:
        model = glass.fixture(ext, ext, normal=(.985, 0, .174))
        if ext != LEGACY[2]:
            doc = json.loads(model.read_text())
            doc["materials"][0]["extensions"][ext]["glassReflectionMaskFactor"] = 1
            model.write_text(json.dumps(doc))
        on = glass.render(model, ext)
        off = glass.render(model, ext+"_off", ["--asobo-disable", ext])
        glass.check(glass.same(off, plain_png), ext+": own switch restores core glTF exactly")
        glass.check(not glass.same(on, off), ext+": reflection visible at grazing incidence")
        if ext != LEGACY[2]:
            glass.check(glass.same(on, glass.render(reference, ext+"_reference")), ext+": zero-width v2 path")
            doc = json.loads(model.read_text())
            doc["materials"][0]["extensions"][ext]["glassReflectionMaskFactor"] = 0
            model.write_text(json.dumps(doc))
            zero = glass.render(model, ext+"_zero")
            glass.check(np.mean(glass.pixels(on)) > np.mean(glass.pixels(zero))+2, ext+": mask zero removes reflection")
            doc["materials"][0]["extensions"][ext].pop("glassReflectionMaskFactor")
            model.write_text(json.dumps(doc))
            glass.check(glass.same(zero, glass.render(model, ext+"_default")), ext+": omitted mask uses SDK default zero")
            # Presence without extensionsUsed must have no rendering effect.
            doc["extensionsUsed"].remove(ext)
            model.write_text(json.dumps(doc))
            glass.check(glass.same(glass.render(model, ext+"_undeclared"), plain_png), ext+": undeclared ignored")
        else:
            doc = json.loads(model.read_text())
            doc["materials"][0]["extensions"][ext].update(rainDropScale=7, wiper1State=.8, wiper4State=.5)
            model.write_text(json.dumps(doc))
            glass.check(glass.same(on, glass.render(model, ext+"_weather")), "v1 rain/wiper values have no static effect")
            add_mask_colors(doc)
            model.write_text(json.dumps(doc))
            glass.check(glass.same(on, glass.render(model, ext+"_vertex_masks")), "v1 vertex masks do not tint or hide dry glass")
    for code in ("Glass", "Windshield", "Porthole", "GeoDecal", "Unknown"):
        doc = json.loads(plain.read_text())
        doc["materials"][0]["extras"] = {"ASOBO_material_code": code}
        path = OUT / ("code_"+code+".gltf")
        path.write_text(json.dumps(doc))
        on = glass.render(path, "code_"+code)
        off = glass.render(path, "code_"+code+"_off", ["--asobo-disable", "ASOBO_material_code"])
        glass.check(glass.same(off, plain_png), code+": extras display switch")
        glass.check((not glass.same(on, plain_png)) == (code in ("Glass", "Windshield")), code+": exact type mapping")
        if code == "Windshield":
            add_mask_colors(doc)
            path.write_text(json.dumps(doc))
            glass.check(glass.same(on, glass.render(path, "code_Windshield_masks")), "extras windshield ignores vertex weather masks as colour")
    if glass.failures:
        raise SystemExit(1)


def add_mask_colors(doc):
    data = base64.b64decode(doc["buffers"][0]["uri"].split(",", 1)[1])
    offset = len(data)
    data += struct.pack("<16f", *([0, .2, .8, 0]*4))
    doc["buffers"][0].update(byteLength=len(data), uri="data:application/octet-stream;base64,"+base64.b64encode(data).decode())
    doc["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": 64})
    doc["accessors"].append({"bufferView": len(doc["bufferViews"])-1, "componentType": 5126, "count": 4, "type": "VEC4"})
    doc["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = len(doc["accessors"])-1


def capture(model, label, flags=(), exe=EXE):
    png = OUT / ("m2020_"+label+".png")
    png.unlink(missing_ok=True)
    args = [str(exe), str(model), "--isolated", "--hide-grid", "--window-size", "1200x900",
            "--fixed-frame-time", ".05", "--aa", "none", "--vsync", "off",
            "--capture-viewport", str(png), "--capture-frame", "12",
            "--profile-report", str(png.with_suffix(".json")), "--exit-after-profile", *flags]
    result = subprocess.run(args, cwd=REPO, capture_output=True, timeout=600)
    log = result.stdout + result.stderr
    png.with_suffix(".log").write_bytes(log)
    if result.returncode in (0xc0000142, -1073741502):
        raise SystemExit("0xc0000142: do not launch Glider again in this session")
    if result.returncode or not png.exists() or b"GL automation errors: 0" not in log:
        raise RuntimeError(f"{label}: exit {result.returncode}, capture/GL diagnostics failed")
    return png


def pmdg():
    model = OUT / "vc.gltf"
    cameras = {
        "overhead": OVERHEAD,
        "windshield": ["--camera-target", "0,1.2,15.4", "--camera-yaw", "180", "--camera-pitch", "0", "--camera-distance", "1.5"],
        "standby": ["--select-node", "bl_main_standby", "--camera-yaw", "180", "--camera-distance", ".5", "--exposure", "2"],
    }
    for name, flags in cameras.items():
        for suffix, exe in (("before", BASE), ("after", EXE)):
            capture(model, name+"_"+suffix, flags, exe)
    doc = json.loads(model.read_text())
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            meta = primitive.get("extras", {}).get("ASOBO_primitive", {})
            accessor = doc["accessors"][primitive["indices"]]
            start, count = meta.get("StartIndex", 0), meta.get("PrimitiveCount", accessor["count"]//3)*3
            if start or count != accessor["count"]:
                sliced = copy.deepcopy(accessor)
                sliced["byteOffset"] = sliced.get("byteOffset", 0)+start*{5121: 1, 5123: 2, 5125: 4}[sliced["componentType"]]
                sliced["count"] = count
                doc["accessors"].append(sliced)
                primitive["indices"] = len(doc["accessors"])-1
                meta.pop("StartIndex", None)
                meta.pop("PrimitiveCount", None)
    reference = OUT / "vc_explicit_index_slices.gltf"
    reference.write_text(json.dumps(doc))
    disable_glass = ["--asobo-disable", "ASOBO_material_kitty_glass", "--asobo-disable", "ASOBO_material_code"]
    for name, flags in cameras.items():
        a = capture(reference, name+"_range_reference", flags, BASE)
        b = capture(model, name+"_range_only", flags+disable_glass)
        if a.read_bytes() != b.read_bytes():
            raise RuntimeError(name+": explicit-index baseline differs from native slices with legacy glass off")
    e195 = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\Packages\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf")
    a = capture(e195, "e195_before", exe=BASE)
    b = capture(e195, "e195_after")
    if a.read_bytes() != b.read_bytes():
        raise RuntimeError("E195 package must remain byte-identical to 8e932ca")
    print("PMDG three-camera explicit-slice references and packaged E195: byte-identical", flush=True)


def ablations():
    original = json.loads((OUT / "vc.gltf").read_text())
    # Recreate the old whole-accessor behavior with the final executable; no old
    # executable copies or alternate renderer build are needed for ablations.
    for mesh in original["meshes"]:
        for primitive in mesh["primitives"]:
            meta = primitive.get("extras", {}).get("ASOBO_primitive", {})
            meta.pop("StartIndex", None)
            meta.pop("PrimitiveCount", None)
    no_ranges = OUT / "vc_ignore_primitive_ranges.gltf"
    no_ranges.write_text(json.dumps(original))
    on = capture(no_ranges, "overhead_glass_only", OVERHEAD)
    capture(no_ranges, "overhead_decal_off", OVERHEAD+["--asobo-disable", "ASOBO_material_blend_gbuffer"])
    explicit = copy.deepcopy(original)
    for mat in explicit["materials"]:
        decal = mat.get("extensions", {}).get("ASOBO_material_blend_gbuffer")
        if decal is not None:
            decal.update({key: 1 for key in ("baseColorBlendFactor", "metallicBlendFactor", "roughnessBlendFactor",
                                           "normalBlendFactor", "emissiveBlendFactor", "occlusionBlendFactor")})
    path = OUT / "vc_explicit_decal_defaults.gltf"
    path.write_text(json.dumps(explicit))
    if on.read_bytes() != capture(path, "overhead_explicit_defaults", OVERHEAD).read_bytes():
        raise RuntimeError("explicit defaults must match the empty decal object exactly")
    for variant, label in (("vc_identity_ao.gltf", "overhead_identity_ao"),
                           ("vc_single_options.gltf", "overhead_single_options")):
        capture(OUT / variant, label, OVERHEAD)
    doc = copy.deepcopy(original)
    indices = set()
    for mat in doc["materials"]:
        if "ASOBO_material_blend_gbuffer" in mat.get("extensions", {}):
            info = mat.get("pbrMetallicRoughness", {}).get("baseColorTexture")
            if info:
                indices.add(doc["textures"][info["index"]]["extensions"]["MSFT_texture_dds"]["source"])
    for index in indices:
        path = Path(doc["images"][index]["uri"])
        if not path.exists():
            print("Missing authored image:", path.name, flush=True)
            continue
        out = OUT / f"flipped_decal_{index}.png"
        Image.open(path).transpose(Image.Transpose.FLIP_TOP_BOTTOM).save(out)
        doc["images"][index]["uri"] = str(out)
    model = OUT / "vc_flip_decal_albedo.gltf"
    model.write_text(json.dumps(doc))
    capture(model, "overhead_flip_decal_albedo", OVERHEAD)


if __name__ == "__main__":
    if "--ablations" in sys.argv:
        ablations()
    elif "--pmdg" in sys.argv:
        pmdg()
    else:
        mappings()
