# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.7 review captures: the sun disk, its glare (bloom) and the optional lens effect.

All over the control pipe (the camera is aimed from the sun direction Glider reports), 1600 x 900, TAA, bloom on:
  * e195_sunset_plane[_ae]: E195 exterior (without its ground equipment) on the A3.6 ground plane, procedural sky,
    A6.1 clock at the default date and place, the sun 3 degrees above the horizon (preset dusk), seen past the aircraft;
    exposure 0 and auto exposure;
  * e195_sunset_lens_off / _lens_on: the same view (auto exposure) without and with the lens effect (ghosts + streak);
  * noon_sky / noon_sky_lens: an empty scene, procedural sky at noon, looking at the sun (FoV 40), lens off / on;
  * windshield_sun: the E195 exterior windshield (reference view 10's camera) with the sun placed at the mirror
    direction of the view ray about the glass normal at the image centre (normal from probed world positions), HDR
    environment, Manual sun: the A3.1 highlight on the glass;
  * contact_sheet.png.
Writes build/review/sundisk/. One Glider process at a time; nothing is written outside build/.

Run: python scripts/review_sun_disk.py
"""

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_asobo_common import REAL_ASSETS, REPO  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402
import validate_sun_disk as vsd  # noqa: E402

OUT = REPO / "build" / "review" / "sundisk"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "taa", "--fixed-frame-time", "0.05", "--vsync", "off"]
SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--hide-grid"]


def normalize(v):
    n = math.sqrt(sum(c * c for c in v))
    return [c / n for c in v]


def angles(d):
    return math.degrees(math.atan2(d[0], d[2])) % 360.0, math.degrees(math.asin(max(-1.0, min(1.0, d[1]))))


def shoot(session, name, frames=48):
    png = OUT / f"{name}.png"
    session.call("capture", path=str(png), settle_frames=frames, overlays=False)
    state = session.call("get_state")
    data = {"sun": state["sun"], "sky": state.get("sky"), "timeOfDay": state.get("timeOfDay"),
            "camera": session.call("get_camera")}
    (OUT / f"{name}.json").write_text(json.dumps(data, indent=1), encoding="utf-8")
    print(f"OK   {name}", flush=True)
    return name, png if png.exists() else None, data


def e195_sunset(rows):
    exterior = REAL_ASSETS.get("Exterior")
    if not (exterior and Path(exterior).exists()):
        print("E195 exterior not available")
        return
    model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
    session = vsd.Session(model, SKY + ["--time-preset", "dusk", "--ground", "plane", "--shadows", "on"], common=COMMON)
    try:
        sun = session.call("get_state")["sun"]["direction"]
        centre = session.call("get_camera")["target"]
        horizontal = normalize([sun[0], 0.0, sun[2]])
        # Behind the aircraft as seen from the sun, 40 m away, 4 m up; aim 9 degrees beside the sun so both show.
        side = [horizontal[2], 0.0, -horizontal[0]]
        eye = [centre[0] - 40.0 * horizontal[0] + 12.0 * side[0], centre[1] + 4.0, centre[2] - 40.0 * horizontal[2] + 12.0 * side[2]]
        target = [centre[0], centre[1] + 1.5, centre[2]]
        session.place(eye, target, 50.0)
        session.converge()
        rows.append(shoot(session, "e195_sunset_plane"))
        session.env("auto_exposure", True)
        rows.append(shoot(session, "e195_sunset_plane_ae"))
        rows.append(shoot(session, "e195_sunset_lens_off"))
        session.call("set_viewport_setting", key="lens_flare", value="both")
        rows.append(shoot(session, "e195_sunset_lens_on"))
        errors = session.close()
        print(f"     e195 session GL errors: {errors}")
    except Exception as error:  # noqa: BLE001
        print(f"FAIL e195 sunset: {error}")
        session.process.kill()


def noon_sky(rows):
    session = vsd.Session(vsd.empty_model(), SKY + ["--time-preset", "noon"], common=COMMON)
    try:
        sun = session.call("get_state")["sun"]["direction"]
        session.look(vsd.offset_direction(sun, 6.0, -8.0), 40.0)
        session.converge()
        rows.append(shoot(session, "noon_sky"))
        session.call("set_viewport_setting", key="lens_flare", value="both")
        rows.append(shoot(session, "noon_sky_lens"))
        errors = session.close()
        print(f"     noon sky session GL errors: {errors}")
    except Exception as error:  # noqa: BLE001
        print(f"FAIL noon sky: {error}")
        session.process.kill()


def windshield(rows):
    exterior = REAL_ASSETS.get("Exterior")
    if not (exterior and Path(exterior).exists()):
        return
    camera = ["--camera-target", "0.0,1.80,12.8", "--camera-yaw", "-25", "--camera-pitch", "-15", "--camera-distance", "2.0"]
    session = vsd.Session(exterior, camera + ["--sun", "on", "--sun-intensity", "8"], common=COMMON)
    try:
        session.settle(8)
        vp = session.call("get_state")["viewports"][0]
        w, h = vp["width"], vp["height"]
        # Probed world positions on BLEND glass are those of the cabin behind it, so the glass normal is searched
        # instead: the sun is put at the mirror direction of the view ray (towards the windshield centre, the
        # reference view's target) for candidate normals, and the brightest highlight in the windshield band wins.
        eye = session.call("get_camera")["eye"]
        target = [0.0, 1.80, 12.8]
        v = normalize([eye[i] - target[i] for i in range(3)])
        best = None
        for n_el in range(10, 75, 5):
            for n_az in range(-40, 45, 10):
                n = vsd.direction(n_az, n_el)
                d = sum(n[i] * v[i] for i in range(3))
                if d <= 0.05:
                    continue
                sun = [2.0 * d * n[i] - v[i] for i in range(3)]
                session.env("sun_dir", list(angles(sun)))
                session.settle(3)
                peak = session.call("probe_region", x0=int(0.38 * w), y0=int(0.48 * h), x1=int(0.95 * w),
                                    y1=int(0.68 * h))["luminance"]["max"]
                if best is None or peak > best[0]:
                    best = (peak, n, sun)
        azimuth, elevation = angles(best[2])
        print(f"     windshield normal {[round(c, 3) for c in best[1]]}, sun az {azimuth:.1f} el {elevation:.1f}, "
              f"highlight peak {best[0]:.1f}")
        session.env("sun_dir", [azimuth, elevation])
        rows.append(shoot(session, "windshield_sun"))
        session.env("sun", False)
        rows.append(shoot(session, "windshield_sun_off"))
        errors = session.close()
        print(f"     windshield session GL errors: {errors}")
    except Exception as error:  # noqa: BLE001
        print(f"FAIL windshield: {error}")
        session.process.kill()


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
    draw.text((margin, 12), "A6.7 sun disk, bloom glare and lens effect (TAA, bloom on)", font=font, fill="white")
    for i, (name, png, data) in enumerate(rows):
        left = margin + (i % columns) * (cell_w + margin)
        top = 50 + (i // columns) * (cell_h + label_h + margin)
        sun = (data or {}).get("sun", {})
        label = f"{name}  sun el {sun.get('elevation', 0):.1f}  disk {'on' if sun.get('disk', {}).get('visible') else 'off'}"
        draw.text((left, top), label, font=font, fill=(205, 210, 220))
        if png:
            sheet.paste(Image.open(png).convert("RGB").resize((cell_w, cell_h)), (left, top + label_h))
    sheet.save(OUT / "contact_sheet.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    vsd.OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    e195_sunset(rows)
    noon_sky(rows)
    windshield(rows)
    if any(png for _, png, _ in rows):
        contact_sheet(rows)
    print(f"RESULT: {len(rows)} captures in {OUT}")
    return 0 if rows else 1


if __name__ == "__main__":
    sys.exit(main())
