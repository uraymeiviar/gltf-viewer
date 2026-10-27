# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A5.2 real-asset before/after review; writes only build/review/uv_options.

Read source glTFs, buffers and textures only; never consult simulator config files.
The optional Dust forward view disables the decal extension identically in both builds,
because the SDK Nose attachment consists of a decal shell without its aircraft receiver.
"""
import json
import re
from pathlib import Path

import harness_process as subprocess
from PIL import Image, ImageChops, ImageDraw, ImageEnhance
from e195_assets import COCKPIT, TEXTURE_FOLDERS

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/uv_options"
BASELINE = REPO / "build/baselines/d0761e7.exe"
EXE = REPO / "build/bin/Release/Glider.exe"
SDK = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
ATTACHMENTS = SDK / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo"
TEXTURES = SDK / "Sources/Art/Max/textures"
PMDG = Path(r"F:\msfs-addons\pmdg-aircraft-739\SimObjects\Airplanes\PMDG 737-900\model.BW\PMDG_739NG3_BW_LOD0.gltf")


def prepare(source, label, search_dirs):
    """Absolute-URI review copy in build; no source geometry/material changes."""
    doc = json.loads(source.read_text(encoding="utf-8"))
    missing = []
    index = {}
    for folder in search_dirs:
        if folder.exists():
            for path in folder.rglob("*"):
                if path.is_file():
                    index.setdefault(path.name.lower(), path)
    for item in [*doc.get("buffers", []), *doc.get("images", [])]:
        uri = item.get("uri", "")
        if not uri or uri.startswith("data:"):
            continue
        local = source.parent / uri
        resolved = local if local.is_file() else index.get(Path(uri.replace("\\", "/")).name.lower())
        if resolved is None:
            missing.append(uri)
            item["uri"] = local.as_posix()
        else:
            item["uri"] = resolved.as_posix()
    modifications = []
    if label == "da62_tyre_static":
        # The source attachment has an already-deformed tyre in both executables.
        # Keep its untouched capture too; a supplementary static rest-mesh copy
        # makes the real tyre texture inspectable without changing source files.
        for node in doc.get("nodes", []):
            if node.get("name") == "GEAR_Tire_Still_Left":
                node.pop("skin", None)
                modifications.append("Removed skin binding on GEAR_Tire_Still_Left for static rest-mesh review")
    output = OUT / (label + "_abs.gltf")
    output.write_text(json.dumps(doc), encoding="utf-8")
    (OUT / (label + "_inputs.json")).write_text(json.dumps({"source": str(source), "missing": missing, "modifications": modifications}, indent=2))
    print(label + ": unresolved textures: " + str(len(missing)), flush=True)
    return output


def render(exe, model, name, flags):
    png = OUT / (name + ".png")
    png.unlink(missing_ok=True)
    args = [str(exe), str(model), "--isolated", "--window-size", "1600x1000", "--aa", "none",
            "--fixed-frame-time", "0.05", "--vsync", "off", "--hide-grid", *flags,
            "--capture-viewport", str(png), "--capture-frame", "16",
            "--profile-report", str(OUT / (name + ".json")), "--exit-after-profile"]
    result = subprocess.run(args, cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / (name + ".log")).write_text(log, encoding="utf-8")
    if result.returncode & 0xffffffff == 0xc0000142:
        raise SystemExit("STOP Windows 0xc0000142; no retries")
    if result.returncode or not png.exists() or "GL automation errors: 0" not in log or re.search(r"GL error (?:0x|during)|GL error \(0x", log):
        raise RuntimeError(name + ": capture/GL failure, see log")
    return png


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cases = [
        ("e195_lcd", COCKPIT, [COCKPIT.parent.parent / folder for folder in TEXTURE_FOLDERS],
         ["--select-node", "LCD_MASK_IESS_SCREEN", "--camera-yaw", "180", "--camera-pitch", "0", "--camera-distance", ".01", "--debug-view", "basecolor"]),
        ("da62_tyre", ATTACHMENTS / "Part_Exterior_Gear_L/model/Gear_LOD00.gltf", [TEXTURES],
         ["--select-node", "GEAR_Tire_Still_Left", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "1.5"]),
        ("da62_tyre_static", ATTACHMENTS / "Part_Exterior_Gear_L/model/Gear_LOD00.gltf", [TEXTURES],
         ["--isolate-node", "GEAR_Tire_Still_Left", "--camera-yaw", "90", "--camera-pitch", "0", "--camera-distance", ".7"]),
        ("da62_dust", ATTACHMENTS / "Asset_Exterior_Nose_Default/model/Nose_LOD00.gltf", [TEXTURES],
         ["--select-node", "Dust_Nose_Default", "--camera-yaw", "-45", "--camera-pitch", "-10", "--camera-distance", ".7",
          "--asobo-disable", "ASOBO_material_geometry_decal"]),
        ("pmdg_fuselage", PMDG, list(PMDG.parent.parent.glob("texture*")),
         ["--isolate-node", "fuselage", "--camera-yaw", "-65", "--camera-pitch", "-5", "--camera-distance", "28"]),
    ]
    results = []
    for name, source, folders, flags in cases:
        if not source.exists():
            raise FileNotFoundError(source)
        model = prepare(source, name, folders)
        before = render(BASELINE, model, name + "_before", flags)
        after = render(EXE, model, name + "_after", flags)
        a, b = Image.open(before).convert("RGB"), Image.open(after).convert("RGB")
        diff = ImageChops.difference(a, b)
        pixels = list(diff.getdata())
        changed = sum(pixel != (0, 0, 0) for pixel in pixels)
        result = {"name": name, "source": str(source), "flags": flags, "before": before.name,
                  "after": after.name, "changedPixels": changed, "pixels": len(pixels),
                  "magentaAfter": sum(r>180 and b>180 and g<90 for r,g,b in b.getdata())}
        results.append(result)
        print(json.dumps(result), flush=True)
    (OUT / "review.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    sheet = Image.new("RGB", (1200, len(results)*400), (24,24,28))
    draw = ImageDraw.Draw(sheet)
    for row, item in enumerate(results):
        for col, phase in enumerate(("before", "after")):
            im = Image.open(OUT / item[phase]).convert("RGB")
            if item["name"] == "e195_lcd":
                im = ImageEnhance.Brightness(im).enhance(8)
            im.thumbnail((594,360))
            sheet.paste(im, (col*600, row*400+30))
            draw.text((col*600+8,row*400+6), item["name"] + " " + phase + (" d0761e7" if phase=="before" else " A5.2") + (" (display gain 8x)" if item["name"] == "e195_lcd" else ""), fill="white")
    sheet.save(OUT / "contact_sheet.png")
    print("Review complete: " + str(OUT / "contact_sheet.png"), flush=True)


if __name__ == "__main__":
    main()
