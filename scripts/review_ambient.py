# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.8 review captures: the viewer ambient light (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.8).

  * DA62 SDK cockpit at night (if installed, read only): procedural sky with the A6.1 clock, Auto exposure (the
    sky's default), Automatic emissive day/night; ambient off, 1, 3 and 10 cd/m^2;
  * E195 cockpit main displays at night (same environment): ambient off and 3 cd/m^2;
  * E195 exterior with the default HDR at default settings: ambient off and 3 cd/m^2 (expected: barely visible).
Writes build/review/ambient/<name>.png / .json / .log and contact_sheet.png. One Glider process at a time; nothing is
written outside build/.

Run: python scripts/review_ambient.py
"""

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402
from validate_asobo_common import E195_SEARCH_PATHS  # noqa: E402
import validate_night  # noqa: E402

OUT = REPO / "build" / "review" / "ambient"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
SKY_NIGHT = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid", "--time-preset", "night",
             "--time-of-day", "auto"]


def ambient(cd):
    return [] if cd is None else ["--ambient", "on", "--ambient-intensity", str(cd)]


def capture(name, model, flags, frame=64, extra_paths=()):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    paths = [arg for path in extra_paths for arg in ("--search-path", path)]
    args = [str(EXE), str(model)] + COMMON + paths + list(flags)
    result = subprocess.run(args + ["--capture-viewport", str(png), "--capture-frame", str(frame), "--profile-report",
                                    str(report), "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and "GL automation errors: 0" in log
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return (png if png.exists() else None), data, ok


def label_of(name, data):
    if not data:
        return name
    text = name
    vp = ((data.get("night") or {}).get("exposure", {}).get("viewports") or [None])[0]
    if vp:
        text += f"  EV {vp.get('exposureEv', 0):.1f}"
    return text


def mean_luma(png):
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(png).convert("RGB"), dtype=np.float64) / 255.0
    return float((a @ np.array([0.2126, 0.7152, 0.0722])).mean())


def contact_sheet(rows):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 17)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin, columns = 560, 28, 12, 4
    first = next(png for _, png, _ in rows if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    lines = math.ceil(len(rows) / columns)
    sheet = Image.new("RGB", (margin + columns * (cell_w + margin), 50 + lines * (cell_h + label_h + margin)), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A3.8 viewer ambient light: night cockpits (procedural sky, Auto exposure) and the E195 "
                            "exterior with the default HDR", font=font, fill="white")
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
    OUT.mkdir(parents=True, exist_ok=True)
    validate_night.OUT.mkdir(parents=True, exist_ok=True)
    rows, failed = [], []

    def add(name, png, data, ok):
        rows.append((name, png, data))
        if not ok:
            failed.append(name)

    if validate_night.DA62_COCKPIT.exists():
        # 100 and 1000 cd/m^2 beyond the brief: the screens (MSFS-scaled emissive) pin the highlight meter.
        for cd in (None, 1, 3, 10, 100, 1000):
            name = f"da62_night_ambient_{'off' if cd is None else cd}"
            add(name, *capture(name, validate_night.DA62_COCKPIT, SKY_NIGHT + validate_night.DA62_CAMERA + ambient(cd)))
    else:
        print("DA62 SDK cockpit not available")
    cockpit = REAL_ASSETS.get("Cockpit")
    if cockpit and Path(cockpit).exists():
        for cd in (None, 3):
            name = f"e195_cockpit_night_ambient_{'off' if cd is None else cd}"
            add(name, *capture(name, cockpit, list(view("cockpit_main_displays")["flags"]) + SKY_NIGHT + ambient(cd),
                               extra_paths=E195_SEARCH_PATHS))
    else:
        print("E195 cockpit not available")
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        for cd in (None, 3):
            name = f"e195_exterior_hdr_ambient_{'off' if cd is None else cd}"
            add(name, *capture(name, exterior, list(view("exterior_fuselage_nose")["flags"]) + ambient(cd)))
    else:
        print("E195 exterior not available")
    for name, png, _ in rows:
        if png:
            print(f"  {name}: mean luma {mean_luma(png):.4f}")
    if any(png for _, png, _ in rows):
        contact_sheet(rows)
    print("RESULT: " + ("FAIL " + ", ".join(failed) if failed else "OK"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
