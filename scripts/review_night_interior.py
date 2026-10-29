# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.4b review captures: Auto with the highlight meter as the procedural sky's default exposure.

Seattle-Tacoma, UTC-7, heading 0, procedural sky with the A6.1 clock, Automatic emissive day/night, TAA, 1600 x 900:
  * E195 cockpit main displays at night: the baseline's Auto (A6.4, average meter; --exposure-mode auto) against the
    current default (Auto, highlight meter);
  * DA62 SDK cockpit at night (if installed, read only): baseline Auto against the current default, and the current
    build with --exposure-metering average;
  * E195 cockpit at noon (windshield view), current default;
  * E195 exterior on the A3.6 ground plane (without its ground support equipment): noon, sunset, night, current default.
Writes build/review/night_interior/<name>.png / .json and contact_sheet.png. One Glider process at a time; nothing is
written outside build/.

Run: python scripts/review_night_interior.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402
from validate_asobo_common import E195_SEARCH_PATHS  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402
import validate_night  # noqa: E402

OUT = REPO / "build" / "review" / "night_interior"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid"]
NIGHT = ["--time-preset", "night", "--time-of-day", "auto"]
NOON = ["--time-preset", "noon", "--time-of-day", "auto"]
E195_CAMERA = ["--camera-yaw", "35", "--camera-pitch", "12"]


def capture(name, model, flags, exe=EXE, frame=64, extra_paths=()):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    paths = [arg for path in extra_paths for arg in ("--search-path", path)]
    args = [str(exe), str(model)] + COMMON + SKY + paths + list(flags)
    result = subprocess.run(args + ["--capture-viewport", str(png), "--capture-frame", str(frame), "--profile-report",
                                    str(report), "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and (exe != EXE or "GL automation errors: 0" in log)
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return (png if png.exists() else None), data, ok


def label_of(name, data):
    if not data:
        return name
    vp = (data.get("night", {}).get("exposure", {}).get("viewports") or [{}])[0]
    text = f"{name}  EV {vp.get('exposureEv', 0):.1f}"
    meter = vp.get("meter")
    if meter:
        text += f" ({meter.get('limitedBy')})"
    return text


def contact_sheet(rows):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 17)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin, columns = 560, 28, 12, 3
    first = next(png for _, png, _ in rows if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    lines = math.ceil(len(rows) / columns)
    sheet = Image.new("RGB", (margin + columns * (cell_w + margin), 50 + lines * (cell_h + label_h + margin)), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A6.4b: Auto + highlight metering (procedural sky default) vs the A6.4 Auto; Seattle, "
                            "Automatic emissive day/night", font=font, fill="white")
    for i, (name, png, data) in enumerate(rows):
        left = margin + (i % columns) * (cell_w + margin)
        top = 50 + (i // columns) * (cell_h + label_h + margin)
        draw.text((left, top), label_of(name, data), font=font, fill=(205, 210, 220))
        if png:
            image = Image.open(png).convert("RGB")
            image = image.resize((cell_w, int(cell_w * image.size[1] / image.size[0])))
            sheet.paste(image.crop((0, 0, cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", type=Path, default=REPO / "build" / "baselines" / "d853c78.exe")
    args = parser.parse_args()
    baseline = args.baseline_exe if args.baseline_exe.is_absolute() else REPO / args.baseline_exe
    OUT.mkdir(parents=True, exist_ok=True)
    validate_night.OUT.mkdir(parents=True, exist_ok=True)
    rows, failed = [], []

    def add(name, png, data, ok):
        rows.append((name, png, data))
        if not ok:
            failed.append(name)

    cockpit = REAL_ASSETS.get("Cockpit")
    if cockpit and Path(cockpit).exists():
        flags = list(view("cockpit_main_displays")["flags"]) + NIGHT
        if baseline.exists():
            add("e195_cockpit_night_old_auto", *capture("e195_cockpit_night_old_auto", cockpit, flags + ["--exposure-mode", "auto"],
                                                        exe=baseline, extra_paths=E195_SEARCH_PATHS))
        add("e195_cockpit_night_new", *capture("e195_cockpit_night_new", cockpit, flags, extra_paths=E195_SEARCH_PATHS))
        add("e195_cockpit_noon_new", *capture("e195_cockpit_noon_new", cockpit,
                                              list(view("cockpit_windshield_inside")["flags"]) + NOON,
                                              extra_paths=E195_SEARCH_PATHS))
    else:
        print("E195 cockpit not available")
    if validate_night.DA62_COCKPIT.exists():
        flags = NIGHT + validate_night.DA62_CAMERA
        if baseline.exists():
            add("da62_night_old_auto", *capture("da62_night_old_auto", validate_night.DA62_COCKPIT,
                                                flags + ["--exposure-mode", "auto"], exe=baseline))
        add("da62_night_new", *capture("da62_night_new", validate_night.DA62_COCKPIT, flags))
        add("da62_night_new_average", *capture("da62_night_new_average", validate_night.DA62_COCKPIT,
                                               flags + ["--exposure-metering", "average"]))
    else:
        print("DA62 SDK cockpit not available")
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
        sunset_h, _ = validate_night.sunset_times()
        for name, when in (("noon", ["--time-preset", "noon"]), ("sunset", ["--time", validate_night.hhmm(sunset_h)]),
                           ("night", ["--time-preset", "night"])):
            add(f"e195_exterior_{name}", *capture(f"e195_exterior_{name}", model, when + ["--ground", "plane"] + E195_CAMERA))
    else:
        print("E195 exterior not available")
    if any(png for _, png, _ in rows):
        contact_sheet(rows)
    print("RESULT: " + ("FAIL " + ", ".join(failed) if failed else "OK"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
