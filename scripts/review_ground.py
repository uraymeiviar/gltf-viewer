# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.6 review captures: the ground plane under the E195 exterior and the DA62 SDK airframe.

Rows: E195 and DA62 on concrete (with slab joints) and on grass, each with the sun in two directions (the
aircraft's shadow on the ground); a low camera towards the horizon (fog into the sky, distance fade of the
detail); the grid drawn on top of the plane. The E195 is the exterior without its stairs and barrier (ground support
equipment that stands 1 m below the nose gear in the file), so its nose wheels touch the plane in Model base mode.
Writes build/review/ground/<name>.png and contact_sheet.png. One Glider process at a time; nothing is written
outside build/.

Run: python scripts/review_ground.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402

OUT = REPO / "build" / "review" / "ground"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_AIRFRAME = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Exterior_Airframe/model/Airframe_LOD00.gltf"
DA62_TEXTURES = DA62 / "Sources/Art/Max/textures"
SUN_1 = ["--sun", "on", "--sun-dir", "45,40", "--sun-intensity", "8"]
SUN_2 = ["--sun", "on", "--sun-dir", "250,22", "--sun-intensity", "8"]


def models():
    result = {}
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        result["e195"] = (str(without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")), [], ["--camera-yaw", "35", "--camera-pitch", "12"])
    if DA62_AIRFRAME.exists():
        result["da62"] = (str(DA62_AIRFRAME), ["--search-path", str(DA62_TEXTURES)], ["--camera-yaw", "35", "--camera-pitch", "18"])
    return result


def capture(name, model, args):
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), model] + args + COMMON + ["--capture-viewport", str(png), "--capture-frame", "64",
                                                               "--profile-report", str(OUT / f"{name}.json"), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and "GL automation errors: 0" in log
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return png if png.exists() else None, ok


def contact_sheet(table):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin = 640, 30, 12
    first = next(png for row in table for png in row[1] if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    width = margin + 2 * (cell_w + margin)
    height = 50 + len(table) * (cell_h + label_h + margin)
    sheet = Image.new("RGB", (width, height), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A3.6 ground plane: model base, IBL + sun with shadows, horizon fog (default HDR, TAA, exposure 0)",
              font=font, fill="white")
    for r, (labels, pngs) in enumerate(table):
        top = 50 + r * (cell_h + label_h + margin)
        for c, png in enumerate(pngs):
            left = margin + c * (cell_w + margin)
            draw.text((left, top), labels[c], font=font, fill=(205, 210, 220))
            if png:
                sheet.paste(Image.open(png).convert("RGB").resize((cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    available = models()
    table, failed = [], []
    def row(labels, jobs, grid=False):
        plane = ["--ground", "plane"] + ([] if grid else ["--hide-grid"])
        pngs = []
        for name, key, extra in jobs:
            model, model_flags, view = available[key]
            png, ok = capture(name, model, model_flags + view + plane + extra)
            pngs.append(png)
            if not ok:
                failed.append(name)
        table.append((labels, pngs))

    for key, title in (("e195", "E195"), ("da62", "DA62 SDK")):
        if key not in available:
            continue
        for preset, label in (("concrete", "concrete"), ("grass", "grass")):
            row([f"{title}, {label}, sun az 45 el 40", f"{title}, {label}, sun az 250 el 22"],
                [(f"{key}_{preset}_sun1", key, ["--ground-preset", preset] + SUN_1),
                 (f"{key}_{preset}_sun2", key, ["--ground-preset", preset] + SUN_2)])
    low = {"e195": ["--camera-pitch", "1.5", "--camera-distance", "60", "--camera-target", "0,-1,0"],
           "da62": ["--camera-pitch", "1.5", "--camera-distance", "22", "--camera-target", "0,-0.2,0"]}
    jobs = []
    for key, preset in (("e195", "concrete"), ("da62", "grass")):
        if key in available:
            jobs.append((f"{key}_low_horizon", key, ["--ground-preset", preset] + SUN_1 + low[key]))
    if len(jobs) == 2:
        row(["E195, concrete, camera 1.5 deg above the horizon", "DA62 SDK, grass, camera 1.5 deg above the horizon"], jobs)
    jobs = []
    for key in ("e195", "da62"):
        if key in available:
            jobs.append((f"{key}_grid_on_ground", key, ["--ground-preset", "concrete", "--ground-grid", "on", "--camera-pitch", "35"] + SUN_1))
    if len(jobs) == 2:
        row(["E195, grid on the ground (plane height)", "DA62 SDK, grid on the ground (plane height)"], jobs, grid=True)
    contact_sheet(table)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}")
    if failed:
        print("FAILED: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
