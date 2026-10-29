# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.9 review captures: sky occlusion inside the model (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.9).

Seattle-Tacoma, UTC-7, heading 0, procedural sky with the A6.1 clock, Automatic emissive day/night, Auto exposure (the
sky's default), TAA, 1600 x 900, synchronous sky filtering:
  * E195 cockpit at noon (windshield view), sky occlusion off / on (the default);
  * DA62 SDK cockpit at noon (if installed, read only), off / on;
  * E195 exterior at noon on the A3.6 ground plane (without its ground support equipment), the whole aircraft and the
    left main gear and engine from the front, off / on;
  * E195 cockpit at sunset, on;
  * the Sky Visibility debug view of the E195 cockpit (windshield view) and of the exterior gear view.
It also measures the A3.7 sky portals against the geometric window light in the DA62 cockpit at noon (mean linear
radiance of the lower panel, probe_region): sky occlusion off / on, environment occluders on / off, portal strength
0 / 0.35 / 1. Writes build/review/sky_occlusion/<name>.png / .json, metrics.json and contact_sheet.png. One Glider
process at a time; nothing is written outside build/.

Run: python scripts/review_sky_occlusion.py
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import review_night_interior as review  # noqa: E402
import validate_night as night  # noqa: E402
from validate_asobo_common import E195_SEARCH_PATHS, REAL_ASSETS, REPO, view  # noqa: E402
from validate_ground import without_ground_equipment  # noqa: E402

OUT = REPO / "build" / "review" / "sky_occlusion"
NOON = ["--time-preset", "noon", "--time-of-day", "auto"]
GEAR_VIEW = ["--frame-node", "LG_Main_Rot_L", "--frame-view", "front", "--frame-margin", "5"]


def portal_metrics():
    """Mean linear radiance of the DA62's lower panel under the portal / occluder configurations (pipe session)."""
    if not night.DA62_COCKPIT.exists():
        return None
    night.OUT = OUT
    session = night.Session(night.DA62_COCKPIT, night.SKY + NOON + night.DA62_CAMERA)
    results = {}
    try:
        path = session.capture("da62_portal_probe", frames=24)
        from PIL import Image
        width, height = Image.open(path).size
        region = dict(x0=int(0.08 * width), y0=int(0.80 * height), x1=int(0.92 * width), y1=int(0.97 * height))
        configs = [("off_portals_0.35", "off", True, 0.35), ("on_portals_0.35", "on", True, 0.35),
                   ("on_portals_0", "on", True, 0.0), ("on_portals_1", "on", True, 1.0),
                   ("on_portals_2", "on", True, 2.0), ("on_no_occluders_portals_0", "on", False, 0.0),
                   ("on_no_occluders_portals_0.35", "on", False, 0.35)]
        for name, mode, occluders, strength in configs:
            session.env("sky_occlusion", mode)
            session.env("sky_occlusion_occluders", occluders)
            session.call("set_viewport_setting", key="sky_portal_strength", value=strength)
            session.capture(f"da62_portal_{name}", frames=24)
            probe = session.call("probe_region", **region)
            results[name] = probe["luminance"]["mean"]
            print(f"  DA62 lower panel, {name}: mean linear luminance {results[name]:.6f}")
        results["region"] = region
        results["glErrors"] = session.close()
    except Exception as error:  # noqa: BLE001
        print(f"FAIL portal metrics: {error}")
        session.process.kill()
        return None
    return results


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    review.OUT = OUT
    review.SKY += ["--sky-time-slicing", "off"]
    rows, failures, metrics = [], [], {}

    def add(name, model, flags, paths=()):
        png, data, ok = review.capture(name, model, flags, extra_paths=paths)
        rows.append((name, png, data))
        if not ok:
            failures.append(name)
        if data:
            so = data.get("skyOcclusion") or {}
            ev = (data.get("night", {}).get("exposure", {}).get("viewports") or [{}])[0].get("exposureEv")
            metrics[name] = {"exposureEv": ev, "skyOcclusion": {k: so.get(k) for k in ("active", "volume", "bake", "casters",
                                                                                        "occluderCasters")}}

    cockpit = REAL_ASSETS["Cockpit"]
    if Path(cockpit).exists():
        windshield = list(view("cockpit_windshield_inside")["flags"])
        for mode in ("off", "on"):
            add(f"e195_cockpit_noon_{mode}", cockpit, windshield + NOON + ["--sky-occlusion", mode], E195_SEARCH_PATHS)
    if night.DA62_COCKPIT.exists():
        for mode in ("off", "on"):
            add(f"da62_cockpit_noon_{mode}", night.DA62_COCKPIT, night.DA62_CAMERA + NOON + ["--sky-occlusion", mode])
    exterior = REAL_ASSETS["Exterior"]
    if Path(exterior).exists():
        model = without_ground_equipment(exterior, OUT / "e195_no_gse.gltf")
        for mode in ("off", "on"):
            add(f"e195_exterior_noon_{mode}", model, NOON + ["--ground", "plane"] + review.E195_CAMERA + ["--sky-occlusion", mode])
        for mode in ("off", "on"):
            add(f"e195_gear_noon_{mode}", model, NOON + ["--ground", "plane"] + GEAR_VIEW + ["--sky-occlusion", mode])
    if Path(cockpit).exists():
        sunset_h, _ = night.sunset_times()
        add("e195_cockpit_sunset_on", cockpit, list(view("cockpit_windshield_inside")["flags"]) +
            ["--time", night.hhmm(sunset_h), "--time-of-day", "auto"], E195_SEARCH_PATHS)
        add("e195_cockpit_skyvisibility", cockpit, list(view("cockpit_windshield_inside")["flags"]) + NOON +
            ["--debug-view", "skyvisibility"], E195_SEARCH_PATHS)
    if Path(exterior).exists():
        add("e195_gear_skyvisibility", OUT / "e195_no_gse.gltf", NOON + ["--ground", "plane"] + GEAR_VIEW +
            ["--debug-view", "skyvisibility"])
    metrics["da62_portals"] = portal_metrics()
    (OUT / "metrics.json").write_text(json.dumps(metrics, indent=1), encoding="utf-8")
    review.contact_sheet(rows)
    from PIL import Image, ImageDraw, ImageFont
    path = OUT / "contact_sheet.png"
    sheet = Image.open(path)
    draw = ImageDraw.Draw(sheet)
    draw.rectangle((0, 0, sheet.width, 45), fill=(24, 26, 30))
    draw.text((12, 12), "A3.9: sky occlusion off / on (procedural sky default, medium quality); Seattle, Auto exposure",
              font=ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 17), fill="white")
    sheet.save(path)
    print("RESULT: " + ("FAIL " + ", ".join(failures) if failures else "OK"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
