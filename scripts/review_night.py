# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.4 / A6.5 / A6.6 review captures: exposure across the day, the moon, the stars, emissive panels at night.

Seattle-Tacoma, UTC-7, heading 0, procedural sky with the A6.1 clock, the sky's default exposure (Time-of-day curve) and
tone mapper (PBR Neutral), sun/moon shadows, TAA:
  * E195 exterior on the A3.6 ground plane (without its ground support equipment): noon, sunset (geometric -0.83 deg),
    dusk (sunset + 35 min, about -6 deg), a full-moon night (2026-05-01 23:30, moon 15 deg up) and a moonless night
    (2026-04-15 solar midnight, stars), plus the moonless night with Auto exposure;
  * the E195 cockpit main displays (Auto exposure, and the curve for comparison) and the DA62 SDK cockpit (Auto; if
    installed, read only) at night with Automatic day/night (ASOBO_material_emissive night multipliers);
  * the tone mapper comparison at sunset: the same frame through PBR Neutral, ACES Hill and ACES Narkowicz; for sky
    pixels around the setting sun the hue shift and the saturation kept between the exposed linear radiance (probe)
    and the displayed colour are printed and written to tonemap_comparison.json.
Writes build/review/night/<name>.png / .json and contact_sheet.png. One Glider process at a time; nothing is written
outside build/.

Run: python scripts/review_night.py
"""

import colorsys
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

OUT = REPO / "build" / "review" / "night"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid"]
DA62 = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
DA62_COCKPIT = DA62 / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
DA62_TEXTURES = DA62 / "Sources/Art/Max/textures"
DA62_CAMERA = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20", "--camera-fov", "55",
               "--camera-yaw", "220", "--search-path", str(DA62_TEXTURES)]
E195_CAMERA = ["--camera-yaw", "35", "--camera-pitch", "12"]
FULL_MOON = ["--date", "2026-05-01", "--time", "23:30"]
NIGHT_EMISSIVE = ["--time-preset", "night", "--time-of-day", "auto"]


def capture(name, model, flags, frame=64, extra_paths=()):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    paths = [arg for path in extra_paths for arg in ("--search-path", path)]
    args = [str(EXE)] + ([str(model)] if model else []) + COMMON + SKY + paths + list(flags)
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
    tod, night = data.get("timeOfDay", {}), data.get("night", {})
    vp = (night.get("exposure", {}).get("viewports") or [{}])[0]
    moon = night.get("moon", {})
    text = f"{name} {tod.get('time', '')} sun {tod.get('sun', {}).get('elevation', 0):.1f}  EV {vp.get('exposureEv', 0):.1f}"
    if moon.get("aboveHorizon"):
        text += f"  moon {moon.get('illuminatedFraction', 0) * 100:.0f}% {moon.get('illuminanceLux', 0):.3f} lx"
    return text


def tonemap_comparison(model, sunset):
    """Sunset frame through each tone mapper: hue shift and saturation kept for sky pixels near the sun."""
    session = validate_night.Session(model, SKY + ["--ground", "plane", "--time", sunset, "--window-size", "1280x720",
                                                   "--aa", "none", "--bloom", "off"] + E195_CAMERA,
                                     common=["--isolated", "--fixed-frame-time", "0.05", "--vsync", "off"])
    rows = []
    try:
        state = session.call("get_state")
        sun_azimuth = state["timeOfDay"]["sun"]["gliderAzimuth"]
        camera = session.look(validate_night_direction(sun_azimuth + 25.0, 6.0), 60.0)
        session.capture("_tm_settle", frames=24)
        exposure_ev = session.call("get_state")["night"]["exposure"]["viewports"][0]["exposureEv"]
        samples = []
        for gy in range(int(camera.height * 0.15), int(camera.height * 0.5), 12):
            for gx in range(0, camera.width, 16):
                samples.append((gx, gy, session.probe(gx, gy)))
        result = {"exposureEv": exposure_ev, "samples": len(samples), "mappers": {}}
        for mapper in ("neutral", "hill", "narkowicz"):
            session.env("tone_mapping", mapper)
            png = session.capture(f"tonemap_sunset_{mapper}", frames=4)
            from PIL import Image
            image = Image.open(png).convert("RGB")
            hue_errors, saturation_kept = [], []
            for gx, gy, hdr in samples:
                exposed = [c * 2.0 ** exposure_ev for c in hdr]
                h0, s0, v0 = colorsys.rgb_to_hsv(*[max(c, 0.0) for c in exposed])
                ldr = [(c / 255.0) ** 2.2 for c in image.getpixel((gx, gy))]
                h1, s1, v1 = colorsys.rgb_to_hsv(*ldr)
                if s0 < 0.15 or v1 < 0.02:
                    continue
                hue_errors.append(abs(((h1 - h0) * 360.0 + 180.0) % 360.0 - 180.0))
                saturation_kept.append(s1 / s0)
            hue_errors.sort()
            entry = {"pixels": len(hue_errors),
                     "hueShiftMeanDeg": sum(hue_errors) / max(len(hue_errors), 1),
                     "hueShift95Deg": hue_errors[int(0.95 * (len(hue_errors) - 1))] if hue_errors else None,
                     "saturationKeptMean": sum(saturation_kept) / max(len(saturation_kept), 1)}
            result["mappers"][mapper] = entry
            print(f"  {mapper:9s}: hue shift mean {entry['hueShiftMeanDeg']:.2f} deg, 95% {entry['hueShift95Deg']:.2f} deg, "
                  f"saturation kept {entry['saturationKeptMean']:.3f} ({entry['pixels']} sky pixels)", flush=True)
            rows.append((f"tonemap_sunset_{mapper}", png, None))
        (OUT / "tonemap_comparison.json").write_text(json.dumps(result, indent=1), encoding="utf-8")
        session.close()
    except Exception as error:  # noqa: BLE001
        print(f"FAIL tone mapper comparison: {error}")
        session.process.kill()
    return rows


def validate_night_direction(azimuth, elevation):
    az, el = math.radians(azimuth), math.radians(elevation)
    return [math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az)]


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
    draw.text((margin, 12), "A6.4-A6.6: Seattle, procedural sky, time-of-day exposure curve, PBR Neutral, moon/stars, "
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
    OUT.mkdir(parents=True, exist_ok=True)
    validate_night.OUT.mkdir(parents=True, exist_ok=True)
    sunset_h, dusk_h = validate_night.sunset_times()
    sunset, dusk = validate_night.hhmm(sunset_h), validate_night.hhmm(dusk_h)
    rows, failed = [], []

    def add(name, png, data, ok):
        rows.append((name, png, data))
        if not ok:
            failed.append(name)

    exterior = REAL_ASSETS.get("Exterior")
    if exterior and Path(exterior).exists():
        model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
        for name, when in (("noon", ["--time-preset", "noon"]), ("sunset", ["--time", sunset]), ("dusk", ["--time", dusk]),
                           ("night_full_moon", FULL_MOON), ("night_moonless", ["--time-preset", "night"]),
                           ("night_moonless_auto", ["--time-preset", "night", "--exposure-mode", "auto"])):
            add(f"e195_{name}", *capture(f"e195_{name}", model, when + ["--ground", "plane"] + E195_CAMERA))
        for name, png, data in tonemap_comparison(model, sunset):
            add(name, png, data, png is not None)
    else:
        print("E195 exterior not available")
    cockpit = REAL_ASSETS.get("Cockpit")
    if cockpit and Path(cockpit).exists():
        # Interiors at night: Auto exposure (metered; the time-of-day curve is the exterior's and leaves the MSFS-scaled
        # panels far brighter than the night around them), and the curve for comparison.
        flags = list(view("cockpit_main_displays")["flags"]) + NIGHT_EMISSIVE
        add("e195_cockpit_night_auto", *capture("e195_cockpit_night_auto", cockpit, flags + ["--exposure-mode", "auto"],
                                                extra_paths=E195_SEARCH_PATHS))
        add("e195_cockpit_night_curve", *capture("e195_cockpit_night_curve", cockpit, flags, extra_paths=E195_SEARCH_PATHS))
    if DA62_COCKPIT.exists():
        add("da62_night_auto", *capture("da62_night_auto", DA62_COCKPIT, NIGHT_EMISSIVE + DA62_CAMERA + ["--exposure-mode", "auto"]))
    else:
        print("DA62 SDK cockpit not available")
    if any(png for _, png, _ in rows):
        contact_sheet(rows)
    print("RESULT: " + ("FAIL " + ", ".join(failed) if failed else "OK"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
