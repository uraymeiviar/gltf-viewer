# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Wireframe Overlay: lines over an intact shaded image, and nothing else changes.

The bug this guards (docs/RENDERER_OPTIMIZATION_PHASE_4.md, 12.37): the overlay pass redrew createScene's transmission
set-up for scenes with transmissive materials, the duplicate Opaque2/Grid2/Transparent2 drawables and the
TransmissiveBlit pass, whose post-command clears the whole framebuffer. The shaded image was wiped and only lines
remained. The fix leaves those drawables out of the overlay scene.

Viewport captures, so panels and toolbar layout do not matter; AA none and TAA; no frozen pre-fix binary. Each answer is
computed from the same view with the overlay off, or compared with the regression baseline (--baseline-exe):
  * every pixel the overlay changes lies on a projected triangle edge of the fixture (opaque, blend and transmissive
    quads, from in front and through the transmissive quad), and it draws each outer edge, so the image is not wiped;
  * at opacity 1 every changed pixel is exactly the line colour; at opacity 0.5 it is the same pixel of the overlay-off
    image blended 1 to 8 times (coincident edges), and the same pixels change;
  * the same two answers with the skybox hidden, where the clear also wiped the image;
  * the default opacity is 0.5 (--wireframe-overlay equals --wireframe-color 1,1,1,0.5);
  * Wireframe mode ignores the overlay and its opacity; it draws only the line colour on the clear colour;
  * overlay off, Wireframe mode and the overlay at an explicit opaque colour are byte-identical to the baseline. The
    explicit colour keeps this comparison valid across the default's change; the default is the baseline's on a clean tree;
  * the Inspector's own checkboxes, by UI probe: overlay on, background off, overlay off equal the same state from flags,
    so switching the overlay leaves nothing behind; zero GL errors.
Self-test: a wiped image, a stray pixel, a wrong line colour and a wrong opacity, made from the real images, must be refused.

--broken-exe PATH runs the wipe detector on a binary known to have the bug (build/wireframe_fix/before/Glider.exe, from
before the fix) and passes only if the detector reports it. That binary is compared only with itself, overlay off against
on, so later renderer changes do not matter: it proves the detector.

Run: python scripts/validate_wireframe_overlay.py [--baseline-exe build/baselines/<sha>.exe] [--broken-exe <exe>]
"""

import argparse
import base64
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from glider_harness import baseline_exe  # noqa: E402
from PIL import Image  # noqa: E402
from validate_asobo_common import COMMON, EXE, REPO, check, finish, render, same  # noqa: E402
from validate_sun import Camera  # noqa: E402

OUT = REPO / "build" / "wireframe_overlay"
FIXTURE = REPO / "resources/test_models/production_passes_fixture.gltf"
FOX = REPO / "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb"
DUCK = REPO / "OpenGL-Renderer/Tools/tools/operations/tests/resources/Duck/glTF-Binary/Duck.glb"
FRONT = ["--camera-distance", "4", "--camera-yaw", "25", "--camera-pitch", "15"]
BEHIND = ["--camera-distance", "4", "--camera-yaw", "205", "--camera-pitch", "15"]  # the transmissive quad is nearest
# name, model, camera, the fixture meshes whose four outer edges are in full view (None: not the fixture)
CASES = (("fixture_front", FIXTURE, FRONT, (0,)),
         ("fixture_behind", FIXTURE, BEHIND, (0, 1, 2)),
         ("fox", FOX, FRONT, None),
         ("duck", DUCK, FRONT, None))
NO_GRID = ["--hide-grid"]  # the grid blends over the lines, so their exact colours are read without it
NO_SKY = ["--set", "background=off"]
OVERLAY = ["--wireframe-overlay"]
LINE = (255, 0, 255)
OPAQUE = ["--wireframe-color", "1,0,1,1"]
HALF = ["--wireframe-color", "1,0,1,0.5"]
WIREFRAME = ["--render-mode", "wireframe"]
EDGE_REACH = 1.5  # pixels from a projected edge; a one pixel line rasterizes within it
TOLERANCE = 3  # 8-bit levels: each blend layer rounds
MAX_LAYERS = 8  # coincident edges blend once each; the shared diagonal and a back and front face add up


def image(path):
    return Image.open(path).convert("RGB")


def changed_pixels(off, on):
    width = off.width
    a, b = off.tobytes(), on.tobytes()
    return [((i // 3) % width, (i // 3) // width) for i in range(0, len(a), 3) if a[i:i + 3] != b[i:i + 3]]


def wrong_at_opaque(off, on):
    """Pixels that changed to anything but the line colour."""
    return [p for p in changed_pixels(off, on) if on.getpixel(p) != LINE]


def layers(under, got, alpha=0.5):
    """How many blends of the line colour over `under` give `got`, or None."""
    for n in range(1, MAX_LAYERS + 1):
        keep = (1.0 - alpha) ** n
        if all(abs(g - (u * keep + c * (1.0 - keep))) <= TOLERANCE for g, u, c in zip(got, under, LINE)):
            return n
    return None


def wrong_at_half(off, on):
    """(pixels that are no blend of the line colour over the same pixel of `off`, pixels blended once)."""
    wrong, once = [], 0
    for p in changed_pixels(off, on):
        n = layers(off.getpixel(p), on.getpixel(p))
        if n is None:
            wrong.append(p)
        once += n == 1
    return wrong, once


def fixture_edges():
    """{mesh index: triangle edges}, and the outer edges of each quad. The fixture's nodes have no transforms."""
    gltf = json.loads(FIXTURE.read_text(encoding="utf-8"))
    blob = base64.b64decode(gltf["buffers"][0]["uri"].split(",", 1)[1])

    def read(index):
        accessor = gltf["accessors"][index]
        view = gltf["bufferViews"][accessor["bufferView"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        if accessor["type"] == "SCALAR":
            return struct.unpack_from(f"<{accessor['count']}H", blob, start)
        values = struct.unpack_from(f"<{accessor['count'] * 3}f", blob, start)
        return [values[i:i + 3] for i in range(0, len(values), 3)]

    all_edges, outer = {}, {}
    for number, mesh in enumerate(gltf["meshes"]):
        primitive = mesh["primitives"][0]
        positions, indices = read(primitive["attributes"]["POSITION"]), read(primitive["indices"])
        count = {}
        for t in range(0, len(indices), 3):
            for a, b in ((0, 1), (1, 2), (2, 0)):
                key = tuple(sorted((indices[t + a], indices[t + b])))
                count[key] = count.get(key, 0) + 1
        all_edges[number] = [(positions[a], positions[b]) for a, b in count]
        outer[number] = [(positions[a], positions[b]) for (a, b), seen in count.items() if seen == 1]
    return all_edges, outer


def segment_distance(point, a, b):
    ax, ay, bx, by = *a, *b
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((point[0] - ax) * dx + (point[1] - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(point[0] - ax - t * dx, point[1] - ay - t * dy)


def off_the_edges(changed, camera, edges):
    """Changed pixels further than EDGE_REACH from every projected triangle edge."""
    projected = [(camera.project(a), camera.project(b)) for mesh in edges.values() for a, b in mesh]
    return [p for p in changed if not any(segment_distance(p, a, b) <= EDGE_REACH for a, b in projected)]


def missing_edge_points(changed, camera, edges):
    """Points along an outer edge with no changed pixel near them; the line is absent there."""
    near = set(changed)
    missing = 0
    for a, b in edges:
        for step in range(1, 16):
            t = step / 16.0
            x, y = camera.project(tuple(a[i] + (b[i] - a[i]) * t for i in range(3)))
            cx, cy = int(round(x)), int(round(y))
            missing += not any((cx + dx, cy + dy) in near for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2))
    return missing


def wipe_report(off, on, camera, edges):
    """The share of the image the overlay changed and the changed pixels off the fixture's edges."""
    changed = changed_pixels(off, on)
    return len(changed) / (off.width * off.height), len(off_the_edges(changed, camera, edges))


def half_is_right(off, on):
    wrong, once = wrong_at_half(off, on)
    return once > 0 and not wrong


def self_test(off, opaque, half, camera, edges):
    """The detectors refuse damage made from this run's own images."""
    print("Self-test: damaged images are refused")
    changed = changed_pixels(off, opaque)
    check(half_is_right(off, half) and not wrong_at_opaque(off, opaque) and wipe_report(off, opaque, camera, edges)[1] == 0,
          "the undamaged images are accepted")
    xs, ys = [p[0] for p in changed], [p[1] for p in changed]
    middle = ((min(xs) + max(xs)) // 2 + 40, (min(ys) + max(ys)) // 2 - 80)
    wiped = opaque.copy()
    wiped.paste((90, 110, 130), (middle[0] - 25, middle[1] - 25, middle[0] + 25, middle[1] + 25))
    share, stray = wipe_report(off, wiped, camera, edges)
    check(stray > 1000 and share < 0.02, f"a small patch of the shaded image replaced: {stray} pixels off the edges, though only {share:.2%} changed")
    stray_pixel = opaque.copy()
    stray_pixel.putpixel((5, 5), LINE)
    check(wipe_report(off, stray_pixel, camera, edges)[1] == 1, "one line pixel away from every edge: off the edges")
    recoloured = opaque.copy()
    recoloured.putpixel(changed[0], (0, 255, 0))
    check(len(wrong_at_opaque(off, recoloured)) == 1, "one line pixel of another colour: wrong at opacity 1")
    check(not half_is_right(off, opaque), "opaque lines where opacity 0.5 was asked: none is blended once")
    quarter = off.copy()
    for p in changed:
        quarter.putpixel(p, tuple(round(u + (c - u) * 0.25) for u, c in zip(off.getpixel(p), LINE)))
    check(not half_is_right(off, quarter), "lines at opacity 0.25: no blend at 0.5")
    outer = [edge for mesh in edges.values() for edge in mesh]
    check(missing_edge_points([], camera, outer) > 0, "no lines at all: the edges are missing")


def ui_run(exe, model, flags, lines, name):
    """Runs a UI script (its `capture` lines write viewport images); returns its report."""
    script, report = OUT / f"{name}.txt", OUT / f"{name}.json"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    report.unlink(missing_ok=True)
    subprocess.run([str(exe), str(model)] + COMMON + list(flags) + ["--ui-script", str(script), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, text=True, timeout=300)
    return json.loads(report.read_text(encoding="utf-8")) if report.exists() else {"failed": True, "errors": ["no report"]}


def check_case(name, model, camera_flags, full_view, baseline, all_edges, outer):
    for aa in ("none", "taa"):
        tag = f"{name}_{aa}"
        print(tag)
        look = camera_flags + NO_GRID + ["--aa", aa]
        shot = {}
        for key, flags in (("off", []), ("opaque", OVERLAY + OPAQUE), ("half", OVERLAY + HALF),
                           ("sky_off", NO_SKY), ("sky_opaque", NO_SKY + OVERLAY + OPAQUE), ("sky_half", NO_SKY + OVERLAY + HALF)):
            shot[key] = render(EXE, model, f"{tag}_{key}", look + flags, OUT)
        if not check(all(shot.values()), f"{tag}: captured"):
            continue
        for key, flags in (("off", []), ("opaque", OVERLAY + OPAQUE), ("sky_off", NO_SKY), ("sky_opaque", NO_SKY + OVERLAY + OPAQUE)):
            theirs = render(baseline, model, f"{tag}_{key}_baseline", look + flags, OUT)
            check(theirs is not None and same(shot[key], theirs), f"{tag}: {key} byte-identical to the baseline")
        camera = Camera(json.loads((OUT / f"{tag}_off.json").read_text(encoding="utf-8")))
        for off_key, opaque_key, half_key, what in (("off", "opaque", "half", "over the skybox"),
                                                    ("sky_off", "sky_opaque", "sky_half", "skybox hidden")):
            off, opaque, half = image(shot[off_key]), image(shot[opaque_key]), image(shot[half_key])
            changed = changed_pixels(off, opaque)
            wrong = wrong_at_opaque(off, opaque)
            check(len(changed) > 0 and not wrong, f"{tag}, {what}: opacity 1, {len(changed)} pixels change, all to the line colour ({len(wrong)} wrong)")
            wrong, once = wrong_at_half(off, half)
            check(half_is_right(off, half), f"{tag}, {what}: opacity 0.5, each is a blend over the shaded pixel ({len(wrong)} wrong, {once} blended once)")
            check(set(changed_pixels(off, half)) == set(changed), f"{tag}, {what}: opacity changes no pixel's coverage")
            if full_view is None:
                continue
            share, stray = wipe_report(off, opaque, camera, all_edges)
            check(0.0 < share < 0.02 and stray == 0, f"{tag}, {what}: only the lines change, {share:.2%} of the image, {stray} pixels off the edges")
            for mesh in full_view:
                missing = missing_edge_points(changed, camera, outer[mesh])
                check(missing == 0, f"{tag}, {what}: mesh {mesh}'s outer edges are drawn ({missing} of 60 points missing)")
            if name == "fixture_front" and aa == "none" and off_key == "off":
                self_test(off, opaque, half, camera, all_edges)

    print(f"{name}: Wireframe mode")
    look = camera_flags + ["--aa", "none"]
    wire = render(EXE, model, f"{name}_wire", look + WIREFRAME, OUT)
    theirs = render(baseline, model, f"{name}_wire_baseline", look + WIREFRAME, OUT)
    check(wire is not None and theirs is not None and same(wire, theirs), f"{name}: Wireframe mode at the default colour byte-identical to the baseline")
    half = render(EXE, model, f"{name}_wire_half", look + WIREFRAME + ["--wireframe-color", "1,1,1,0.5"], OUT)
    check(same(wire, half), f"{name}: Wireframe mode draws opaque, the opacity is the overlay's")
    flagged = render(EXE, model, f"{name}_wire_flagged", look + WIREFRAME + OVERLAY, OUT)
    check(same(wire, flagged), f"{name}: Wireframe mode ignores the overlay switch")
    clear = render(EXE, model, f"{name}_wire_clear", look + NO_GRID + WIREFRAME + OPAQUE + ["--clear-color", "0,0,0"], OUT)
    colours = set(image(clear).getdata()) if clear else set()
    check(LINE in colours and colours <= {LINE, (0, 0, 0)}, f"{name}: Wireframe mode draws only the line colour on the clear colour ({sorted(colours)[:4]})")

    plain = render(EXE, model, f"{name}_grid_off", look, OUT)
    lined = render(EXE, model, f"{name}_grid_opaque", look + OVERLAY + OPAQUE, OUT)
    check(same(plain, render(baseline, model, f"{name}_grid_off_baseline", look, OUT)), f"{name}: with the grid, overlay off byte-identical to the baseline")
    check(same(lined, render(baseline, model, f"{name}_grid_opaque_baseline", look + OVERLAY + OPAQUE, OUT)), f"{name}: with the grid, overlay byte-identical to the baseline")
    if full_view is not None:
        camera = Camera(json.loads((OUT / f"{name}_grid_off.json").read_text(encoding="utf-8")))
        share, stray = wipe_report(image(plain), image(lined), camera, all_edges)
        check(0.0 < share < 0.02 and stray == 0, f"{name}: with the grid, only the lines change, {share:.2%}, {stray} pixels off the edges")


def check_live():
    """The Inspector's checkboxes and a run from flags reach the same state, in either order."""
    print("Inspector route (UI probes)")
    look = FRONT + NO_GRID
    shots = {key: OUT / f"live_{key}.png" for key in ("off", "overlay", "overlay_nosky", "nosky")}
    for path in shots.values():
        path.unlink(missing_ok=True)
    lines = ["wait 24", f"capture {shots['off'].as_posix()}",
             "clickp hier.viewport 1", "wait 6", "clickp vp.wireframe_overlay 0", "wait 8", f"capture {shots['overlay'].as_posix()}",
             "clickp hier.scene 0", "wait 6", "clickp scene.background 0", "wait 8", f"capture {shots['overlay_nosky'].as_posix()}",
             "clickp hier.viewport 1", "wait 6", "clickp vp.wireframe_overlay 0", "wait 8", f"capture {shots['nosky'].as_posix()}",
             "snap end", "wait 2"]
    report = ui_run(EXE, FIXTURE, look, lines, "live")
    check(not report["failed"], f"the probes respond ({report['errors']})")
    end = next((snap for snap in report.get("snaps", []) if snap["label"] == "end"), {})
    check(end.get("glErrors") == 0 and end.get("idConflict") == 0, f"zero GL errors, no ImGui ID conflicts ({end.get('glErrors')}, {end.get('idConflict')})")
    flags = {"off": [], "overlay": OVERLAY, "overlay_nosky": OVERLAY + NO_SKY, "nosky": NO_SKY}
    for key, extra in flags.items():
        ours = render(EXE, FIXTURE, f"live_flags_{key}", look + extra, OUT)
        check(same(shots[key], ours), f"{key}: the Inspector equals the flags")


def check_defaults():
    print("Default opacity")
    look = FRONT + NO_GRID
    default = render(EXE, FIXTURE, "default_overlay", look + OVERLAY, OUT)
    half = render(EXE, FIXTURE, "default_half", look + OVERLAY + ["--wireframe-color", "1,1,1,0.5"], OUT)
    full = render(EXE, FIXTURE, "default_full", look + OVERLAY + ["--wireframe-color", "1,1,1,1"], OUT)
    check(same(default, half) and not same(default, full), "the overlay's default colour is white at opacity 0.5")


def check_broken(exe, all_edges):
    """The wipe detector on a binary with the bug. Its UI script has vpsetting but no capture, so the flag captures."""
    print(f"Negative control: {exe}")
    images = {}
    for key, lines in (("off", ["wait 60"]), ("overlay", ["wait 8", "vpsetting 1 wireframe on", "wait 60"])):
        path, profile, script = OUT / f"broken_{key}.png", OUT / f"broken_{key}.json", OUT / f"broken_{key}.txt"
        path.unlink(missing_ok=True)
        script.write_text("\n".join(lines) + "\n", encoding="utf-8")
        subprocess.run([str(exe), str(FIXTURE)] + COMMON + FRONT + ["--ui-script", str(script), "--ui-report", str(script.with_suffix(".report.json")), "--capture-viewport", str(path),
                                                                    "--capture-frame", "40", "--profile-report", str(profile), "--exit-after-profile"],
                       cwd=REPO, capture_output=True, text=True, timeout=300)
        images[key] = path
    if not check(all(path.exists() for path in images.values()), "the broken binary captured both images"):
        return
    # Its report has no camera: the same flags on this build (fixture_front's capture) give the same view of the fixture.
    camera = Camera(json.loads((OUT / "fixture_front_grid_off.json").read_text(encoding="utf-8")))
    check(image(images["off"]).size == (camera.width, camera.height), "the same viewport size as this build's camera")
    share, stray = wipe_report(image(images["off"]), image(images["overlay"]), camera, all_edges)
    check(stray > 1000 and share >= 0.02, f"the detector reports the wipe: {share:.1%} of the image changed, {stray} pixels off the edges")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe")
    parser.add_argument("--broken-exe", help="a binary with the wipe bug; the detector must report it")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    baseline = baseline_exe(args.baseline_exe)
    print(f"Baseline: {baseline}")
    if not check(baseline is not None and Path(baseline).exists(), "a baseline exists (python scripts/save_baseline.py)"):
        return finish()
    all_edges, outer = fixture_edges()
    for name, model, camera_flags, full_view in CASES:
        check_case(name, model, camera_flags, full_view, baseline, all_edges, outer)
    check_defaults()
    check_live()
    if args.broken_exe:
        check_broken(REPO / args.broken_exe, all_edges)
    return finish()


if __name__ == "__main__":
    main()
