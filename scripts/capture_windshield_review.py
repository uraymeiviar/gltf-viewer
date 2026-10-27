# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Capture reference windshield views for E195, PMDG 737, and PMDG 777.

Captures before (99de26c.exe), after (ice 0), after (ice 0.5), and after (ice 1.0),
plus profile reports, and builds a comprehensive contact sheet.
"""

import glob
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASELINE = REPO / "build" / "baselines" / "99de26c.exe"
REVIEW_DIR = REPO / "build" / "review" / "windshield"
REVIEW_DIR.mkdir(parents=True, exist_ok=True)

COMMON_ARGS = [
    "--isolated",
    "--window-size", "1600x900",
    "--aa", "none",
    "--hide-grid",
    "--clear-color", "0.02,0.02,0.025",
    "--fixed-frame-time", "0.05",
    "--vsync", "off",
]

def run_capture(exe, model_path, output_png, profile_json, camera_args, extra_args=()):
    cmd = [
        str(exe),
        str(model_path),
        *COMMON_ARGS,
        *camera_args,
        *extra_args,
        "--capture-viewport", str(output_png),
        "--capture-frame", "40",
        "--profile-report", str(profile_json),
        "--exit-after-profile"
    ]
    print(f"Running: {' '.join(cmd[:6])} ... -> {output_png.name}", flush=True)
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, timeout=300)
    if res.returncode != 0 or not output_png.is_file():
        print(f"Error running capture for {output_png.name} (code {res.returncode}):", flush=True)
        print(res.stderr[-1000:], flush=True)
        return False
    return True

def main():
    e195_model = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf")
    e195_tex_dirs = glob.glob(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\texture*")
    e195_search = []
    for td in e195_tex_dirs:
        e195_search += ["--search-path", td]
    e195_cam = ["--camera-target", "0,1.74,11.86", "--camera-yaw", "180", "--camera-pitch", "5", "--camera-distance", "0.25", "--camera-fov", "75"]

    pmdg737_model = REPO / "build" / "review" / "pmdg" / "vc.gltf"
    pmdg737_cam = ["--camera-target", "0.001,1.547,15.616", "--camera-yaw", "180", "--camera-pitch", "0", "--camera-distance", "0.75", "--camera-fov", "75"]

    pmdg777_model = REPO / "build" / "review" / "detailmap" / "pmdg773_vc_abs.gltf"
    pmdg777_cam = ["--camera-target", "-0.005,2.287,30.524", "--camera-yaw", "180", "--camera-pitch", "0", "--camera-distance", "1.2", "--camera-fov", "75"]

    aircraft_tasks = [
        ("e195", e195_model, e195_cam, e195_search),
        ("pmdg737", pmdg737_model, pmdg737_cam, []),
        ("pmdg777", pmdg777_model, pmdg777_cam, []),
    ]

    for prefix, model, cam, search in aircraft_tasks:
        print(f"\n=== Processing {prefix} ===")
        # 1. Before
        before_png = REVIEW_DIR / f"{prefix}_before.png"
        before_json = REVIEW_DIR / f"{prefix}_before_profile.json"
        run_capture(BASELINE, model, before_png, before_json, cam, search)

        # 2. After (ice 0)
        ice0_png = REVIEW_DIR / f"{prefix}_after_ice0.png"
        ice0_json = REVIEW_DIR / f"{prefix}_after_ice0_profile.json"
        run_capture(EXE, model, ice0_png, ice0_json, cam, [*search, "--windshield-icing", "0.0"])

        # 3. After (ice 0.5)
        ice05_png = REVIEW_DIR / f"{prefix}_after_ice05.png"
        ice05_json = REVIEW_DIR / f"{prefix}_after_ice05_profile.json"
        run_capture(EXE, model, ice05_png, ice05_json, cam, [*search, "--windshield-icing", "0.5"])

        # 4. After (ice 1.0)
        ice1_png = REVIEW_DIR / f"{prefix}_after_ice1.png"
        ice1_json = REVIEW_DIR / f"{prefix}_after_ice1_profile.json"
        run_capture(EXE, model, ice1_png, ice1_json, cam, [*search, "--windshield-icing", "1.0"])

    print("\n=== Generating Contact Sheet ===")
    cols = ["before", "after_ice0", "after_ice05", "after_ice1"]
    col_labels = ["Baseline (99de26c)", "After (Icing 0.0)", "After (Icing 0.5)", "After (Icing 1.0)"]
    rows = [("e195", "Embraer E195 (v3)"), ("pmdg737", "PMDG 737-900 (Legacy)"), ("pmdg777", "PMDG 777-300ER (Legacy)")]

    thumb_w, thumb_h = 400, 225
    margin = 20
    header_h = 50
    row_label_w = 220

    sheet_w = row_label_w + len(cols) * (thumb_w + margin) + margin
    sheet_h = header_h + len(rows) * (thumb_h + margin) + margin

    sheet = Image.new("RGB", (sheet_w, sheet_h), (25, 25, 30))
    draw = ImageDraw.Draw(sheet)

    for c_idx, cl in enumerate(col_labels):
        x = row_label_w + c_idx * (thumb_w + margin) + thumb_w // 2
        draw.text((x, 15), cl, fill=(240, 240, 240), anchor="mt")

    for r_idx, (r_prefix, r_title) in enumerate(rows):
        y = header_h + r_idx * (thumb_h + margin)
        draw.text((margin, y + thumb_h // 2), r_title, fill=(220, 220, 220), anchor="lm")
        for c_idx, col in enumerate(cols):
            img_path = REVIEW_DIR / f"{r_prefix}_{col}.png"
            if img_path.is_file():
                im = Image.open(img_path)
                im.thumbnail((thumb_w, thumb_h), Image.Resampling.LANCZOS)
                x = row_label_w + c_idx * (thumb_w + margin)
                sheet.paste(im, (x, y))

    contact_path = REVIEW_DIR / "windshield_contact_sheet.png"
    sheet.save(contact_path)
    print(f"Contact sheet saved: {contact_path}")

if __name__ == "__main__":
    main()
