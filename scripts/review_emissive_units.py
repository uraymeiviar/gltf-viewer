# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.6b serial review: old/new cockpits, ambient at calibrated units, Auto/Curve and exterior across the day."""

import argparse
import sys
from pathlib import Path

import review_night_interior as review
import validate_night as night
from validate_asobo_common import EXE, REAL_ASSETS, REPO, view
from validate_ground import without_ground_equipment

OUT = REPO / "build" / "review" / "emissive_units"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-exe", type=Path, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    review.OUT = OUT
    review.SKY += ["--sky-time-slicing", "off"]
    rows, failures = [], []

    def add(name, model, flags, exe=EXE, paths=()):
        png, data, ok = review.capture(name, model, flags, exe=exe, extra_paths=paths)
        rows.append((name, png, data))
        if not ok:
            failures.append(name)
        if data:
            night.describe(name, data)

    cockpit = REAL_ASSETS["Cockpit"]
    from validate_asobo_common import E195_SEARCH_PATHS
    for aircraft, model, camera in (("e195", cockpit, list(view("cockpit_main_displays")["flags"])),
                                    ("da62", night.DA62_COCKPIT, night.DA62_CAMERA)):
        if not Path(model).exists():
            print(f"SKIP {aircraft} not installed")
            continue
        paths = E195_SEARCH_PATHS if aircraft == "e195" else ()
        screens = night.E195_SCREENS if aircraft == "e195" else night.DA62_SCREENS
        reports = [a for n in screens for a in ("--report-node", n)]
        base = list(camera) + reports
        add(f"{aircraft}_night_old", model, base + review.NIGHT, args.baseline_exe.resolve(), paths)
        add(f"{aircraft}_night_new", model, base + review.NIGHT, paths=paths)
        for when, flags in (("night", review.NIGHT), ("noon", review.NOON)):
            noon_camera = list(view("cockpit_windshield_inside")["flags"]) + reports if aircraft == "e195" and when == "noon" else base
            if when == "noon":
                add(f"{aircraft}_noon_new", model, noon_camera + flags, paths=paths)
            add(f"{aircraft}_{when}_curve", model, noon_camera + flags + ["--exposure-mode", "curve"], paths=paths)
        if aircraft == "da62":
            for intensity in (3, 30):
                add(f"da62_night_ambient_{intensity}", model,
                    base + review.NIGHT + ["--ambient", "on", "--ambient-intensity", str(intensity)])
    exterior = REAL_ASSETS["Exterior"]
    if Path(exterior).exists():
        model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
        sunset_h, _ = night.sunset_times()
        for name, when in (("noon", review.NOON), ("sunset", ["--time", night.hhmm(sunset_h)]), ("night", review.NIGHT)):
            for mode in ("auto", "curve"):
                add(f"e195_exterior_{name}_{mode}", model,
                    when + ["--ground", "plane", "--exposure-mode", mode] + review.E195_CAMERA)
    # Reuse the sheet layout; give this review its own accurate title.
    review.contact_sheet(rows)
    from PIL import Image, ImageDraw, ImageFont
    path = OUT / "contact_sheet.png"
    sheet = Image.open(path)
    draw = ImageDraw.Draw(sheet)
    draw.rectangle((0, 0, sheet.width, 45), fill=(24, 26, 30))
    draw.text((12, 12), "A1.6b: ASOBO sky emissive in cd/m^2 (1/12800); baseline 1/1000; Seattle; Auto unless labelled Curve",
              font=ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 17), fill="white")
    sheet.save(path)
    print("RESULT: " + ("FAIL " + ", ".join(failures) if failures else "PASS"))
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
