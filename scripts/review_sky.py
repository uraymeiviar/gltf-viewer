# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.2 / A6.3 review captures: the procedural sky and the environment lighting filtered from it.

Default place and date (Seattle-Tacoma, 2026-04-15, UTC-7), heading 0, the sun from the clock (time_of_day_mode on),
--environment sky (Hillaire 2020 atmosphere; sun colour and illuminance from its transmittance), sun shadows, TAA,
exposure 0 (A6.4 will refine it):
  * E195 exterior on the A3.6 ground plane (Model base, without its ground support equipment) at dawn, noon,
    afternoon, sunset (the computed sunset time, geometric elevation -0.83 degrees) and dusk (sunset + 35 min);
  * the DA62 SDK cockpit at noon and sunset (if the MSFS SDK sample is installed; read only);
  * the sky alone looking at the horizon towards the setting sun (an empty scene, over the control pipe), at
    exposure 0 and +4 EV.
Sunset and dusk are also captured with auto exposure ("_ae"): at exposure 0 they are physically dark (A6.4).
Writes build/review/sky/<name>.png / .json and contact_sheet.png. One Glider process at a time; nothing is written
outside build/.

Run: python scripts/review_sky.py
"""

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REAL_ASSETS, REPO  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402
import validate_sky  # noqa: E402

OUT = REPO / "build" / "review" / "sky"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_COCKPIT = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_TEXTURES = DA62 / "Sources/Art/Max/textures"
DA62_CAMERA = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20", "--camera-fov", "55",
               "--camera-yaw", "220", "--search-path", str(DA62_TEXTURES)]
E195_CAMERA = ["--camera-yaw", "35", "--camera-pitch", "12"]


def capture(name, model, flags, frame=64):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    args = [str(EXE)] + ([str(model)] if model else []) + COMMON + SKY + list(flags)
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
    tod, sky = data.get("timeOfDay", {}), data.get("sky", {})
    sun = sky.get("sunIlluminance", [0, 0, 0])
    return (f"{name} {tod.get('time', '')}  sun el {tod.get('sun', {}).get('elevation', 0):.1f}  "
            f"E {0.2126 * sun[0] + 0.7152 * sun[1] + 0.0722 * sun[2]:.2f}  IBL {sky.get('refine', {}).get('frontSize', 0)} px")


def sunset_times():
    """The default date's sunset (local clock) from a profile report, and dusk 35 minutes later."""
    _, data, _ = capture("_events", None, ["--time-preset", "noon"], frame=2)
    sunset = data["timeOfDay"]["events"]["sunset"]["hours"]
    fmt = lambda h: f"{int(h):02d}:{int(h * 60) % 60:02d}:{int(round(h * 3600)) % 60:02d}"  # noqa: E731
    return fmt(sunset), fmt(sunset + 35.0 / 60.0)


def sky_only(sunset):
    """The horizon towards the setting sun, sky only (empty scene), at sunset: exposure 0 and +4 EV."""
    session = validate_sky.Session(validate_sky.empty_model(), SKY + ["--time", sunset, "--aa", "none"])
    rows = []
    try:
        state = session.call("get_state")
        azimuth = state["timeOfDay"]["sun"]["gliderAzimuth"]
        view = validate_sky.direction(azimuth, 6.0)
        session.place([0.0, 0.0, 0.0], list(view), fov_y=60.0)
        for name, ev in (("sky_horizon_sunset", 0.0), ("sky_horizon_sunset_ev4", 4.0)):
            png = OUT / f"{name}.png"
            session.env("exposure", ev)
            session.call("capture", path=str(png), settle_frames=24, overlays=False)
            data = {"timeOfDay": session.call("get_state")["timeOfDay"], "sky": session.call("get_state")["sky"]}
            (OUT / f"{name}.json").write_text(json.dumps(data, indent=1), encoding="utf-8")
            rows.append([name, png if png.exists() else None, data, True])
        errors = session.close()
        for row in rows:
            row[3] = errors == 0
            print(("OK   " if errors == 0 else "FAIL ") + row[0], flush=True)
        return rows
    except Exception as error:  # noqa: BLE001
        print(f"FAIL sky_horizon_sunset: {error}")
        session.process.kill()
        return [["sky_horizon_sunset", None, None, False]]


def contact_sheet(rows):
    from PIL import Image, ImageDraw, ImageFont
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 18)
    except OSError:
        font = ImageFont.load_default()
    cell_w, label_h, margin, columns = 560, 28, 12, 3
    first = next(png for _, png, _ in rows if png)
    w0, h0 = Image.open(first).size
    cell_h = int(cell_w * h0 / w0)
    lines = math.ceil(len(rows) / columns)
    sheet = Image.new("RGB", (margin + columns * (cell_w + margin), 50 + lines * (cell_h + label_h + margin)), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "A6.2/A6.3 procedural sky: Seattle 2026-04-15, heading 0, sun from the atmosphere, "
                            "IBL filtered from the sky, shadows, TAA, exposure 0", font=font, fill="white")
    for i, (name, png, data) in enumerate(rows):
        left = margin + (i % columns) * (cell_w + margin)
        top = 50 + (i // columns) * (cell_h + label_h + margin)
        draw.text((left, top), label_of(name, data), font=font, fill=(205, 210, 220))
        if png:
            sheet.paste(Image.open(png).convert("RGB").resize((cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sunset, dusk = sunset_times()
    rows, failed = [], []

    def add(name, png, data, ok):
        rows.append((name, png, data))
        if not ok:
            failed.append(name)

    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
        auto = ["--auto-exposure", "on"]
        for name, when in (("dawn", ["--time-preset", "dawn"]), ("noon", ["--time-preset", "noon"]),
                           ("afternoon", ["--time-preset", "afternoon"]), ("sunset", ["--time", sunset]),
                           ("sunset_ae", ["--time", sunset] + auto), ("dusk", ["--time", dusk]),
                           ("dusk_ae", ["--time", dusk] + auto)):
            add(f"e195_{name}", *capture(f"e195_{name}", model, when + ["--ground", "plane"] + E195_CAMERA))
    else:
        print("E195 exterior not available")
    if DA62_COCKPIT.exists():
        for name, when in (("noon", ["--time-preset", "noon"]), ("sunset", ["--time", sunset]),
                           ("sunset_ae", ["--time", sunset, "--auto-exposure", "on"])):
            add(f"da62_{name}", *capture(f"da62_{name}", DA62_COCKPIT, when + DA62_CAMERA))
    else:
        print("DA62 SDK cockpit not available")
    for name, png, data, ok in sky_only(sunset):
        add(name, png, data, ok)
    if any(png for _, png, _ in rows):
        contact_sheet(rows)
    print("RESULT: " + ("FAIL " + ", ".join(failed) if failed else "OK"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
