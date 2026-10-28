# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.7 review captures: ASOBO_sky_portal on the DA62 SDK cockpit, next to the SDK's before/after example.

Rows: the pilot's view and the seats/panel view of the DA62 SDK cockpit (textures through --search-path, as the other
reviews). Columns: portals off / on with the sun at az 45 el 40 (shadows on), off / on with the sun at az 250 el 22,
and the shadow-visibility debug view of each sun (where the A3.1 / A3.2 sun already reaches the cabin through the
glass, which casts no shadow). A last row holds the gizmo view (window dump, Show sky portals) and the SDK pair
(lights_11_skyportal_example_1/_2.png, read only). Default HDR, its sky as background, ACES Hill, exposure 0, no AA.

Writes build/review/skyportal/<name>.png, review_sky_portal.json (changed share, mean change per channel, sun
visibility share) and contact_sheet.png. One Glider process at a time; every process is waited for (the subprocess
timeout kills it); nothing is written outside build/.

Run: python scripts/review_sky_portal.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from review_shadows import DA62_COCKPIT, DA62_TEXTURES  # noqa: E402
from validate_asobo_common import EXE, REPO  # noqa: E402

OUT = REPO / "build" / "review" / "skyportal"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "none", "--fixed-frame-time", "0.05", "--vsync", "off"]
SDK_IMAGES = Path(r"C:\MSFS 2024 Flighting SDK\Documentation\public\images\3_Models_And_Textures\Aircraft\airframe\lights")
SUNS = {"sunA": ["--sun", "on", "--sun-dir", "45,40", "--sun-intensity", "8"],
        "sunB": ["--sun", "on", "--sun-dir", "250,22", "--sun-intensity", "8"]}
VIEWS = {
    "pilot": ("DA62 cockpit, pilot's view",
              ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20",
               "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid"]),
    "seats": ("DA62 cockpit, seats and panel",
              ["--camera-target", "0.0,0.45,0.6", "--camera-distance", "1.6", "--camera-pitch", "-25",
               "--camera-fov", "60", "--camera-yaw", "180", "--hide-grid"]),
}
OFF = ["--asobo-disable", "ASOBO_sky_portal"]


def capture(name, args):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(DA62_COCKPIT)] + args + COMMON + ["--search-path", str(DA62_TEXTURES),
                            "--capture-viewport", str(png), "--capture-frame", "30", "--profile-report", str(report),
                            "--exit-after-profile"], cwd=REPO, capture_output=True, text=True, timeout=900)
    log = result.stdout + result.stderr
    (OUT / f"{name}.log").write_text(log, encoding="utf-8")
    ok = png.exists() and "GL automation errors: 0" in log
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return png if png.exists() else None, ok


def dump_window(name, args):
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(DA62_COCKPIT)] + args + COMMON + ["--search-path", str(DA62_TEXTURES),
                            "--dump-frame", str(png), "--exit-after-dump"], cwd=REPO, capture_output=True, text=True,
                            timeout=900)
    ok = png.exists() and result.returncode == 0
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return png if png.exists() else None, ok


def stats(off, on):
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(off).convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(on).convert("RGB"), dtype=np.int16)
    d = b - a
    m = np.abs(d).max(axis=2)
    changed = m > 0
    return {"changedShare": float(changed.mean()), "changedBy4LevelsShare": float((m >= 4).mean()),
            "maxLevels": int(m.max()),
            "meanChangeOnChangedRGB": [float(v) for v in (d[changed].mean(axis=0) if changed.any() else [0, 0, 0])],
            "meanLevelOffRGB": [float(v) for v in a[changed].mean(axis=0)] if changed.any() else [0, 0, 0]}


def lit_share(debug_png):
    import numpy as np
    from PIL import Image
    v = np.asarray(Image.open(debug_png).convert("L"), dtype=np.float64) / 255.0
    return {"litShare": float((v > 0.5).mean())}


def sdk_stats():
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(SDK_IMAGES / "lights_11_skyportal_example_1.png").convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(SDK_IMAGES / "lights_11_skyportal_example_2.png").convert("RGB"), dtype=np.int16)
    regions = {"face": (575, 90, 690, 230), "left panel": (40, 280, 350, 440), "right wall": (820, 60, 1000, 450)}
    return {name: {"meanChangeRGB": [float(v) for v in (b - a)[y0:y1, x0:x1].reshape(-1, 3).mean(axis=0)],
                   "meanLevelBeforeRGB": [float(v) for v in a[y0:y1, x0:x1].reshape(-1, 3).mean(axis=0)]}
            for name, (x0, y0, x1, y1) in regions.items()}


def contact_sheet(rows):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 17)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin, columns = 400, 26, 10, 6
    cell_h = int(cell_w * 9 / 16)
    width = margin + columns * (cell_w + margin)
    height = 46 + len(rows) * (cell_h + label_h + margin)
    sheet = Image.new("RGB", (width, height), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A3.7 sky portals, DA62 SDK cockpit: off / on per sun (shadows on), the sun's shadow "
              "visibility, gizmo view, SDK before/after (default HDR, ACES Hill, exposure 0)", font=font, fill="white")
    for r, cells in enumerate(rows):
        top = 46 + r * (cell_h + label_h + margin)
        for c, (label, png) in enumerate(cells):
            left = margin + c * (cell_w + margin)
            draw.text((left, top), label, font=font, fill=(205, 210, 220))
            if png:
                image = Image.open(png).convert("RGB")
                image.thumbnail((cell_w, cell_h))
                sheet.paste(image, (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    if not DA62_COCKPIT.exists():
        print(f"DA62 cockpit not found: {DA62_COCKPIT}")
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    summary, failed, rows = {}, [], []
    for key, (title, flags) in VIEWS.items():
        row = []
        for sun, sun_flags in SUNS.items():
            off, ok_off = capture(f"{key}_{sun}_off", flags + sun_flags + OFF)
            on, ok_on = capture(f"{key}_{sun}_on", flags + sun_flags)
            failed += [n for n, ok in ((f"{key}_{sun}_off", ok_off), (f"{key}_{sun}_on", ok_on)) if not ok]
            if off and on:
                summary[f"{key}_{sun}"] = stats(off, on)
            row += [(f"{title}: {sun} portals off", off), (f"{sun} portals on", on)]
        for sun, sun_flags in SUNS.items():
            debug, ok = capture(f"{key}_{sun}_shadowvisibility", flags + sun_flags + ["--debug-view", "shadowvisibility"])
            if not ok:
                failed.append(f"{key}_{sun}_shadowvisibility")
            if debug:
                summary.setdefault(f"{key}_{sun}", {}).update(lit_share(debug))
            row.append((f"{sun} sun visibility (white = lit)", debug))
        rows.append(row)
    # No sun: the default HDR only.
    off, _ = capture("pilot_nosun_off", VIEWS["pilot"][1] + OFF)
    on, _ = capture("pilot_nosun_on", VIEWS["pilot"][1])
    if off and on:
        summary["pilot_nosun"] = stats(off, on)
    gizmo, ok = dump_window("gizmo_window", ["--camera-target", "0.0,0.6,-0.2", "--camera-distance", "2.6",
                                             "--camera-pitch", "35", "--camera-yaw", "200", "--hide-grid",
                                             "--show-sky-portals"])
    if not ok:
        failed.append("gizmo_window")
    selected, ok = dump_window("gizmo_selected", ["--camera-target", "0.0,0.6,-0.2", "--camera-distance", "2.6",
                                                  "--camera-pitch", "35", "--camera-yaw", "200", "--hide-grid",
                                                  "--select-node", "PORTAL_Window_FL"])
    if not ok:
        failed.append("gizmo_selected")
    summary["sdk_pair"] = sdk_stats()
    rows.append([("no sun, portals off", off), ("no sun, portals on", on),
                 ("gizmo view (Show sky portals)", gizmo), ("selected PORTAL_Window_FL", selected),
                 ("SDK: without sky portal", SDK_IMAGES / "lights_11_skyportal_example_1.png"),
                 ("SDK: with sky portal / AO", SDK_IMAGES / "lights_11_skyportal_example_2.png")])
    (OUT / "review_sky_portal.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    contact_sheet(rows)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}; failed: {failed or 'none'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
