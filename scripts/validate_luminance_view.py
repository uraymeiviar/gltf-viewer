# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""The Luminance debug view (header/imgui/LuminanceView.h, docs/ARCHITECTURE.md).

Known answers computed here, not read back from Glider: a fixture of emissive-only quads (black base colour, zero
specular, ambient and sun off, black clear colour) looked at along -Z, each emitting a chosen luminance in cd/m^2
(emissiveFactor 1 x KHR_materials_emissive_strength L / 12,800: 12,800 cd/m^2 per environment unit), below, inside and
above the default Absolute scale (0.01 .. 100,000 cd/m^2), at band centres and near band edges. Checked:
  * Absolute, bands on: each quad's colour is the Turbo colour (the published polynomial, computed here) of its
    decade's centre; black below the minimum (and the black background), magenta above the maximum;
  * bands off: the continuous Turbo colour of each quad;
  * edited ends (luminance_min / luminance_max) move the bands;
  * Exposure-relative with a fixed manual EV: stops from middle grey at that exposure, black / magenta outside;
  * Emissive only on a lit scene (grey base colour, IBL, sun, background) equals the emission alone, background black;
  * MSAA and TAA map the resolved value (same quad colours);
  * the hovered value readout (a real --ui-script-follow mouse move) equals the expected cd/m^2 within 1%, and the EV;
  * bloom on / off give the same Luminance image; the pipe (set_viewport_setting) renders exactly like the flags;
    refused values; zero GL errors;
  * Shaded + post effects (and another debug view) byte-identical to the baseline binary (--baseline-exe, default the
    regression baseline build/baselines/<HEAD>.exe).

Run: python scripts/validate_luminance_view.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import atexit
import base64
import json
import math
import os
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
os.environ.setdefault("GLIDER_NO_FOCUS", "1")
os.environ.setdefault("GLIDER_MONITOR", "left")
import harness_process as subprocess  # noqa: E402
from glider_harness import start_pipe_session  # noqa: E402
from PIL import Image  # noqa: E402
from validate_asobo_common import COMMON, EXE, REPO, check, finish, render, same  # noqa: E402
from validate_sun import Camera  # noqa: E402

OUT = REPO / "build" / "luminance_view"
CD_PER_UNIT = 12800.0
TOLERANCE = 2  # 8-bit levels
DARK = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--ambient", "off",
        "--sun", "off", "--camera-target", "6,1.5,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "16"]
LIT = ["--hide-grid", "--auto-exposure", "off",
       "--camera-target", "6,1.5,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "16"]
LUMINANCE = ["--debug-view", "luminance"]
# Emitted luminance of each quad (cd/m^2): below the minimum, band 0 near both edges and its centre, band 1, band 2,
# band 4, band 6 at its centre and near the maximum, above the maximum.
VALUES = [0.005, 0.012, 0.0316, 0.09, 0.5, 3.0, 300.0, 30000.0, 95000.0, 200000.0]
COLUMNS = 5


def turbo(x):
    """Turbo (Mikhailov 2019, Apache-2.0), the published polynomial approximation (Ruofei Du), clamped."""
    x = min(max(x, 0.0), 1.0)
    v = (1.0, x, x * x, x ** 3, x ** 4, x ** 5)
    rows = ((0.13572138, 4.61539260, -42.66032258, 132.13108234, -152.94239396, 59.28637943),
            (0.09140261, 2.19418839, 4.84296658, -14.18503333, 4.27729857, 2.82956604),
            (0.10667330, 12.64194608, -60.58204836, 110.36276771, -89.90310912, 27.34824973))
    return tuple(min(max(sum(a * b for a, b in zip(row, v)), 0.0), 1.0) for row in rows)


def expected_colour(value, low, high, bands):
    """The 8-bit colour of a value in the scale's unit (log10 cd/m^2 or EV); None for 'zero' (black)."""
    if value is None:
        return (0, 0, 0)
    span = high - low
    t = (value - low) / span
    if t < 0.0:
        return (0, 0, 0)
    if t > 1.0:
        return (255, 0, 255)
    if bands:
        count = max(math.ceil(span - 1e-4), 1)
        k = min(math.floor(value - low), count - 1)
        t = min((k + 0.5) / span, 1.0)
    return tuple(round(255.0 * c) for c in turbo(t))


def absolute(cd, low=-2.0, high=5.0, bands=True):
    return expected_colour(math.log10(cd) if cd > 0 else None, low, high, bands)


def relative(cd, ev, low=-6.0, high=6.0, bands=True):
    y = cd / CD_PER_UNIT * (2.0 ** ev) / 0.18
    return expected_colour(math.log2(y) if y > 0 else None, low, high, bands)


def half_values(cd):
    """The luminances the RGBA16F scene target can hold for an emission of cd/m^2: below the smallest normal half
    float (about 0.78 cd/m^2 here) the stored value is a subnormal with steps of 2^-24 units, rounded either way."""
    units = cd / CD_PER_UNIT
    if units >= 2.0 ** -14:
        return [cd]
    step = 2.0 ** -24
    return [math.floor(units / step) * step * CD_PER_UNIT, math.ceil(units / step) * step * CD_PER_UNIT]


def centre(index):
    return ((index % COLUMNS) * 3.0, (index // COLUMNS) * 3.0, 0.0)


def build_fixture(path, lit):
    """Quads of 2 x 2 m in the XY plane, facing +Z; lit: grey base colour and the default specular."""
    gltf = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": []}], "nodes": [], "meshes": [],
            "materials": [], "accessors": [], "bufferViews": [], "buffers": [],
            "extensionsUsed": ["KHR_materials_emissive_strength"] + ([] if lit else ["KHR_materials_specular"])}
    blob = bytearray()
    positions = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    normals = [(0, 0, 1)] * 4
    for rows, kind in ((positions, "VEC3"), (normals, "VEC3")):
        flat = [c for row in rows for c in row]
        gltf["bufferViews"].append({"buffer": 0, "byteOffset": len(blob), "byteLength": 4 * len(flat), "target": 34962})
        blob += struct.pack(f"<{len(flat)}f", *flat)
        accessor = {"bufferView": len(gltf["bufferViews"]) - 1, "componentType": 5126, "type": kind, "count": len(rows)}
        if rows is positions:
            accessor.update({"min": [-1, -1, 0], "max": [1, 1, 0]})
        gltf["accessors"].append(accessor)
    gltf["bufferViews"].append({"buffer": 0, "byteOffset": len(blob), "byteLength": 12, "target": 34963})
    blob += struct.pack("<6H", 0, 1, 2, 0, 2, 3)
    gltf["accessors"].append({"bufferView": 2, "componentType": 5123, "type": "SCALAR", "count": 6})
    for index, cd in enumerate(VALUES):
        material = {"name": f"emit_{cd:g}", "doubleSided": True,
                    "pbrMetallicRoughness": {"baseColorFactor": [0.6, 0.6, 0.6, 1.0] if lit else [0.0, 0.0, 0.0, 1.0],
                                             "metallicFactor": 0.0, "roughnessFactor": 0.5 if lit else 1.0},
                    "emissiveFactor": [1.0, 1.0, 1.0],
                    "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": cd / CD_PER_UNIT}}}
        if not lit:
            material["extensions"]["KHR_materials_specular"] = {"specularFactor": 0.0}
        gltf["materials"].append(material)
        gltf["meshes"].append({"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": index}]})
        gltf["nodes"].append({"name": f"quad_{index}", "mesh": index, "translation": list(centre(index))})
        gltf["scenes"][0]["nodes"].append(index)
    gltf["buffers"].append({"byteLength": len(blob),
                            "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()})
    path.write_text(json.dumps(gltf), encoding="utf-8")
    return path


def capture(exe, model, name, flags):
    png = render(exe, model, name, list(flags), OUT)
    if png is None:
        return None, None
    report = json.loads((OUT / f"{name}.json").read_text(encoding="utf-8"))
    return png, Camera(report)


def pixel(image, camera, point):
    x, y = camera.project(point)
    return image.getpixel((int(round(x)), int(round(y))))[:3]


def compare(image, camera, point, expected, label):
    got = pixel(image, camera, point)
    ok = all(abs(g - e) <= TOLERANCE for g, e in zip(got, expected))
    return check(ok, f"{label}: {tuple(expected)} (got {got})")


def check_quads(png, camera, expected_of, label):
    if not check(png is not None, f"{label}: captured"):
        return
    image = Image.open(png).convert("RGB")
    for index, cd in enumerate(VALUES):
        # The expected colour of the emitted value, or of a neighbouring value the half-float target can hold.
        candidates = [expected_of(value) for value in [cd] + half_values(cd)]
        got = pixel(image, camera, centre(index))
        ok = any(all(abs(g - e) <= TOLERANCE for g, e in zip(got, want)) for want in candidates)
        check(ok, f"{label}, {cd:g} cd/m2: {candidates[0]}" + (f" or {candidates[1:]} (half float)" if len(candidates) > 2 else "")
              + f" (got {got})")
    compare(image, camera, (6.0, -2.5, 0.0), (0, 0, 0), f"{label}, background (zero) is black")


def max_difference(a, b):
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if ia.size != ib.size:
        return 999
    return max(max(abs(x - y) for x, y in zip(pa, pb)) for pa, pb in zip(ia.getdata(), ib.getdata()))


class Session:
    """A --control-pipe Glider; killed whenever it fails or the script exits."""

    def __init__(self, model, flags):
        self.process, self.pipe = start_pipe_session([EXE, model] + COMMON + list(flags) + ["--control-pipe"])
        atexit.register(self.kill)

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def set(self, key, value):
        return self.call("set_viewport_setting", key=key, value=value)

    def capture(self, name):
        path = OUT / f"{name}.png"
        path.unlink(missing_ok=True)
        self.call("capture", path=str(path), settle_frames=8, overlays=False)
        return path

    def close(self):
        errors = self.call("get_state").get("glErrors")
        try:
            self.call("quit")
        except Exception:  # noqa: BLE001 - the pipe closes as Glider exits
            pass
        try:
            self.pipe.close()
        except Exception:  # noqa: BLE001
            pass
        try:
            self.process.wait(60)
        except Exception:  # noqa: BLE001
            self.kill()
        return errors

    def kill(self):
        if self.process.poll() is None:
            self.process.kill()


def follow_session(model, flags, steps, timeout=240):
    """Runs Glider with --ui-script-follow: steps(send) drives it, send(line) appends a line and returns its JSONL
    result (None for the harness's own commands, wait and move, which write none: the next registry command's reply
    comes after them). Glider is killed when the steps fail or time out."""
    script = OUT / "hover_script.txt"
    results = Path(str(script) + ".out.jsonl")
    script.write_text("", encoding="utf-8")
    results.unlink(missing_ok=True)
    process = subprocess.Popen([str(EXE), str(model)] + COMMON + list(flags) + ["--ui-script-follow", str(script)], cwd=REPO,
                               stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.time() + timeout
    lines = [0]

    def send(line):
        with script.open("a", encoding="utf-8") as f:
            f.write(line + "\n")
        lines[0] += 1
        if line.split()[0] in ("wait", "move"):
            return None
        while time.time() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"Glider exited ({process.returncode}) before answering {line!r}")
            if results.exists():
                for row in results.read_text(encoding="utf-8").splitlines():
                    try:
                        entry = json.loads(row)
                    except ValueError:
                        continue
                    if entry.get("line") == lines[0]:
                        return entry
            time.sleep(0.1)
        raise RuntimeError(f"no answer to {line!r} in time")

    try:
        steps(send)
        with script.open("a", encoding="utf-8") as f:
            f.write("quit\n")
        process.wait(max(deadline - time.time(), 5))
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(30)


def check_hover(model):
    print("Hovered value readout (--ui-script-follow mouse move)")
    index = VALUES.index(300.0)

    def steps(send):
        send("wait 20")
        state = send("get_state")["result"]
        camera_json = send("get_camera")["result"]
        vp = state["viewports"][0]
        camera = Camera({"camera": {"eye": camera_json["eye"], "target": camera_json["target"], "up": camera_json["up"],
                                    "fovY": camera_json["fovY"], "viewport": [vp["width"], vp["height"]]}})
        rect = vp["luminance"]["imageRect"]
        check(vp["luminance"]["active"] and vp["luminance"]["hoveredValue"] is None, "before a hover: no hovered value")
        px, py = camera.project(centre(index))
        x = rect[0] + (px + 0.5) * (rect[2] - rect[0]) / vp["width"]
        y = rect[1] + (py + 0.5) * (rect[3] - rect[1]) / vp["height"]
        send(f"move {x:.1f} {y:.1f}")
        send("wait 10")
        lum = send("get_state")["result"]["viewports"][0]["luminance"]
        value = lum["hoveredValue"]
        check(lum["hovered"] is True, "the cursor is over the image")
        if check(value is not None, "a hovered value was read back"):
            error = abs(value["cd"] - 300.0) / 300.0
            check(error <= 0.01, f"hovered value 300 cd/m2 within 1% (got {value['cd']:.6g}, {error * 100:.3f}%)")
        send("set_viewport_setting luminance_scale relative")
        send("wait 10")
        lum = send("get_state")["result"]["viewports"][0]["luminance"]
        value = lum["hoveredValue"]
        want = math.log2(300.0 / CD_PER_UNIT * 2.0 / 0.18)
        if check(value is not None and value["ev"] is not None, "relative: a hovered EV"):
            check(abs(value["ev"] - want) <= 0.015 and abs(value["cd"] - 300.0) <= 3.0,
                  f"relative at EV 1: {want:+.3f} EV (got {value['ev']:+.4f}), still {value['cd']:.4g} cd/m2")
        background = rect[0] + 0.5 * (rect[2] - rect[0]), rect[1] + 0.03 * (rect[3] - rect[1])
        send(f"move {background[0]:.1f} {background[1]:.1f}")
        send("wait 10")
        value = send("get_state")["result"]["viewports"][0]["luminance"]["hoveredValue"]
        check(value is not None and value["cd"] == 0 and value["ev"] is None, f"background reads 0 cd/m2 and no EV ({value})")
        check(send("get_state")["result"]["glErrors"] == 0, "hover session: zero GL errors")

    try:
        follow_session(model, DARK + LUMINANCE + ["--exposure", "1"], steps)
    except Exception as error:  # noqa: BLE001
        check(False, f"hover session: {error}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if args.baseline_exe:
        baseline = Path(args.baseline_exe)
        baseline = baseline if baseline.is_absolute() else REPO / baseline
    else:
        sha = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO, capture_output=True, text=True).stdout.strip()
        baseline = REPO / "build" / "baselines" / f"{sha}.exe"
    dark = build_fixture(OUT / "luminance_dark.gltf", lit=False)
    lit = build_fixture(OUT / "luminance_lit.gltf", lit=True)

    print("Absolute scale, CLI route")
    abs_png, camera = capture(EXE, dark, "absolute", DARK + LUMINANCE)
    check_quads(abs_png, camera, absolute, "absolute, bands")
    png, camera = capture(EXE, dark, "continuous", DARK + LUMINANCE + ["--luminance-bands", "off"])
    check_quads(png, camera, lambda cd: absolute(cd, bands=False), "absolute, continuous")
    png, camera = capture(EXE, dark, "ends", DARK + LUMINANCE + ["--luminance-max", "1000", "--luminance-min", "0.1"])
    check_quads(png, camera, lambda cd: absolute(cd, -1.0, 3.0), "absolute 0.1 .. 1000")

    print("Exposure-relative at manual EV 1")
    rel_png, camera = capture(EXE, dark, "relative", DARK + LUMINANCE + ["--luminance-scale", "relative", "--exposure", "1"])
    check_quads(rel_png, camera, lambda cd: relative(cd, 1.0), "relative, EV 1")
    png, camera = capture(EXE, dark, "relative_ends", DARK + LUMINANCE + ["--luminance-scale", "relative", "--exposure", "1",
                                                                       "--luminance-ev-min", "-3", "--luminance-ev-max", "3",
                                                                       "--luminance-bands", "off"])
    check_quads(png, camera, lambda cd: relative(cd, 1.0, -3.0, 3.0, False), "relative -3 .. +3 EV, continuous")

    print("Emissive only on a lit scene")
    shaded_lit, camera = capture(EXE, lit, "lit_luminance", LIT + LUMINANCE)
    emissive_lit, camera = capture(EXE, lit, "lit_emissive_only", LIT + LUMINANCE + ["--luminance-emissive-only", "on"])
    check_quads(emissive_lit, camera, absolute, "emissive only, lit scene")
    if emissive_lit and abs_png:
        diff = max_difference(emissive_lit, abs_png)
        check(diff <= 1, f"emissive only on the lit scene equals the emission-only scene (max difference {diff})")
    if shaded_lit and emissive_lit:
        image = Image.open(shaded_lit).convert("RGB")
        check(pixel(image, camera, centre(0)) != (0, 0, 0) and not same(shaded_lit, emissive_lit),
              "the lit scene's Luminance view shows the lighting (the 0.005 cd/m2 quad is lit, not black)")
        check(pixel(image, camera, (6.0, -2.5, 0.0)) != (0, 0, 0), "the lit scene's background has luminance")

    print("Anti-aliasing maps the resolved value")
    for name, flags in (("msaa4", ["--aa", "msaa4"]), ("taa", ["--aa", "taa"])):
        png, camera = capture(EXE, dark, f"aa_{name}", DARK + LUMINANCE + flags)
        check_quads(png, camera, absolute, f"absolute, {name}")

    print("Bloom does not change the Luminance view")
    bloom_on = list(DARK)
    bloom_on[bloom_on.index("--bloom") + 1] = "on"
    png, _ = capture(EXE, dark, "bloom_on", bloom_on + LUMINANCE)
    check(same(png, abs_png), "bloom on equals bloom off")
    png, _ = capture(EXE, lit, "lit_bloom_off", LIT + ["--bloom", "off"] + LUMINANCE)
    check(same(png, shaded_lit), "lit scene: bloom off equals the default (on)")

    print("Pipe route")
    session = Session(dark, DARK)
    try:
        session.set("render_mode", "luminance")
        check(same(session.capture("pipe_absolute"), abs_png), "set_viewport_setting render_mode luminance equals --debug-view luminance")
        session.call("set_environment", key="exposure", value=1)
        session.set("luminance_scale", "relative")
        check(same(session.capture("pipe_relative"), rel_png), "luminance_scale relative equals --luminance-scale relative")
        session.set("luminance_scale", "absolute")
        state = session.call("get_state")["viewports"][0]["luminance"]
        check(state["scale"] == "absolute" and state["min"] == 0.01 and state["max"] == 100000 and state["bands"] is True
              and state["emissiveOnly"] is False and state["evMin"] == -6 and state["evMax"] == 6,
              f"get_state reports the view's settings ({ {k: state[k] for k in ('scale', 'min', 'max', 'bands', 'emissiveOnly')} })")
        for key, value in (("luminance_min", -1), ("luminance_min", 0), ("luminance_min", 1e6), ("luminance_max", 0.001),
                           ("luminance_ev_max", -7), ("luminance_scale", "log"), ("luminance_bands", "maybe")):
            try:
                session.set(key, value)
                check(False, f"{key} {value} refused")
            except Exception:  # noqa: BLE001 - glider_ctl.GliderError
                check(True, f"{key} {value} refused")
        check(session.call("get_viewport_setting", key="luminance_min")["value"] == 0.01, "a refused value leaves the setting")
        session.set("render_mode", "shaded_post")
        state = session.call("get_state")["viewports"][0]["luminance"]
        check(state["active"] is False and state["hoveredValue"] is None, "leaving the view: inactive, no sample kept")
    finally:
        errors = session.close()
    check(errors == 0, f"pipe session: zero GL errors ({errors})")

    check_hover(dark)

    print("Every other view is unchanged")
    if baseline.exists():
        for name, model, flags in (("shaded_post", lit, LIT), ("shaded_post_dark", dark, DARK), ("emissive", lit, LIT + ["--debug-view", "emissive"])):
            ours, _ = capture(EXE, model, f"same_{name}", flags)
            theirs, _ = capture(baseline, model, f"baseline_{name}", flags)
            check(ours is not None and theirs is not None and same(ours, theirs), f"{name}: byte-identical to the baseline {baseline.name}")
    else:
        check(False, f"baseline {baseline} exists")
    return finish()


if __name__ == "__main__":
    main()
