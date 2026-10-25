# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A4 first part: glass reflection, slab refraction and exact disabled-path checks.

Run with Anaconda Python. Artifacts and measurements go to build/review/glass_*.
Uses synthetic geometry only; aircraft comparisons are recorded in the A4 report.
"""
import base64
import json
import os
import re
import struct
from pathlib import Path

import numpy as np
from PIL import Image
import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review"
EXE = REPO / "build/bin/Release/Glider.exe"
BASELINE = REPO / "build/baselines/ed36dd8.exe"
GLASS = "ASOBO_material_glass_v2"
WINDSHIELD = "ASOBO_material_windshield_v3"
failures = []


def check(ok, label):
    print(("PASS " if ok else "FAIL ") + label, flush=True)
    if not ok:
        failures.append(label)


def fixture(name, extension=GLASS, width=0.0, normal=(0, 0, 1), tint=(1, 1, 1), alignment=False):
    # Checkerboard is opaque and unlit: scene-copy errors cannot hide behind lighting.
    data = bytearray()
    views, accessors, meshes = [], [], []
    materials = [{"pbrMetallicRoughness": {"baseColorFactor": [*tint, 0.02],
                  "metallicFactor": 0, "roughnessFactor": 0.08},
                  "alphaMode": "BLEND", "doubleSided": True}]
    if extension:
        materials[0]["extensions"] = {extension: {"glassWidth": width} if extension == GLASS else {}}
    for level in (0.025, 0.35):
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": [level]*3+[1]},
                          "extensions": {"KHR_materials_unlit": {}}})

    def quad(x0, y0, x1, y1, z, material, n=(0, 0, 1)):
        positions = [(x0,y0,z),(x1,y0,z),(x1,y1,z),(x0,y1,z)]
        first = len(accessors)
        for values in (positions, [n]*4):
            offset = len(data)
            data.extend(b"".join(struct.pack("<3f", *v) for v in values))
            views.append({"buffer": 0, "byteOffset": offset, "byteLength": 48})
            acc = {"bufferView": len(views)-1, "componentType": 5126, "count": 4, "type": "VEC3"}
            if values is positions:
                acc.update(min=[x0,y0,z], max=[x1,y1,z])
            accessors.append(acc)
        # No index buffer: keep buffer alignment trivially valid.
        meshes.append({"primitives": [{"attributes": {"POSITION": first, "NORMAL": first+1},
                                      "mode": 6, "material": material}]})

    quad(-1,-1,1,1,0,0,normal)
    for y in range(-8, 8):
        for x in range(-8, 8):
            quad(x*0.3,y*0.3,(x+1)*0.3,(y+1)*0.3,-0.5,1+(x+y)%2)
    if alignment:
        # Neutral zero-width glass makes C = B exactly: no reflection/tint/coverage
        # differences can disguise a scene-copy lookup or missing-draw error.
        materials[0]["pbrMetallicRoughness"]["baseColorFactor"] = [1,1,1,0]
        materials[0]["extensions"]["KHR_materials_unlit"] = {}
        materials[0]["extensions"]["ASOBO_material_draw_order"] = {"drawOrderOffset": 0}
        decal = "ASOBO_material_geometry_decal"
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": [0.1,0.8,0.1,1],
                          "metallicFactor": 0, "roughnessFactor": 1},
                          "extensions": {decal: {}}, "doubleSided": True})
        # A small E-shaped decal tests the accumulated labels, not just opaque geometry.
        for rect in [(-0.5,-0.4,-0.43,0.4),(-0.5,0.33,0.1,0.4),
                     (-0.5,-0.035,0.0,0.035),(-0.5,-0.4,0.1,-0.33)]:
            quad(*rect,-0.4999,3)
        # Ordinary transparent draws must also appear in the copy.
        materials.append({"pbrMetallicRoughness": {"baseColorFactor": [0.1,0.2,0.7,1]},
                          "alphaMode": "BLEND", "extensions": {"KHR_materials_unlit": {}},
                          "doubleSided": True})
        quad(0.3,-0.4,0.55,0.4,-0.25,4)
    doc = {"asset": {"version": "2.0"}, "scene": 0,
           "scenes": [{"nodes": list(range(len(meshes)))}],
           "nodes": [{"mesh": i} for i in range(len(meshes))], "meshes": meshes,
           "materials": materials, "accessors": accessors, "bufferViews": views,
           "buffers": [{"byteLength": len(data), "uri": "data:application/octet-stream;base64,"+
                        base64.b64encode(data).decode()}],
           "extensionsUsed": ["KHR_materials_unlit"] + ([extension] if extension else [])}
    if alignment:
        doc["extensionsUsed"] += ["ASOBO_material_geometry_decal", "ASOBO_material_draw_order"]
    path = OUT / ("glass_"+name+".gltf")
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def render(model, name, flags=(), exe=EXE, env=None):
    png = OUT / ("glass_"+name+".png")
    png.unlink(missing_ok=True)
    args = [str(exe), str(model), "--isolated", "--window-size", "1000x800",
            "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none",
            "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
            "--camera-distance", "4", "--exposure", "-3", "--capture-viewport", str(png), "--capture-frame", "24",
            "--profile-report", str(png.with_suffix(".json")), "--exit-after-profile", *flags]
    result = subprocess.run(args, cwd=REPO, capture_output=True, timeout=300, env=env)
    (OUT / ("glass_"+name+".log")).write_bytes(result.stdout+result.stderr)
    if result.returncode or not png.exists():
        raise RuntimeError(f"{name}: exit {result.returncode}; see log")
    if exe == EXE:
        log = (result.stdout+result.stderr).decode("utf-8",errors="replace")
        if "GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x",log):
            raise RuntimeError(f"{name}: GL diagnostics failed; see log")
        if not (env and env.get("GLIDER_TEST_GLASS_COPY_FAILURE")) and "Glass copy fallback:" in log:
            raise RuntimeError(f"{name}: unexpected glass copy fallback; see log")
    return png


def pixels(path):
    a = np.asarray(Image.open(path).convert("RGB"), dtype=float)
    h,w = a.shape[:2]
    return a[h//2-60:h//2+60,w//2-60:w//2+60]


def same(a,b):
    return a.read_bytes() == b.read_bytes()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    metrics = {}
    # At zero thickness, all features behind neutral glass must retain their pixels.
    aligned = fixture("alignment", alignment=True)
    for name, flags in (("none",[]),("msaa2",["--aa","msaa2"]),
                        ("msaa4",["--aa","msaa4"]),("msaa8",["--aa","msaa8"]),
                        ("scale_half",["--render-scale","0.5"])):
        on = pixels(render(aligned,"alignment_"+name+"_on",flags))
        off = pixels(render(aligned,"alignment_"+name+"_off",flags+["--asobo-disable",GLASS]))
        delta = np.abs(on-off)
        costs = {(dx,dy):float(np.abs(on[4:-4,4:-4] -
                 off[4+dy:116+dy,4+dx:116+dx]).mean())
                 for dy in range(-3,4) for dx in range(-3,4)}
        shift = min(costs,key=costs.get)
        metrics["alignment_"+name] = {"mean_abs_delta":float(delta.mean()),
            "max_delta":float(delta.max()),"changed_pixels":int(np.any(delta>0,axis=2).sum()),
            "best_shift":list(shift)}
        check(shift == (0,0) and delta.mean() < (1.5 if name.startswith("msaa") else 0.01),
              f"zero-width {name}: shift {shift}, MAE {delta.mean():.6f}, max {delta.max():.0f}/255")
    # Raw RGB alone hid the black-viewport bug: ImGui blends the captured alpha again.
    # A complete background replacement must be opaque, also on the very first render.
    lit = fixture("msaa_lit",normal=(0.7071,0,0.7071))
    reference = None
    for aa in ("none","msaa2","msaa4","msaa8"):
        first = OUT / ("glass_msaa_"+aa+"_first.png")
        path = render(lit,"msaa_"+aa,["--aa",aa,"--capture-first-frame",str(first)])
        image = np.asarray(Image.open(path).convert("RGBA"))
        h,w = image.shape[:2]
        center = image[h//2-60:h//2+60,w//2-60:w//2+60]
        initial = np.asarray(Image.open(first).convert("RGBA"))
        ih,iw = initial.shape[:2]
        initial = initial[ih//2-40:ih//2+40,iw//2-40:iw//2+40]
        check(center[:,:,3].min()==255 and initial[:,:,3].min()==255,
              aa+": complete glass colour is opaque in first and steady viewport images")
        if reference is None: reference=center[:,:,:3].astype(float)
        delta=float(np.abs(center[:,:,:3].astype(float)-reference).mean())
        metrics["lit_"+aa+"_mean_delta_from_none"]=delta
        check(delta<2, f"{aa}: lit glass interior MAE {delta:.6f}/255 against none")
    for fault in ("allocation","multisample","resolve","capture"):
        env=dict(os.environ,GLIDER_TEST_GLASS_COPY_FAILURE=fault)
        path=render(lit,"fallback_"+fault,["--aa","msaa8"],env=env)
        rgba=np.asarray(Image.open(path).convert("RGBA"))
        h,w=rgba.shape[:2]
        center=rgba[h//2-60:h//2+60,w//2-60:w//2+60]
        check(pixels(path).mean()>10 and center[:,:,3].min()==255,
              fault+": injected failure stays visible with opaque viewport alpha")
        log=(OUT/("glass_fallback_"+fault+".log")).read_text(encoding="utf-8",errors="replace")
        check(log.count("Glass copy fallback:")==1,fault+": fallback logs once")
    # Tilt the shading normal, retaining identical raster coverage/checkerboard pixels.
    # This isolates the Fresnel change from camera projection and visible sky changes.
    for extension, prefix in ((GLASS,"v2"),(WINDSHIELD,"v3")):
        for angle, normal in (("head",(0,0,1)),("grazing",(0.985,0,0.174))):
            model = fixture(prefix+"_"+angle, extension, normal=normal)
            on = render(model,prefix+"_"+angle+"_on")
            off = render(model,prefix+"_"+angle+"_off",["--asobo-disable",extension])
            baseline = render(model,prefix+"_"+angle+"_baseline",["--asobo-disable",extension],BASELINE)
            check(same(off,baseline),prefix+" "+angle+" switch off is byte-identical to ed36dd8")
            delta = float(np.mean(pixels(on)-pixels(off)))
            metrics[prefix+"_"+angle+"_brightness_delta"] = delta
        head = metrics[prefix+"_head_brightness_delta"]
        grazing = metrics[prefix+"_grazing_brightness_delta"]
        check(grazing > 2, f"{prefix} grazing reflection visible: mean delta {grazing:.3f}/255")
        check(0 <= head < 8 and head < grazing*0.3, f"{prefix} head-on reflection weaker: {head:.3f} vs {grazing:.3f}/255")
    thin = fixture("width_zero",normal=(0.7071,0,0.7071))
    thick = fixture("width_large",width=3.0,normal=(0.7071,0,0.7071))
    a,b = render(thin,"width_zero"),render(thick,"width_large")
    diff = np.abs(pixels(a)-pixels(b))
    metrics["width_mean_abs_delta"] = float(diff.mean())
    check(diff.mean()>5, f"width shifts checkerboard: mean absolute delta {diff.mean():.3f}/255")
    check(same(render(thin,"width_zero_off",["--asobo-disable",GLASS]),
               render(thick,"width_large_off",["--asobo-disable",GLASS])), "width has no effect with switch off")
    tinted = render(fixture("tinted",normal=(0.7071,0,0.7071),tint=(0.3,1,0.3)),"tinted")
    check(pixels(tinted)[:,:,0].mean() < pixels(a)[:,:,0].mean()-2,
          "base colour tints the copied background")
    # Exercise the pre-existing KHR transmission copy path with no ASOBO declaration.
    khr = fixture("khr",None)
    doc = json.loads(khr.read_text())
    doc["materials"][0]["extensions"] = {"KHR_materials_transmission": {"transmissionFactor":0.8}}
    doc["extensionsUsed"].append("KHR_materials_transmission")
    khr.write_text(json.dumps(doc),encoding="utf-8")
    for label, model, flags in (("khr",khr,[]),
                                ("plain",fixture("plain",None),[]),
                                ("asobo_off",thick,["--asobo","off"])):
        check(same(render(model,label,flags),render(model,label+"_baseline",flags,BASELINE)),
              label+" is byte-identical to ed36dd8")
    (OUT/"glass_metrics.json").write_text(json.dumps(metrics,indent=2),encoding="utf-8")
    print(json.dumps(metrics,indent=2))
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
