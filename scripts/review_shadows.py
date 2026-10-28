# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.2 review captures: cascaded sun shadows on the E195 exterior, the DA62 SDK airframe and cockpit, a grazing
sun, and the two shadow debug views.

Rows: E195 exterior (exterior_fuselage_nose), DA62 airframe (exterior), DA62 cockpit (the A3.1 view, where the
unshadowed sun leaked in), the A3.2 contact fixture under a 5 degree sun, and the E195 exterior in the Shadow
Cascades and Shadow Visibility debug views. Columns: sun on without shadows (the A3.1 image) and with the default
shadows (2048 x 3 cascades, PCF). Writes build/review/shadows/<row>_<column>.png, review_shadows.json (the shadow
state of each capture) and contact_sheet.png. One Glider process at a time; nothing is written outside build/.

Run: python scripts/review_shadows.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import shadow_fixtures  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402

OUT = REPO / "build" / "review" / "shadows"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "none", "--fixed-frame-time", "0.05", "--vsync", "off"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_ASOBO = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo"
DA62_COCKPIT = DA62_ASOBO / "Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_AIRFRAME = DA62_ASOBO / "Part_Exterior_Airframe/model/Airframe_LOD00.gltf"
DA62_TEXTURES = DA62 / "Sources/Art/Max/textures"
SUN = ["--sun", "on", "--sun-dir", "45,40", "--sun-intensity", "8"]

COLUMNS = [
    ("noshadow", "sun, no shadows (A3.1)", ["--shadows", "off"]),
    ("shadow", "sun + shadows (2048 x 3, PCF)", []),
]


def rows():
    result = []
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        flags = [str(exterior)] + view("exterior_fuselage_nose")["flags"] + ["--hide-grid"] + SUN
        result.append(("e195_exterior", "E195 exterior (exterior_fuselage_nose), az 45 el 40", flags))
    if DA62_AIRFRAME.exists():
        result.append(("da62_exterior", "DA62 SDK airframe, az 45 el 40",
                       [str(DA62_AIRFRAME), "--camera-yaw", "35", "--camera-pitch", "25", "--hide-grid",
                        "--search-path", str(DA62_TEXTURES)] + SUN))
    if DA62_COCKPIT.exists():
        result.append(("da62_cockpit", "DA62 SDK cockpit (the A3.1 view), az 45 el 40",
                       [str(DA62_COCKPIT), "--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05",
                        "--camera-pitch", "-20", "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid",
                        "--search-path", str(DA62_TEXTURES)] + SUN))
    contact = shadow_fixtures.contact(OUT / "contact.gltf")
    result.append(("grazing", "1 m cube on a plane, sun az 90 el 5 (grazing)",
                   [str(contact), "--camera-target", "0,0,0", "--camera-yaw", "50", "--camera-pitch", "30",
                    "--camera-distance", "5", "--hide-grid", "--sun", "on", "--sun-dir", "90,5", "--sun-intensity", "8"]))
    return result


DEBUG_ROWS = [("debug_cascades", "Shadow Cascades", "shadowcascades"), ("debug_visibility", "Shadow Visibility", "shadowvisibility")]


def capture(name, args):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(EXE)] + args + COMMON + ["--capture-viewport", str(png), "--capture-frame", "30",
                                                          "--profile-report", str(report), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and "GL automation errors: 0" in log
    shadows = None
    if report.exists():
        shadows = json.loads(report.read_text(encoding="utf-8")).get("sun", {}).get("shadows")
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return png if png.exists() else None, shadows, ok


def contact_sheet(table):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin = 640, 30, 12
    first = next(png for row in table for png in row[2] if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    width = margin + 2 * (cell_w + margin)
    height = 50 + len(table) * (cell_h + label_h + margin)
    sheet = Image.new("RGB", (width, height), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A3.2 sun shadows: without / with cascaded shadow maps (default HDR, ACES Hill, exposure 0)",
              font=font, fill="white")
    for r, (_, labels, pngs) in enumerate(table):
        top = 50 + r * (cell_h + label_h + margin)
        for c, png in enumerate(pngs):
            left = margin + c * (cell_w + margin)
            draw.text((left, top), labels[c], font=font, fill=(205, 210, 220))
            if png:
                sheet.paste(Image.open(png).convert("RGB").resize((cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    table, summary, failed = [], {}, []
    for key, title, args in rows():
        pngs = []
        for column, label, flags in COLUMNS:
            name = f"{key}_{column}"
            png, shadows, ok = capture(name, args + flags)
            pngs.append(png)
            summary[name] = shadows
            if not ok:
                failed.append(name)
        table.append((key, [f"{title} - {COLUMNS[0][1]}", COLUMNS[1][1]], pngs))
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        flags = [str(exterior)] + view("exterior_fuselage_nose")["flags"] + ["--hide-grid"] + SUN
        pngs, labels = [], []
        for key, label, mode in DEBUG_ROWS:
            png, shadows, ok = capture(f"e195_{key}", flags + ["--debug-view", mode])
            pngs.append(png)
            labels.append(f"E195 exterior - debug view {label}")
            summary[f"e195_{key}"] = shadows
            if not ok:
                failed.append(key)
        table.append(("debug", labels, pngs))
    (OUT / "review_shadows.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    contact_sheet(table)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}; failed: {failed or 'none'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
