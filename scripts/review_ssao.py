# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.5 review captures: screen-space ambient occlusion on the DA62 SDK cockpit (two views), the E195 cockpit and the
E195 exterior.

Columns: SSAO off (the current image), SSAO on (defaults: radius 0.5 m, medium, full resolution), SSAO on with a
1.5 m radius, and the `ssao` debug view (raw visibility, radius 0.5 m). Default HDR, its sky as background, ACES
Hill, exposure 0, no AA. Writes build/review/ssao/<row>_<column>.png, review_ssao.json (the per-capture SSAO state
and the share of pixels that changed against the off image) and contact_sheet.png. One Glider process at a time;
every process is waited for (subprocess timeout kills it); nothing is written outside build/.

Run: python scripts/review_ssao.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from review_shadows import DA62_COCKPIT, DA62_TEXTURES  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view  # noqa: E402

OUT = REPO / "build" / "review" / "ssao"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "none", "--fixed-frame-time", "0.05", "--vsync", "off"]

COLUMNS = [
    ("off", "SSAO off", []),
    ("on", "SSAO on (0.5 m, medium, full)", ["--ssao", "on"]),
    ("wide", "SSAO on, radius 1.5 m", ["--ssao", "on", "--ssao-radius", "1.5"]),
    ("debug", "debug view ssao (0.5 m)", ["--ssao", "on", "--debug-view", "ssao"]),
]


def rows():
    result = []
    if DA62_COCKPIT.exists():
        result.append(("da62_cockpit", "DA62 SDK cockpit",
                       [str(DA62_COCKPIT), "--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05",
                        "--camera-pitch", "-20", "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid",
                        "--search-path", str(DA62_TEXTURES)]))
        result.append(("da62_panel", "DA62 SDK cockpit, panel and seats",
                       [str(DA62_COCKPIT), "--camera-target", "0.0,0.45,0.6", "--camera-distance", "1.6",
                        "--camera-pitch", "-25", "--camera-fov", "60", "--camera-yaw", "180", "--hide-grid",
                        "--search-path", str(DA62_TEXTURES)]))
    cockpit = REAL_ASSETS.get("Cockpit")
    if cockpit and Path(cockpit).exists():
        result.append(("e195_cockpit", "E195 cockpit (cockpit_main_displays)",
                       [str(cockpit)] + view("cockpit_main_displays")["flags"] + ["--hide-grid"]))
    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        result.append(("e195_exterior", "E195 exterior (exterior_fuselage_nose)",
                       [str(exterior)] + view("exterior_fuselage_nose")["flags"] + ["--hide-grid"]))
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
    state = None
    if report.exists():
        state = json.loads(report.read_text(encoding="utf-8")).get("ssao")
    print(("OK   " if ok else "FAIL ") + name, flush=True)
    return png if png.exists() else None, state, ok


def changed_share(a, b):
    import numpy as np
    from PIL import Image
    x = np.asarray(Image.open(a).convert("RGB"), dtype=np.int16)
    y = np.asarray(Image.open(b).convert("RGB"), dtype=np.int16)
    d = np.abs(x - y).max(axis=2)
    return {"changedShare": float((d > 0).mean()), "changedBy4LevelsShare": float((d >= 4).mean()), "maxLevels": int(d.max())}


def contact_sheet(table):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 18)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin = 480, 28, 10
    first = next(png for row in table for png in row[2] if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    width = margin + len(COLUMNS) * (cell_w + margin)
    height = 46 + len(table) * (cell_h + label_h + margin)
    sheet = Image.new("RGB", (width, height), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A3.5 SSAO: off / on (0.5 m) / on (1.5 m) / debug view (default HDR, ACES Hill, exposure 0)",
              font=font, fill="white")
    for r, (_, labels, pngs) in enumerate(table):
        top = 46 + r * (cell_h + label_h + margin)
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
            png, state, ok = capture(name, args + flags)
            pngs.append(png)
            summary[name] = {"ssao": state}
            if not ok:
                failed.append(name)
        for index in (1, 2):
            if pngs[0] and pngs[index]:
                summary[f"{key}_{COLUMNS[index][0]}"].update(changed_share(pngs[0], pngs[index]))
        table.append((key, [f"{title}: {COLUMNS[0][1]}"] + [c[1] for c in COLUMNS[1:]], pngs))
    (OUT / "review_ssao.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    contact_sheet(table)
    print(f"contact sheet: {OUT / 'contact_sheet.png'}; failed: {failed or 'none'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
