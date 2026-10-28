# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.1 review captures: the E195 exterior on the ground plane at the six time-of-day presets, with sun shadows.

The scene is the default place and date (Seattle-Tacoma, 2026-04-15, UTC-7) with the aircraft heading HEADING
(true degrees the nose points to). The sun follows the clock (time_of_day_mode on, Manual colour and intensity 8);
the sky and the environment lighting are still the fixed default HDR until A6.2/A6.3, so night is the HDR's IBL
without a sun. The E195 is the exterior without its ground support equipment (as review_ground.py), so its nose
wheels touch the plane (Model base). Writes build/review/timeofday/<preset>.png, <preset>.json (the computed
time-of-day state) and contact_sheet.png. One Glider process at a time; nothing is written outside build/.

Run: python scripts/review_time_of_day.py [--heading <deg>]
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402

OUT = REPO / "build" / "review" / "timeofday"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
PRESETS = ["dawn", "morning", "noon", "afternoon", "dusk", "night"]
HEADING = 0.0
CAMERA = ["--camera-yaw", "35", "--camera-pitch", "12"]


def capture(preset, model, heading):
    png, report = OUT / f"{preset}.png", OUT / f"{preset}.json"
    png.unlink(missing_ok=True)
    flags = ["--time-of-day-mode", "on", "--time-preset", preset, "--heading", str(heading), "--ground", "plane",
             "--sun-intensity", "8", "--hide-grid"]
    result = subprocess.run([str(EXE), str(model)] + flags + CAMERA + COMMON +
                            ["--capture-viewport", str(png), "--capture-frame", "64", "--profile-report", str(report),
                             "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{preset}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and "GL automation errors: 0" in log
    state = json.loads(report.read_text(encoding="utf-8"))["timeOfDay"] if report.exists() else None
    print(("OK   " if ok else "FAIL ") + preset, flush=True)
    return (png if png.exists() else None), state, ok


def contact_sheet(rows):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin = 640, 30, 12
    first = next(png for _, png, _ in rows if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    width = margin + 3 * (cell_w + margin)
    height = 50 + 2 * (cell_h + label_h + margin)
    sheet = Image.new("RGB", (width, height), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A6.1 time of day: E195 on the ground plane, Seattle 2026-04-15, sun shadows (default HDR, TAA, exposure 0)",
              font=font, fill="white")
    for i, (preset, png, state) in enumerate(rows):
        left = margin + (i % 3) * (cell_w + margin)
        top = 50 + (i // 3) * (cell_h + label_h + margin)
        label = preset
        if state:
            label = f"{preset} {state['time']}  sun az {state['sun']['azimuth']:.0f} el {state['sun']['elevation']:.1f}"
        draw.text((left, top), label, font=font, fill=(205, 210, 220))
        if png:
            sheet.paste(Image.open(png).convert("RGB").resize((cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--heading", type=float, default=HEADING)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    exterior = REAL_ASSETS.get("Exterior")
    if not exterior or not Path(exterior).exists():
        print("E195 exterior not available")
        return 1
    model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
    rows, failed = [], []
    for preset in PRESETS:
        png, state, ok = capture(preset, model, args.heading)
        rows.append((preset, png, state))
        if not ok:
            failed.append(preset)
    if any(png for _, png, _ in rows):
        contact_sheet(rows)
    print("RESULT: " + ("FAIL " + ", ".join(failed) if failed else "OK"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
