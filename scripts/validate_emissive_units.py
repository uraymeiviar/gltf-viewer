# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.6b: sky ASOBO emissive in cd/m^2, unchanged HDR/core glTF, and explicit overrides.

Serial harness; uses probe/probe_region before exposure, plus real-asset HDR baseline captures.
Run: python scripts/validate_emissive_units.py --baseline-exe build/baselines/<sha>.exe
"""

import argparse
import json
from contextlib import contextmanager
from pathlib import Path

import validate_asobo_emissive as emissive
import validate_night as night
from validate_asobo_common import EXE, REAL_ASSETS, REPO, check, finish, same, view

OUT = REPO / "build" / "emissive_units"
VIEW = ["--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "2", "--hide-grid"]
DARK = ["--ambient", "off", "--bloom", "off",
        "--sun-dir", "0,-90", "--moon", "off", "--stars", "off", "--clear-color", "0,0,0"]


@contextmanager
def session(model, flags):
    s = night.Session(model, flags)
    try:
        yield s
    finally:
        if s.process.poll() is None:
            try:
                check(s.close() == 0, "control session: zero GL errors")
            finally:
                if s.process.poll() is None:
                    s.process.kill()
                    s.process.wait(60)


def fixture(name, asobo=True, strength=None, day=None, nocturnal=None, factor=5000):
    path = emissive.make_quad_fixture(name, [factor] * 3, asobo, day, nocturnal)
    doc = json.loads(path.read_text(encoding="utf-8"))
    # Black diffuse + zero specular: no illumination contaminates the emissive probes.
    doc.setdefault("extensionsUsed", []).append("KHR_materials_specular")
    doc["materials"][0].setdefault("extensions", {})["KHR_materials_specular"] = {"specularFactor": 0}
    if strength is not None:
        doc.setdefault("extensionsUsed", []).append("KHR_materials_emissive_strength")
        doc["materials"][0].setdefault("extensions", {})["KHR_materials_emissive_strength"] = {"emissiveStrength": strength}
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def measure(s, label, expected):
    s.capture(label, frames=12)
    state = s.call("get_state")
    vp = state["viewports"][0]
    x, y = vp["width"] // 2, vp["height"] // 2
    point = s.call("probe", x=x, y=y)
    region = s.call("probe_region", x0=x-4, y0=y-4, x1=x+4, y1=y+4)
    values = point["hdr"][:3] + region["rgb"]["mean"][:3]
    error = max(abs(v - expected) / max(abs(expected), 1e-8) for v in values)
    check(error <= 0.01 and point["pick"] is not None,
          f"{label}: probe + region = {values}, expected {expected:.8g}, error {error*100:.4f}% <= 1%")
    (OUT / f"{label}.probe.json").write_text(json.dumps({"state": state, "probe": point, "region": region}, indent=2), encoding="utf-8")
    return state


def check_units():
    print("[Pre-exposure radiance, source changes, explicit overrides]")
    model = fixture("asobo_5000")
    with session(model, VIEW + DARK + ["--environment", "sky"]) as s:
        st = measure(s, "sky_5000", 5000/12800)
        check(abs(st["msfsEmissive"]["effectiveScale"] - 1/12800) < 1e-10 and not st["msfsEmissive"]["overridden"],
              "get_state: sky default effective scale 1/12800, no override")
        got = s.call("get_environment", key="msfs_emissive_scale")["value"]
        check(abs(got - 1/12800) < 1e-10, "get_environment reports the effective sky scale")
        s.env("environment", "hdr")
        measure(s, "hdr_5000", 5)
        s.env("environment", "sky")
        measure(s, "sky_again", 5000/12800)
        # An explicit value equal to the HDR default must still override the sky.
        s.env("msfs_emissive_scale", 0.001)
        st = measure(s, "sky_explicit_hdr_default", 5)
        check(st["msfsEmissive"]["overridden"], "explicit HDR-default value is an override")
        for source in ("hdr", "sky"):
            s.env("environment", source)
            s.env("msfs_emissive_scale", 0.0002)
            measure(s, f"{source}_override", 1)
        s.env("msfs_emissive_scale", 0)
        measure(s, "sky_zero_override", 0)
    for mode in ("manual", "auto", "curve"):
        # A night Curve pre-exposure intentionally favors starlight and can overflow bright displays (A6.4b).
        # At noon it fits the target and still exercises pre-exposure removal in both probe APIs.
        with session(model, VIEW + DARK + ["--environment", "sky", "--exposure-mode", mode, "--sun-dir", "0,45"]) as s:
            measure(s, f"sky_preexposure_{mode}", 5000/12800)
    for switch in (["--msfs-emissive", "off"], ["--asobo", "off"]):
        with session(model, VIEW + DARK + ["--environment", "sky"] + switch) as s:
            measure(s, f"sky_disabled_{switch[0][2:]}", 1)
    model = fixture("asobo_strength", strength=2, day=0.5, nocturnal=0.25)
    with session(model, VIEW + DARK + ["--environment", "sky"]) as s:
        measure(s, "sky_strength_day", 5000/12800)
        s.env("time_of_day", "night")
        measure(s, "sky_strength_night", 2500/12800)


def check_baseline(baseline):
    print("[HDR baseline parity: HelloCube, E195 two views, DA62]")
    cases = [("hello", None, [])]
    for name in ("cockpit_main_displays", "exterior_fuselage_nose"):
        entry = view(name)
        model = REAL_ASSETS[entry["model"]]
        if Path(model).exists():
            cases.append((name, model, entry["flags"]))
        else:
            print(f"  SKIP {name}: asset not installed")
    if night.DA62_COCKPIT.exists():
        cases.append(("da62_cockpit", night.DA62_COCKPIT, night.DA62_CAMERA))
    for name, model, flags in cases:
        flags = list(flags) + ["--environment", "hdr"]
        now, _ = night.render(model, f"hdr_now_{name}", flags)
        ref, _ = night.render(model, f"hdr_ref_{name}", flags, exe=baseline)
        check(same(now, ref), f"HDR {name}: byte-identical to {baseline.name}")
    for strength in (None, 5000):
        model = fixture(f"core_{strength}", asobo=False, strength=strength, factor=1)
        for source in ("hdr", "sky"):
            flags = VIEW + DARK + ["--environment", source, "--sky-time-slicing", "off"]
            # A6.4c moved the highlight meter's target; pin the average meter on BOTH binaries for the parity
            # render so it isolates the emissive units (validate_night covers the meter). The probe session below
            # keeps the default meter.
            pin = ["--exposure-metering", "average"] if source == "sky" else []
            now, _ = night.render(model, f"core_now_{strength}_{source}", flags + pin)
            ref, _ = night.render(model, f"core_ref_{strength}_{source}", flags + pin, exe=baseline)
            check(same(now, ref), f"core glTF strength={strength}, {source}: byte-identical to baseline")
            with session(model, flags) as s:
                measure(s, f"core_probe_{strength}_{source}", strength or 1)


def check_cli():
    print("[Explicit CLI override = pipe, either flag order; bloom]")
    model = fixture("override_5000")
    for source in ("hdr", "sky"):
        for order in (0, 1):
            env = ["--environment", source]
            scale = ["--msfs-emissive-scale", "0.001"]
            flags = VIEW + DARK + (env + scale if order == 0 else scale + env)
            cli, _ = night.render(model, f"cli_{source}_{order}", flags)
            with session(model, VIEW + DARK) as s:
                s.env("msfs_emissive_scale", 0.001)
                s.env("environment", source)
                pipe = s.capture(f"pipe_{source}_{order}", frames=24)
                check(same(cli, pipe), f"explicit override CLI = pipe ({source}, order {order}), byte-identical")
                measure(s, f"override_probe_{source}_{order}", 5)
    with session(model, VIEW + DARK + ["--environment", "sky", "--exposure-mode", "manual", "--exposure", "5"]) as s:
        off = s.capture("bloom_off", frames=12)
        s.env("bloom", True)
        on = s.capture("bloom_on", frames=12)
        import numpy as np
        # The corrected 0.390625-unit display, exposed +5 EV, must still generate a halo outside its geometry.
        a, b = night.load(on), night.load(off)
        h, w = a.shape[:2]
        yy, xx = np.mgrid[:h, :w]
        outside = (abs(xx-w/2) > w*0.18) | (abs(yy-h/2) > h*0.3)
        delta = float((a-b)[outside].max())
        check(delta > 2/255, f"corrected emissive still blooms: halo increase {delta*255:.1f} levels > 2")


def check_inspector():
    print("[Inspector edit, source switch and right-click reset through UiProbe]")
    model = fixture("inspector_5000")
    script = OUT / "inspector.txt"
    script.write_text("\n".join([
        "wait 12", "clickp hier.scene 0", "wait 6", "get_state",
        "dclickp scene.msfs.emissive.scale 0", "wait 4", "key a ctrl", "type 0.002", "key enter", "wait 8", "get_state",
        "set_environment environment hdr", "wait 8", "get_state",
        "rclickp scene.msfs.emissive.scale 0", "wait 4", "clickp scene.msfs.emissive.default 0", "wait 8", "get_state",
        "set_environment environment sky", "wait 12", "get_state", "quit", ""
    ]), encoding="utf-8")
    result = night.run([EXE, model, "--isolated", "--environment", "sky", "--sky-time-slicing", "off",
                        "--window-size", "1920x1080", "--fixed-frame-time", "0.05", "--aa", "none", "--vsync", "off",
                        "--ui-script-follow", script])
    (OUT / "inspector.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    rows = [json.loads(line) for line in Path(str(script) + ".out.jsonl").read_text(encoding="utf-8").splitlines()]
    states = [r["result"]["msfsEmissive"] for r in rows if r["cmd"] == "get_state" and r["ok"]]
    expected = [(1/12800, False), (0.002, True), (0.002, True), (0.001, False), (1/12800, False)]
    check(result.returncode == 0 and all(r["ok"] for r in rows) and len(states) == len(expected),
          f"Inspector UI script completed: {len(states)} states, errors {[r for r in rows if not r['ok']]}")
    check(len(states) == len(expected) and all(abs(s["effectiveScale"] - scale) < 1e-9 and s["overridden"] == override
                                             for s, (scale, override) in zip(states, expected)),
          f"Inspector: default -> edit -> HDR override -> reset HDR -> sky default: {states}")
    check(all(r["result"]["glErrors"] == 0 for r in rows if r["cmd"] == "get_state" and r["ok"])
          and "GL error 0x" not in result.stdout + result.stderr, "Inspector: zero GL errors (get_state)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-exe", type=Path, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    night.OUT = emissive.EMIS_DIR = OUT
    check_units()
    check_baseline(args.baseline_exe.resolve())
    check_cli()
    check_inspector()
    check(all(night.gl_errors_zero(log) for _, log in night.gl_logs), "capture runs: zero GL errors")
    finish()


if __name__ == "__main__":
    main()
