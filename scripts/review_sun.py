# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.1 review captures: the sun on HelloCube, the E195 exterior and the DA62 SDK cockpit.

Columns: sun off (the default), From HDR (the default HDR's sun, which is weak: it sits behind clouds near the
horizon), and two manual directions. Writes build/review/sun/<row>_<column>.png, review_sun.json (the effective
sun of each capture) and contact_sheet.png. Shadows do not exist yet (A3.2): in the cockpit the sun lights
surfaces the airframe would shade. One Glider process at a time; nothing is written outside build/.

Run: python scripts/review_sun.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402

OUT = REPO / "build" / "review" / "sun"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "none", "--fixed-frame-time", "0.05", "--vsync", "off"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_COCKPIT = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_TEXTURES = DA62 / "Sources/Art/Max/textures"

COLUMNS = [
    ("off", "sun off", []),
    ("hdr", "From HDR", ["--sun", "on", "--sun-mode", "hdr"]),
    ("manual_a", "manual az 45 el 40, I 8", ["--sun", "on", "--sun-dir", "45,40", "--sun-intensity", "8"]),
    ("manual_b", "manual az 240 el 25, I 8, 4500 K", ["--sun", "on", "--sun-dir", "240,25", "--sun-intensity", "8",
                                                       "--sun-color", "1.3003,0.9457,0.6534"]),
]


def rows():
    result = [("hellocube", "HelloCube (default scene)", [])]
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        result.append(("e195_exterior", "E195 exterior (exterior_fuselage_nose)",
                       [str(exterior)] + view("exterior_fuselage_nose")["flags"] + ["--hide-grid"]))
    if DA62_COCKPIT.exists():
        result.append(("da62_cockpit", "DA62 SDK cockpit (no shadows yet: light leaks in, A3.2)",
                       [str(DA62_COCKPIT), "--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05",
                        "--camera-pitch", "-20", "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid",
                        "--search-path", str(DA62_TEXTURES)]))
    return result


def capture(name, args):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(EXE)] + args + COMMON + ["--capture-viewport", str(png), "--capture-frame", "30",
                                                          "--profile-report", str(report), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and "GL automation errors: 0" in log
    sun = json.loads(report.read_text(encoding="utf-8")).get("sun") if report.exists() else None
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return png if png.exists() else None, sun, ok


def contact_sheet(table):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    cell_w, cell_h, label_h, margin = 560, 0, 30, 12
    first = next(png for row in table for png in row[2] if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    width = margin + len(COLUMNS) * (cell_w + margin)
    height = 50 + len(table) * (cell_h + label_h + margin)
    sheet = Image.new("RGB", (width, height), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A3.1 sun review: off / From HDR / two manual directions (default HDR, ACES Hill, exposure 0)",
              font=font, fill="white")
    for r, (_, title, pngs) in enumerate(table):
        top = 50 + r * (cell_h + label_h + margin)
        for c, png in enumerate(pngs):
            left = margin + c * (cell_w + margin)
            draw.text((left, top), f"{title} - {COLUMNS[c][1]}" if c == 0 else COLUMNS[c][1], font=font, fill=(205, 210, 220))
            if png:
                sheet.paste(Image.open(png).convert("RGB").resize((cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    table, summary, failed = [], {}, []
    for key, title, args in rows():
        pngs = []
        for column, _, flags in COLUMNS:
            name = f"{key}_{column}"
            png, sun, ok = capture(name, args + flags)
            pngs.append(png)
            summary[name] = sun
            if not ok:
                failed.append(name)
        table.append((key, title, pngs))
    (OUT / "review_sun.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    contact_sheet(table)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}; failed: {failed or 'none'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
