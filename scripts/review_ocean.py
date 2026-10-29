# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.8 review captures: the FFT ocean ground under the E195 exterior (model base, the wheels at the sea level).

Rows of build/review/ocean/contact_sheet.png (procedural sky with the A6.1 clock unless stated, TAA, Auto exposure):
  noon: calm (3 m/s), moderate (10 m/s from 60 degrees), moderate from a high camera, rough (20 m/s);
  sunset (the computed sunset time) at 10 and 20 m/s; a full-moon night (2026-05-01 23:30) at 10 m/s, default exposure and
  Auto exposure; the HDR environment at 3 and 20 m/s; a waterline close-up (the nose gear in the waves, wind 10 m/s); a
  horizon view (camera 1.5 degrees above it); the loop seam (ocean time 29.5 s and 30.5 s, T = 30 s); live mode (512^2
  at high quality); and the Ocean settings UI (a window capture of the Ground section in the inspector).
Also seam_strip.png (ocean times 29, 29.5, 30, 30.5, 31 s, a crop of the near sea, with the frame-to-frame differences
printed) and swim_strip.png (five camera positions 0.2 m apart at a fixed ocean time, a crop of the near horizon band, with
the frame-to-frame differences printed: a surface whose vertices slid over the waves would show uneven steps).
Writes build/review/ocean/<name>.png and contact_sheet.png. One Glider process at a time; nothing is written outside build/.

Run: python scripts/review_ocean.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402
import validate_night as night  # noqa: E402

OUT = REPO / "build" / "review" / "ocean"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
NOON = ["--environment", "sky", "--time-of-day-mode", "on", "--time-preset", "noon"]
FULL_MOON = ["--environment", "sky", "--time-of-day-mode", "on", "--date", "2026-05-01", "--time", "23:30"]
HDR_SUN = ["--sun", "on", "--sun-mode", "hdr"]
OCEAN = ["--ground", "ocean", "--hide-grid"]
VIEW = ["--camera-yaw", "35", "--camera-pitch", "12"]
HIGH = ["--camera-yaw", "35", "--camera-pitch", "40"]
WATERLINE = ["--camera-yaw", "60", "--camera-pitch", "3", "--camera-distance", "9", "--camera-target", "0,0.6,11"]
HORIZON = ["--camera-yaw", "150", "--camera-pitch", "1.5", "--camera-distance", "60", "--camera-target", "0,-1,0"]


def capture(name, model, args, aa=None, size=None, frame=64):
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    common = list(COMMON)
    if aa:
        common[common.index("--aa") + 1] = aa
    if size:
        common[common.index("--window-size") + 1] = size
    result = subprocess.run([str(EXE)] + ([model] if model else []) + args + common +
                            ["--capture-viewport", str(png), "--capture-frame", str(frame),
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
    draw.text((margin, 12), "A6.8 FFT ocean: E195 at model base, procedural sky (Seattle, A6.1 clock), TAA, medium quality, "
                            "looping unless stated", font=font, fill="white")
    for r, (labels, pngs) in enumerate(table):
        top = 50 + r * (cell_h + label_h + margin)
        for c, png in enumerate(pngs):
            left = margin + c * (cell_w + margin)
            draw.text((left, top), labels[c], font=font, fill=(205, 210, 220))
            if png:
                image = Image.open(png).convert("RGB")
                sheet.paste(image.resize((cell_w, int(cell_w * image.size[1] / image.size[0]))), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def strip(name, pngs, labels, crop, scale=1):
    """A horizontal strip of equal crops with the mean absolute difference between neighbours (levels, 0..255)."""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 18)
    except OSError:
        font = ImageFont.load_default()
    first = Image.open(pngs[0])
    # crop: fractions (x0, y0, x1, y1) of the capture (the viewport, not the window)
    box = (int(crop[0] * first.size[0]), int(crop[1] * first.size[1]), int(crop[2] * first.size[0]), int(crop[3] * first.size[1]))
    crops = [Image.open(p).convert("RGB").crop(box) for p in pngs]
    w, h = crops[0].size
    w, h = w * scale, h * scale
    sheet = Image.new("RGB", (len(crops) * (w + 6) + 6, h + 34), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    for i, (image, label) in enumerate(zip(crops, labels)):
        sheet.paste(image.resize((w, h), Image.NEAREST if scale > 1 else Image.BILINEAR), (6 + i * (w + 6), 30))
        draw.text((6 + i * (w + 6), 4), label, font=font, fill=(205, 210, 220))
    sheet.save(OUT / f"{name}.png")
    arrays = [np.asarray(c).astype(float) for c in crops]
    steps = [float(np.abs(a - b).mean()) for a, b in zip(arrays, arrays[1:])]
    print(f"{name}: mean |difference| between neighbouring frames {[round(s, 2) for s in steps]} levels", flush=True)
    return steps


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    exterior = REAL_ASSETS.get("Exterior")
    if not exterior or not Path(exterior).exists():
        print("E195 exterior not found")
        sys.exit(1)
    model = str(without_ground_equipment(exterior, OUT / "e195_no_gse.gltf"))
    night.OUT = OUT
    sunset, _ = night.sunset_times()
    sunset_time = ["--time", night.hhmm(sunset)]
    table, failed = [], []

    def row(labels, jobs):
        pngs = []
        for name, args in jobs:
            png, ok = capture(name, model, args)
            pngs.append(png)
            if not ok:
                failed.append(name)
        table.append((labels, pngs))

    sea = lambda wind, *extra: ["--ocean-wind-speed", str(wind), "--ocean-time", "4"] + list(extra)  # noqa: E731
    row(["Noon, calm, wind 3 m/s", "Noon, moderate, wind 10 m/s from 60 deg"],
        [("noon_calm_3", OCEAN + NOON + VIEW + sea(3)), ("noon_moderate_10", OCEAN + NOON + VIEW + sea(10, "--ocean-wind-dir", "60"))])
    row(["Noon, rough, wind 20 m/s", "Noon, moderate, high camera (pitch 40)"],
        [("noon_rough_20", OCEAN + NOON + VIEW + sea(20)), ("noon_moderate_10_high", OCEAN + NOON + HIGH + sea(10, "--ocean-wind-dir", "60"))])
    row(["Sunset, wind 10 m/s", "Sunset, rough, wind 20 m/s"],
        [("sunset_moderate_10", OCEAN + ["--environment", "sky", "--time-of-day-mode", "on"] + sunset_time + VIEW + sea(10)),
         ("sunset_rough_20", OCEAN + ["--environment", "sky", "--time-of-day-mode", "on"] + sunset_time + VIEW + sea(20))])
    row(["Full-moon night, wind 10 m/s (default exposure)", "Full-moon night, wind 10 m/s, Auto exposure"],
        [("moon_moderate_10", OCEAN + FULL_MOON + VIEW + sea(10)),
         ("moon_moderate_10_auto", OCEAN + FULL_MOON + ["--exposure-mode", "auto"] + VIEW + sea(10))])
    row(["HDR environment (default panorama), wind 3 m/s", "HDR environment, rough, wind 20 m/s"],
        [("hdr_calm_3", OCEAN + HDR_SUN + VIEW + sea(3)), ("hdr_rough_20", OCEAN + HDR_SUN + VIEW + sea(20))])
    row(["Waterline close-up (nose gear), wind 10 m/s", "Horizon, camera 1.5 deg above it, wind 10 m/s"],
        [("waterline", OCEAN + NOON + WATERLINE + sea(10)), ("horizon", OCEAN + NOON + HORIZON + sea(10))])
    row(["Loop seam: ocean time 29.5 s (T = 30 s)", "Loop seam: ocean time 30.5 s (= 0.5 s)"],
        [("seam_29_5", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "10", "--ocean-time", "29.5"]),
         ("seam_30_5", OCEAN + NOON + VIEW + ["--ocean-wind-speed", "10", "--ocean-time", "30.5"])])
    row(["Live mode, high quality (4 x 512^2), wind 12 m/s", "Looping, high quality (4 x 256^2), wind 12 m/s"],
        [("live_high_12", OCEAN + NOON + VIEW + sea(12, "--ocean-mode", "live", "--ocean-quality", "high")),
         ("loop_high_12", OCEAN + NOON + VIEW + sea(12, "--ocean-quality", "high"))])

    # The settings UI: a window capture with the Ground section (Ocean) open in the inspector.
    ui_png = OUT / "settings_ui.png"
    # --select-viewport 1 shows the viewport's settings in the inspector (the E195's hierarchy is too long to click the row).
    commands = ["wait 60", "clickp vp.tab.camera 0", "wait 8",
                "set_viewport_setting ground ocean", "set_viewport_setting ocean_wind_speed 12", "wait 40",
                f"shot {ui_png.as_posix()}", ""]
    report = OUT / "settings_ui.json"
    report.unlink(missing_ok=True)
    script = OUT / "settings_ui.txt"
    script.write_text("\n".join(commands), encoding="utf-8")
    result = subprocess.run([str(EXE), model] + NOON + ["--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05",
                                                       "--aa", "taa", "--vsync", "off", "--ui-script", str(script),
                                                       "--ui-report", str(report), "--select-viewport", "1"] + VIEW,
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    (OUT / "settings_ui.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(("OK   " if ui_png.exists() else "FAIL ") + "settings_ui", flush=True)
    if not ui_png.exists():
        failed.append("settings_ui")
    table.append((["Ocean settings UI (Viewport Settings, Camera & Helpers, Ground)", ""], [ui_png if ui_png.exists() else None, None]))
    contact_sheet(table)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}")

    # Strip across the loop seam: crops of the near sea at 29, 29.5, 30, 30.5, 31 s.
    seam_pngs, seam_labels = [], []
    for t in (29.0, 29.5, 30.0, 30.5, 31.0):
        png, ok = capture(f"seamstrip_{t:g}".replace(".", "_"), model, OCEAN + NOON + VIEW + ["--ocean-wind-speed", "10", "--ocean-time", f"{t}"],
                          aa="none", frame=40)
        if not ok:
            failed.append(f"seamstrip {t}")
        seam_pngs.append(png)
        seam_labels.append(f"t = {t:g} s")
    if all(seam_pngs):
        steps = strip("seam_strip", seam_pngs, seam_labels, (0.0, 0.62, 0.6, 1.0))
        (OUT / "seam_strip.json").write_text(json.dumps({"steps": steps, "times": [29.0, 29.5, 30.0, 30.5, 31.0]}), encoding="utf-8")
    # Five camera positions 0.2 m apart at a fixed ocean time (the near horizon band, 2x): vertex swimming would show as uneven
    # steps; a steady camera move gives steps of about the same size.
    swim_pngs = []
    for i in range(5):
        x = 0.2 * i
        png, ok = capture(f"swim_{i}", None, OCEAN + NOON + ["--camera-yaw", "0", "--camera-pitch", "6", "--camera-distance", "25",
                                                             "--camera-target", f"{x},0.0,0", "--ocean-wind-speed", "10",
                                                             "--ocean-time", "7"], aa="none", frame=40)
        if not ok:
            failed.append(f"swim {i}")
        swim_pngs.append(png)
    if all(swim_pngs):
        strip("swim_strip", swim_pngs, [f"camera x + {0.2 * i:.1f} m" for i in range(5)], (0.0, 0.25, 1.0, 0.65))
    if failed:
        print("FAILED: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
