# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.5: screen-space ambient occlusion of the sky light (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.5).

GTAO from the A2.1 prepass depth and normal, computed per viewport before the opaque pass (full or half resolution,
depth-aware denoise and upsample), applied in pbr.frag to the IBL diffuse / specular / sheen / clearcoat of OPAQUE
and MASK surfaces as min(authored occlusion, SSAO). The debug view `ssao` shows the raw visibility (8-bit level =
visibility x 255).

Fixtures (scripts/ssao_fixtures.py): an open box (floor and three walls), a 40 x 40 m plane with a cube floating
3 m above it, a green cube 0.6 m in front of a red wall. Unless stated: uniform
environment L = 0.5 (IBL only), black clear colour (no background), PBR Neutral, exposure 0, no bloom / auto
exposure / AA, radius 0.5 m, medium quality.
Checks:
  * off (the default): byte-identical to the baseline binary (--baseline-exe) on HelloCube, the box fixture and the
    E195 exterior_fuselage_nose and cockpit_main_displays views; --ssao off equals the default; SSAO on changes the
    box image;
  * box interior (full and half resolution, three qualities): the inner corner and the floor/wall creases are
    occluded (corner visibility < CORNER_MAX, crease < CREASE_MAX), the floor centre (1 m from the walls, beyond
    the radius) and the open plane are not (>= OPEN_MIN); the convex faces of the floating cube facing the sky
    keep >= OPEN_MIN; the shaded box darkens only where the debug view is below 1;
  * silhouettes: with a wall 0.6 m behind the cube (beyond the radius), no wall pixel farther than HALO_PX (full) /
    HALO_PX_HALF (half) from the cube's silhouette is darkened below HALO_LEVEL, and the cube's own pixels keep
    >= HALO_LEVEL; against the sky (default HDR background) the open box changes only on its own pixels;
  * temporal stability with TAA: frames 64 and 96 of a static view differ inside the box by at most
    TAA_MAX_LEVELS (mean TAA_MEAN_LEVELS) more than the same frames with SSAO off;
  * the sun term is unchanged: with a black environment (no IBL) and the sun on, SSAO on is byte-identical to off;
    with the uniform environment, (on - off) with the sun equals (on - off) without it (linear, within SUN_ABS);
  * registry keys (ssao, ssao_radius, ssao_intensity, ssao_quality, ssao_resolution), get_state ssao, the debug
    view name, --ssao-* flags, invalid values rejected, intensity 0 = off;
  * zero GL errors.

Every Glider process this script starts is killed if its session fails (no window is left open).

Run: python scripts/validate_ssao.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b,...]
"""

import argparse
import atexit
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import ssao_fixtures  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, view, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import Camera, load, untonemap, LUMA, gl_errors_zero  # noqa: E402

OUT = REPO / "build" / "ssao"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
LOOK = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral"]
BOX_VIEW = ["--camera-target", "0,0.7,0", "--camera-yaw", "20", "--camera-pitch", "25", "--camera-distance", "4.5"]
PLANE_VIEW = ["--camera-target", "0,1.5,0", "--camera-yaw", "30", "--camera-pitch", "35", "--camera-distance", "9"]
HALO_VIEW = ["--camera-target", "0,0,0", "--camera-yaw", "15", "--camera-pitch", "10", "--camera-distance", "4"]
SKY_VIEW = ["--camera-target", "0,0,0", "--camera-yaw", "30", "--camera-pitch", "-10", "--camera-distance", "3.5"]
SSAO = ["--ssao", "on", "--ssao-radius", "0.5"]

CORNER_MAX = 0.6        # visibility in the box's inner corner
CREASE_MAX = 0.8        # visibility at a floor/wall crease
OPEN_MIN = 0.99         # visibility on open surfaces (floor centre, open plane, convex faces)
OPEN_MEAN_MIN = 0.999   # mean visibility over the open plane
HALO_LEVEL = 0.97       # a pixel darker than this counts as darkened
HALO_PX = 1             # full resolution: darkened wall pixels at most this far from the cube's silhouette
HALO_PX_HALF = 2        # half resolution
TAA_MAX_LEVELS = 2      # extra frame-to-frame change (8-bit) inside the box with SSAO on, frames 64 / 96
TAA_MEAN_LEVELS = 0.05
SUN_ABS = 1e-4          # linear HDR probe: (on - off) with and without the sun

gl_logs = []
_processes = []


def _kill_all():
    for process in _processes:
        if process.poll() is None:
            process.kill()


atexit.register(_kill_all)


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, common=COMMON):
    OUT.mkdir(parents=True, exist_ok=True)
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + list(common) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"])
    if Path(exe) == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def visibility(png):
    """The debug view's raw visibility (red channel / 255)."""
    return load(png)[..., 0] / 255.0


def linear(png):
    import numpy as np
    return untonemap(load(png)) @ np.array(LUMA)


def at(img, cam, point):
    x, y = cam.project(point)
    return img[int(round(y)), int(round(x))]


def dilate(mask, steps):
    import numpy as np
    out = mask.copy()
    for _ in range(steps):
        grown = out.copy()
        grown[1:, :] |= out[:-1, :]
        grown[:-1, :] |= out[1:, :]
        grown[:, 1:] |= out[:, :-1]
        grown[:, :-1] |= out[:, 1:]
        grown[1:, 1:] |= out[:-1, :-1]
        grown[:-1, :-1] |= out[1:, 1:]
        grown[1:, :-1] |= out[:-1, 1:]
        grown[:-1, 1:] |= out[1:, :-1]
        out = grown
    return out


class Session:
    """A --control-pipe Glider; the process is killed whenever the session fails or the script exits."""

    def __init__(self, model, flags=()):
        args = [str(EXE)] + ([str(model)] if model else []) + COMMON + list(flags) + ["--control-pipe"]
        self.process = subprocess.Popen(args, cwd=REPO, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        _processes.append(self.process)
        self.pipe = None
        try:
            deadline = time.time() + 120
            while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
                if self.process.poll() is not None or time.time() > deadline:
                    raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
                time.sleep(0.1)
            while True:
                try:
                    self.pipe = glider_ctl.GliderPipe(self.process.pid)
                    break
                except glider_ctl.GliderError:
                    if self.process.poll() is not None or time.time() > deadline:
                        raise
                    time.sleep(0.2)
        except BaseException:
            self.kill()
            raise

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def settle(self, frames=6):
        self.call("capture", path=str(OUT / "_settle.png"), settle_frames=frames, overlays=False)

    def kill(self):
        if self.pipe is not None:
            try:
                self.pipe.close()
            except Exception:  # noqa: BLE001
                pass
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(30)

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
            self.pipe.close()
            self.pipe = None
            self.process.wait(60)
        finally:
            self.kill()
        return errors


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_baseline(baseline, uniform, box):
    print("[off: byte-identical to the baseline]")
    flags = uniform + LOOK + BOX_VIEW
    a, _ = render(box, "cur_box_default", flags)
    b, _ = render(box, "cur_box_off", flags + ["--ssao", "off"])
    c, _ = render(box, "cur_box_on", flags + SSAO)
    check(a is not None and same(a, b), "--ssao off equals the default")
    check(c is not None and not same(a, c), "--ssao on changes the box image")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary")
        return
    base_box, _ = render(box, "base_box", flags, exe=baseline)
    check(base_box is not None and same(base_box, a), f"box: default is byte-identical to {baseline.stem}")
    base_cube, _ = render(None, "base_hellocube", [], exe=baseline)
    cur_cube, _ = render(None, "cur_hellocube", [])
    check(base_cube is not None and same(base_cube, cur_cube), f"HelloCube: default is byte-identical to {baseline.stem}")
    for name in ("exterior_fuselage_nose", "cockpit_main_displays"):
        entry = view(name)
        a = render_view(baseline, entry, f"base_{name}", folder=OUT)
        b = render_view(EXE, entry, f"cur_{name}", folder=OUT)
        check(a is not None and same(a, b), f"{name}: default is byte-identical to {baseline.stem}")


def check_box(uniform, box, plane):
    import numpy as np
    print("[box interior, open plane, convex faces]")
    for resolution in ("full", "half"):
        for quality in ("low", "medium", "high"):
            if resolution == "half" and quality != "medium":
                continue
            tag = f"{resolution}/{quality}"
            flags = uniform + LOOK + SSAO + ["--ssao-resolution", resolution, "--ssao-quality", quality,
                                            "--debug-view", "ssao"]
            png, report = render(box, f"box_dbg_{resolution}_{quality}", flags + BOX_VIEW)
            if png is None:
                check(False, f"{tag}: box render")
                continue
            vis, cam = visibility(png), Camera(report)
            corner = at(vis, cam, (0.97, 0.03, -0.97))
            crease = at(vis, cam, (0.0, 0.02, -0.98))
            centre = at(vis, cam, (0.0, 0.0, 0.0))
            check(corner < CORNER_MAX, f"{tag}: inner corner visibility {corner:.3f} < {CORNER_MAX}")
            check(crease < CREASE_MAX, f"{tag}: floor/back-wall crease {crease:.3f} < {CREASE_MAX}")
            check(centre >= OPEN_MIN, f"{tag}: floor centre (1 m from the walls) {centre:.3f} >= {OPEN_MIN}")
            png, report = render(plane, f"plane_dbg_{resolution}_{quality}", flags + PLANE_VIEW)
            if png is None:
                check(False, f"{tag}: plane render")
                continue
            vis, cam = visibility(png), Camera(report)
            # The plane without the cube and its 1 m surroundings in screen space (the cube floats 3 m up: it
            # occludes nothing, but its pixels are not plane pixels).
            covered = vis > 0.0
            cube = np.zeros_like(covered)
            for x in (-0.5, 0.5):
                for y in (3.0, 4.0):
                    for z in (-0.5, 0.5):
                        px, py = cam.project((x, y, z))
                        cube[max(0, int(py) - 2):int(py) + 3, max(0, int(px) - 2):int(px) + 3] = True
            ys, xs = np.nonzero(cube)
            box_mask = np.zeros_like(covered)
            box_mask[ys.min() - 4:ys.max() + 5, xs.min() - 4:xs.max() + 5] = True
            plane_px = covered & ~box_mask
            values = vis[plane_px]
            check(values.size > 50000 and values.min() >= OPEN_MIN and values.mean() >= OPEN_MEAN_MIN,
                  f"{tag}: open plane ({values.size} px) min {values.min():.3f} >= {OPEN_MIN}, "
                  f"mean {values.mean():.4f} >= {OPEN_MEAN_MIN}")
            faces = {"top": (0.0, 4.0, 0.0), "+x": (0.5, 3.5, 0.0), "+z": (0.0, 3.5, 0.5)}
            low = {k: at(vis, cam, p) for k, p in faces.items()}
            check(min(low.values()) >= OPEN_MIN,
                  f"{tag}: convex cube faces {', '.join(f'{k} {v:.3f}' for k, v in low.items())} >= {OPEN_MIN}")
    # Shaded: SSAO darkens the box only where the debug view is below 1.
    on, _ = render(box, "box_shaded_on", uniform + LOOK + SSAO + BOX_VIEW)
    off, _ = render(box, "box_shaded_off", uniform + LOOK + BOX_VIEW)
    dbg = OUT / "box_dbg_full_medium.png"
    if on and off and dbg.exists():
        a, b, vis = linear(on), linear(off), visibility(dbg)
        # Two levels or more: a visibility just under 1 shows as 255 in the debug view and one level in the image.
        darker = (load(off) - load(on)).max(axis=2) >= 2
        occluded = dilate(vis < 0.999, 1)  # one pixel of slack at edges
        check(not (darker & ~occluded).any() and (a <= b + 1e-3).all(),
              f"shaded: SSAO only darkens ({darker.sum()} px, all where the visibility is below 1)")
        corner_drop = 1.0 - a[darker].min() / max(b[darker].max(), 1e-6)
        check(corner_drop > 0.15, f"shaded: the darkest corner pixel loses {corner_drop:.0%} of its sky light")


def check_halo(uniform, halo, box):
    import numpy as np
    print("[silhouettes: no halo]")
    for resolution, limit in (("full", HALO_PX), ("half", HALO_PX_HALF)):
        mask_png, _ = render(halo, f"halo_basecolor_{resolution}", uniform + LOOK + HALO_VIEW + ["--debug-view", "basecolor"])
        dbg_png, _ = render(halo, f"halo_dbg_{resolution}", uniform + LOOK + HALO_VIEW + SSAO +
                            ["--ssao-resolution", resolution, "--debug-view", "ssao"])
        if mask_png is None or dbg_png is None:
            check(False, f"{resolution}: halo renders")
            continue
        colors = load(mask_png)
        cube_mask = (colors[..., 1] > 128) & (colors[..., 0] < 128)
        wall_mask = (colors[..., 0] > 128) & (colors[..., 1] < 128)
        vis = visibility(dbg_png)
        darkened = wall_mask & (vis < HALO_LEVEL)
        near = dilate(cube_mask, limit)
        far_dark = darkened & ~near
        widest = 0
        for steps in range(1, 12):
            if not (darkened & ~dilate(cube_mask, steps)).any():
                widest = steps
                break
        check(cube_mask.sum() > 5000 and not far_dark.any(),
              f"{resolution}: no wall pixel beyond {limit} px of the silhouette is darkened ({darkened.sum()} darkened, "
              f"all within {widest} px)")
        check(vis[cube_mask].min() >= HALO_LEVEL,
              f"{resolution}: the cube's own pixels keep >= {HALO_LEVEL} (min {vis[cube_mask].min():.3f})")
    # Sensitivity: with a 1.5 m radius the wall is within reach and does darken around the cube (the measurement
    # above can see a halo).
    wide_png, _ = render(halo, "halo_dbg_wide", uniform + LOOK + HALO_VIEW + ["--ssao", "on", "--ssao-radius", "1.5",
                                                                               "--debug-view", "ssao"])
    mask_png = OUT / "halo_basecolor_full.png"
    if wide_png and mask_png.exists():
        colors = load(mask_png)
        wall_mask = (colors[..., 0] > 128) & (colors[..., 1] < 128)
        darkened = (wall_mask & (visibility(wide_png) < HALO_LEVEL)).sum()
        check(darkened > 500, f"sensitivity: radius 1.5 m darkens {darkened} wall pixels near the cube")
    # Against the sky (default HDR background): the open box, whose creases are occluded, changes only on its own
    # pixels; no background pixel around it changes.
    sky = ["--hide-grid", "--bloom", "off", "--auto-exposure", "off"] + BOX_VIEW
    on, _ = render(box, "sky_on", sky + SSAO)
    off, _ = render(box, "sky_off", sky)
    mask_png, _ = render(box, "sky_mask", ["--clear-color", "0,0,0", "--hide-grid"] + BOX_VIEW + ["--debug-view", "basecolor"])
    if on and off and mask_png:
        a, b = load(on), load(off)
        box_mask = load(mask_png).sum(axis=2) > 0
        changed = np.abs(a - b).max(axis=2) > 0
        check(box_mask.sum() > 5000 and changed.sum() > 1000 and not (changed & ~box_mask).any(),
              f"sky: {changed.sum()} pixels change, all on the box; no background pixel changes")


def check_taa(uniform, box):
    import numpy as np
    print("[temporal stability with TAA]")
    common = list(COMMON)
    common[common.index("--aa") + 1] = "taa"
    diffs = {}
    for label, extra in (("off", []), ("on", SSAO)):
        f1, _ = render(box, f"taa_{label}_64", uniform + LOOK + BOX_VIEW + extra, frame=64, common=common)
        f2, _ = render(box, f"taa_{label}_96", uniform + LOOK + BOX_VIEW + extra, frame=96, common=common)
        if f1 is None or f2 is None:
            check(False, f"TAA {label} renders")
            return
        diffs[label] = (np.abs(load(f1) - load(f2)).max(axis=2), load(f1).sum(axis=2) > 0)
    d_on, covered = diffs["on"]
    d_off, _ = diffs["off"]
    # The box's surfaces away from its silhouette and from the wall edges (3 px), where TAA's own edge jitter lives.
    inside = covered & ~dilate(~covered, 3)
    vis = OUT / "box_dbg_full_medium.png"
    occluded = (visibility(vis) < 0.95) & inside if vis.exists() else inside
    extra_max = d_on[inside].max() - d_off[inside].max()
    extra_mean = d_on[inside].mean() - d_off[inside].mean()
    check(extra_max <= TAA_MAX_LEVELS and extra_mean <= TAA_MEAN_LEVELS,
          f"TAA frames 64/96 inside the box: SSAO on max {d_on[inside].max():.0f} / mean {d_on[inside].mean():.3f} "
          f"levels, off max {d_off[inside].max():.0f} / mean {d_off[inside].mean():.3f} "
          f"(extra <= {TAA_MAX_LEVELS} / {TAA_MEAN_LEVELS}); occluded pixels ({occluded.sum()}): max "
          f"{d_on[occluded].max():.0f}, mean {d_on[occluded].mean():.3f}")


def check_sun(black, uniform, box):
    print("[the sun term is unchanged]")
    sun = ["--sun", "on", "--sun-dir", "20,50", "--sun-intensity", "1.5"]
    a, _ = render(box, "sun_black_on", black + LOOK + BOX_VIEW + sun + SSAO)
    b, _ = render(box, "sun_black_off", black + LOOK + BOX_VIEW + sun)
    check(a is not None and same(a, b), "black environment (no IBL) + sun: SSAO on is byte-identical to off")
    # Uniform environment: probe the linear HDR scene colour (before TAA and tone mapping) at occluded and open
    # points with the sun and SSAO switched on and off: SSAO must remove the same amount with and without the sun.
    _, report = render(box, "sun_probe_view", uniform + LOOK + BOX_VIEW + sun)
    if report is None:
        check(False, "probe view render")
        return
    cam = Camera(report)
    points = {"corner": (0.9, 0.05, -0.9), "crease": (0.0, 0.03, -0.95), "back wall": (0.0, 1.0, -1.0),
              "floor": (0.0, 0.0, 0.2), "right wall": (0.98, 0.3, -0.3)}
    pixels = {k: tuple(int(round(c)) for c in cam.project(p)) for k, p in points.items()}
    try:
        session = Session(box, uniform + LOOK + BOX_VIEW + sun)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        values = {}
        for sun_on in (True, False):
            for ssao_on in (True, False):
                session.call("set_environment", key="sun", value=sun_on)
                session.call("set_viewport_setting", key="ssao", value=ssao_on)
                session.settle()
                values[(sun_on, ssao_on)] = {k: session.call("probe", x=x, y=y)["hdrLuminance"]
                                             for k, (x, y) in pixels.items()}
        worst, removed = 0.0, 0.0
        for k in points:
            with_sun = values[(True, True)][k] - values[(True, False)][k]
            without = values[(False, True)][k] - values[(False, False)][k]
            worst = max(worst, abs(with_sun - without))
            removed = max(removed, -without)
        sunlit = max(values[(True, False)][k] - values[(False, False)][k] for k in points)
        check(worst <= SUN_ABS and removed > 0.02 and sunlit > 0.05,
              f"probes: (on - off) with the sun equals (on - off) without it within {worst:.2e} (<= {SUN_ABS}); "
              f"SSAO removes up to {removed:.3f}, the sun adds up to {sunlit:.3f}")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.kill()


def check_state(uniform, box):
    print("[registry, get_state, flags]")
    try:
        session = Session(box, uniform + LOOK + BOX_VIEW)
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session started: {error}")
        return
    try:
        state = session.call("get_state")["ssao"]["viewports"][0]
        check(not state["enabled"] and not state["active"] and state["memoryBytes"] == 0 and state["radius"] == 0.5
              and state["quality"] == "medium" and state["resolution"] == "full" and state["intensity"] == 1.0,
              f"defaults: off, nothing allocated, radius 0.5, medium, full, intensity 1 ({state})")
        session.call("set_viewport_setting", key="ssao", value=True)
        session.call("set_viewport_setting", key="ssao_radius", value=0.8)
        session.call("set_viewport_setting", key="ssao_quality", value="high")
        session.call("set_viewport_setting", key="ssao_resolution", value="half")
        session.call("set_viewport_setting", key="ssao_intensity", value=1.5)
        session.settle()
        state = session.call("get_state")["ssao"]["viewports"][0]
        check(state["enabled"] and state["active"] and state["slices"] == 4 and state["steps"] == 8 and
              state["resolution"] == "half" and abs(state["radius"] - 0.8) < 1e-6 and abs(state["intensity"] - 1.5) < 1e-6 and state["memoryBytes"] > 0,
              f"set_viewport_setting: active, high (4 x 8), half, radius 0.8, intensity 1.5, {state['memoryBytes']} bytes")
        check(state["gpuMs"] is not None and state["gpuMs"] > 0.0, f"get_state reports the GPU time ({state['gpuMs']} ms)")
        session.call("set_viewport_setting", key="render_mode", value="ssao")
        mode = session.call("get_state")["renderMode"]
        check(mode == "ssao", f"render_mode ssao ({mode})")
        session.call("set_viewport_setting", key="render_mode", value="shaded")
        for key, bad in (("ssao_quality", "ultra"), ("ssao_resolution", "quarter"), ("ssao_radius", -1.0),
                         ("ssao_intensity", 9.0)):
            try:
                session.call("set_viewport_setting", key=key, value=bad)
                check(False, f"{key} = {bad!r} is rejected")
            except glider_ctl.GliderError:
                check(True, f"{key} = {bad!r} is rejected")
        session.call("set_viewport_setting", key="ssao", value=False)
        session.settle()
        state = session.call("get_state")["ssao"]["viewports"][0]
        check(not state["active"] and state["memoryBytes"] == 0, "off again: every target is released")
        listed = {s["key"] for s in session.call("list_settings", scope="viewport")["settings"]}
        wanted = {"ssao", "ssao_radius", "ssao_intensity", "ssao_quality", "ssao_resolution"}
        check(wanted <= listed, f"list_settings lists the SSAO settings ({sorted(wanted - listed) or 'all'})")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.kill()
    zero_a, _ = render(box, "intensity0", uniform + LOOK + BOX_VIEW + SSAO + ["--ssao-intensity", "0"])
    zero_b = OUT / "cur_box_default.png"
    if not zero_b.exists():
        zero_b, _ = render(box, "cur_box_default", uniform + LOOK + BOX_VIEW)
    check(zero_a is not None and same(zero_a, zero_b), "--ssao-intensity 0 renders the SSAO-off image")
    for bad in (["--ssao", "maybe"], ["--ssao-quality", "ultra"], ["--ssao-resolution", "quarter"],
                ["--ssao-radius", "-2"], ["--ssao-intensity", "abc"]):
        result = run([EXE, box] + COMMON + bad + ["--exit-after-dump"], timeout=120)
        check(result.returncode != 0, f"{' '.join(bad)} is rejected")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default=None, help="comma-separated: baseline,box,halo,taa,sun,state")
    args = parser.parse_args()
    only = set(args.only.split(",")) if args.only else None

    def wanted(name):
        return only is None or name in only
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    OUT.mkdir(parents=True, exist_ok=True)
    uniform = ["--env", str(write_hdr(OUT / "uniform.hdr", 64, 32, lambda x, y: (0.5, 0.5, 0.5)))]
    black = ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]
    box = ssao_fixtures.box_interior(OUT / "box.gltf")
    plane = ssao_fixtures.open_plane(OUT / "plane.gltf")
    halo = ssao_fixtures.halo(OUT / "halo.gltf")
    if wanted("baseline"):
        check_baseline(baseline, uniform, box)
    if wanted("box"):
        check_box(uniform, box, plane)
    if wanted("halo"):
        check_halo(uniform, halo, box)
    if wanted("taa"):
        check_taa(uniform, box)
    if wanted("sun"):
        check_sun(black, uniform, box)
    if wanted("state"):
        check_state(uniform, box)
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in {len(gl_logs)} renders {bad}")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
