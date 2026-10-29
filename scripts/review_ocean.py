# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.8a review captures: the FFT ocean ground under the E195 exterior (model base, the wheels at the sea level).

Rows: noon with the procedural sky, calm (3 m/s) and rough (20 m/s); moderate (10 m/s) from a low and a high camera;
a waterline close-up (the hull and gear meeting the waves); a horizon view (camera 1.5 degrees above the horizon:
aliasing, tiling); the loop seam (ocean time T - 0.5 s, T = 30 s, and 0.5 s after the seam). Writes
build/review/ocean/<name>.png and contact_sheet.png. One Glider process at a time; nothing is written outside build/.

Run: python scripts/review_ocean.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402

OUT = REPO / "build" / "review" / "ocean"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
NOON = ["--environment", "sky", "--time-of-day-mode", "on", "--time-preset", "noon"]
OCEAN = ["--ground", "ocean", "--hide-grid"]
VIEW = ["--camera-yaw", "35", "--camera-pitch", "12"]


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
    draw.text((margin, 12), "A6.8a FFT ocean: E195 at model base, procedural sky at noon (TAA, Auto exposure), medium quality",
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
    exterior = REAL_ASSETS.get("Exterior")
    if not exterior or not Path(exterior).exists():
        print("E195 exterior not found")
        sys.exit(1)
    model = str(without_ground_equipment(exterior, OUT / "e195_no_gse.gltf"))
    table, failed = [], []

    def row(labels, jobs):
        pngs = []
        for name, args in jobs:
            png, ok = capture(name, model, args)
            pngs.append(png)
            if not ok:
                failed.append(name)
        table.append((labels, pngs))

    row(["Calm, wind 3 m/s", "Rough, wind 20 m/s"],
        [("calm_3", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "3", "--ocean-time", "4"]),
         ("rough_20", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "20", "--ocean-time", "4"])])
    row(["Moderate, wind 10 m/s, from 60 deg", "Moderate, high camera (pitch 40)"],
        [("moderate_10", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "10", "--ocean-wind-dir", "60", "--ocean-time", "4"]),
         ("moderate_10_high", OCEAN + NOON + ["--camera-yaw", "35", "--camera-pitch", "40", "--ocean-wind-speed", "10",
                                                "--ocean-wind-dir", "60", "--ocean-time", "4"])])
    row(["Waterline close-up (nose gear), wind 10 m/s", "Horizon, camera 1.5 deg above it, wind 10 m/s"],
        [("waterline", OCEAN + NOON + ["--camera-yaw", "60", "--camera-pitch", "3", "--camera-distance", "9",
                                       "--camera-target", "0,0.6,11", "--ocean-wind-speed", "10", "--ocean-time", "4"]),
         ("horizon", OCEAN + NOON + ["--camera-yaw", "150", "--camera-pitch", "1.5", "--camera-distance", "60",
                                     "--camera-target", "0,-1,0", "--ocean-wind-speed", "10", "--ocean-time", "4"])])
    row(["Loop seam: ocean time 29.5 s (T = 30 s)", "Loop seam: ocean time 30.5 s (= 0.5 s)"],
        [("seam_29_5", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "10", "--ocean-time", "29.5"]),
         ("seam_30_5", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "10", "--ocean-time", "30.5"])])
    contact_sheet(table)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}")
    if failed:
        print("FAILED: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
