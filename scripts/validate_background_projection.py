# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Background projection: every background pixel shows the world direction that projects to it.

cubemap.vert projected the background cube with clip w + 0.1, which drew the HDR panorama about 7% (direction and
camera dependent) towards the image centre. The perspective path now projects the cube exactly (pos.xyww at the far
plane, no w offset) and cubemap.frag reads the world direction as the cube position; the orthographic path keeps the
original projection.

Checks (an empty scene, so every pixel is background):
  * markers: an asymmetric HDR panorama (build/background_projection/markers.hdr) holds narrow discs (0.5 deg radius)
    at known world directions, coloured so no two within 25 deg share a colour. For several yaw / pitch, vertical FoV
    30 / 60 / 90 and two viewport aspect ratios, with AA none and TAA, each visible marker's centroid lies within
    POSITION_TOLERANCE_PX of its direction projected through the reported camera;
  * the last build with the old projection (OLD_PROJECTION_EXE, 4ae2b7b; not --baseline-exe, which moves on) fails
    that check: its largest marker error exceeds BASELINE_MIN_ERROR_PX (skipped when that binary is absent);
  * sun disk: its centroid lies within POSITION_TOLERANCE_PX of the projected sun direction with the HDR (black
    panorama) and with the procedural sky;
  * seams: a smooth panorama (linear in the direction) shows no seam or hole: no dark pixel, and the image Laplacian
    on the cube's edges is no larger than elsewhere (SEAM_RATIO, SEAM_MAX_LAPLACIAN);
  * far plane: with nothing but background, the auto-exposure highlight meter (scene depth < 1 = surface) counts no
    surface cell at several views (the rasterized background depth rounded one 24-bit step below 1 on large clipped
    cube triangles; cubemap.frag writes depth 1 for a perspective camera);
  * orthographic: the marker panorama in an orthographic view is byte-identical to the baseline (path unchanged).

Run: python scripts/validate_background_projection.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b]
"""

import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same  # noqa: E402
from validate_sun import Camera  # noqa: E402

OUT = REPO / "build" / "background_projection"
OLD_PROJECTION_EXE = REPO / "build" / "baselines" / "4ae2b7b.exe"  # the last build with the w + 0.1 projection
COMMON = ["--isolated", "--fixed-frame-time", "0.05", "--vsync", "off"]
POSITION_TOLERANCE_PX = 1.0   # marker / disk centroid against the projected world direction
BASELINE_MIN_ERROR_PX = 3.0   # the old projection must miss by more than this somewhere
SEAM_RATIO = 1.5              # mean |Laplacian| on cube edges against the rest of the image
SEAM_MAX_LAPLACIAN = 8.0      # largest |Laplacian| (8-bit units, per channel) anywhere in the smooth panorama
HOLE_MIN_VALUE = 30           # the smooth panorama is at least 0.1 everywhere (about 50 here): no hole
MARKER_RADIUS_DEG = 0.5
MARKER_RADIANCE = 0.6
PANORAMA_SIZE = (4096, 2048)
COLOURS = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 0), (1, 0, 1), (0, 1, 1), (1, 1, 1)]

WINDOWS = {"wide": "1600x760", "tall": "1000x1000"}
ORIENTATIONS = [(0.0, 0.0), (37.0, 25.0), (-120.0, -35.0), (200.0, 60.0)]
FOVS = [30.0, 60.0, 90.0]


# ---------------------------------------------------------------------------------------------
# Panoramas
# ---------------------------------------------------------------------------------------------

def panorama_directions(width, height):
    """World direction of each panorama texel (IBLSampler.cpp panoramaDirection; HDR rotation 0)."""
    import numpy as np
    theta = (np.arange(height) + 0.5) / height * math.pi
    phi = ((np.arange(width) + 0.5) / width - 0.5) * 2.0 * math.pi
    t, p = np.meshgrid(theta, phi, indexing="ij")
    return np.stack([np.sin(t) * np.cos(p), np.cos(t), np.sin(t) * np.sin(p)], axis=-1)


def write_rgbe(path, rgb):
    """Uncompressed Radiance RGBE, row 0 at the top of the file."""
    import numpy as np
    height, width, _ = rgb.shape
    v = rgb.max(axis=-1)
    mantissa, exponent = np.frexp(v)
    scale = np.where(v > 1e-32, mantissa * 256.0 / np.maximum(v, 1e-32), 0.0)
    out = np.zeros((height, width, 4), dtype=np.uint8)
    out[..., :3] = np.clip(rgb * scale[..., None], 0, 255).astype(np.uint8)
    out[..., 3] = np.where(v > 1e-32, exponent + 128, 0).astype(np.uint8)
    header = f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode("ascii")
    path.write_bytes(header + out.tobytes())


def marker_set():
    """Fibonacci-sphere directions (200) with a greedy colouring: no two markers within 25 deg share a colour."""
    count = 200
    golden = math.pi * (3.0 - math.sqrt(5.0))
    dirs = []
    for i in range(count):
        y = 1.0 - 2.0 * (i + 0.5) / count
        r = math.sqrt(1.0 - y * y)
        a = golden * i + 0.4  # offset: no marker on an axis or a cube edge by construction
        dirs.append((r * math.cos(a), y, r * math.sin(a)))
    colours = []
    for i, d in enumerate(dirs):
        used = {colours[j] for j in range(i)
                if math.degrees(math.acos(max(-1.0, min(1.0, sum(a * b for a, b in zip(d, dirs[j])))))) < 25.0}
        colours.append(next(c for c in range(len(COLOURS)) if c not in used))
    return dirs, colours


def marker_panorama():
    import numpy as np
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "markers.hdr"
    dirs, colours = marker_set()
    if not path.exists():
        d = panorama_directions(*PANORAMA_SIZE)
        rgb = np.zeros(d.shape, dtype=np.float64)
        cos_r = math.cos(math.radians(MARKER_RADIUS_DEG))
        for m, c in zip(dirs, colours):
            dot = d @ np.array(m)
            inside = dot >= cos_r
            rgb[inside] = np.array(COLOURS[c], dtype=np.float64) * MARKER_RADIANCE
        write_rgbe(path, rgb)
    return path, dirs, colours


def smooth_panorama():
    import numpy as np
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "smooth.hdr"
    if not path.exists():
        d = panorama_directions(1024, 512)
        write_rgbe(path, 0.3 + 0.2 * d)  # linear in the direction: no edge anywhere
    return path


def black_panorama():
    import numpy as np
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "black.hdr"
    if not path.exists():
        write_rgbe(path, np.zeros((32, 64, 3)))
    return path


def empty_model():
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "empty.gltf"
    path.write_text(json.dumps({"asset": {"version": "2.0", "generator": "background projection fixture"}, "scene": 0,
                                "scenes": [{"nodes": [0]}], "nodes": [{"name": "Empty"}]}), encoding="utf-8")
    return path


# ---------------------------------------------------------------------------------------------
# Runs
# ---------------------------------------------------------------------------------------------

def render(name, flags, exe=EXE):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    subprocess.run([str(a) for a in [exe, empty_model()] + COMMON + list(flags) +
                    ["--capture-viewport", png, "--capture-frame", "24", "--profile-report", report,
                     "--exit-after-profile"]], cwd=REPO, capture_output=True, text=True, timeout=900)
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else None
    return (png if png.exists() else None), data


def load(png):
    import numpy as np
    from PIL import Image
    return np.asarray(Image.open(png).convert("RGB"), dtype=np.float64)


def camera_flags(window, yaw, pitch, fov, aa):
    return ["--window-size", window, "--camera-yaw", str(yaw), "--camera-pitch", str(pitch), "--camera-fov", str(fov),
            "--aa", aa, "--bloom", "off", "--hide-grid"]


def disc_centroid(cam, direction, x0, x1, y0, y1, samples=4):
    """Screen centroid of the pixels whose ray lies within MARKER_RADIUS_DEG of direction (pixel-centre coordinates)."""
    import numpy as np
    offsets = (np.arange(samples) + 0.5) / samples - 0.5
    ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float64)
    cos_r = math.cos(math.radians(MARKER_RADIUS_DEG))
    coverage = np.zeros(xs.shape)
    for oy in offsets:
        for ox in offsets:
            nx = (2.0 * (xs + ox + 0.5) / cam.width - 1.0) * cam.t * cam.aspect
            ny = (1.0 - 2.0 * (ys + oy + 0.5) / cam.height) * cam.t
            ray = np.stack([cam.f[k] + nx * cam.r[k] + ny * cam.u[k] for k in range(3)], axis=-1)
            ray /= np.linalg.norm(ray, axis=-1, keepdims=True)
            coverage += (ray @ np.array(direction)) >= cos_r
    total = coverage.sum()
    if total <= 0.0:
        return float("nan"), float("nan")
    return (coverage * xs).sum() / total, (coverage * ys).sum() / total


def measure_markers(png, report, dirs, colours):
    """[(marker index, expected (x, y), measured (x, y), error px)] for every marker well inside the image."""
    import numpy as np
    image = load(png)
    cam = Camera(report)
    h, w, _ = image.shape
    projected = []
    for i, d in enumerate(dirs):
        if sum(a * b for a, b in zip(d, cam.f)) <= 0.05:
            continue
        p = cam.project([cam.eye[k] + d[k] for k in range(3)])
        projected.append((i, p))
    results = []
    for i, (x, y) in projected:
        if not (25 <= x < w - 25 and 25 <= y < h - 25):
            continue
        # Search window: below half the distance to the nearest projected marker of the same colour.
        nearest = min([math.hypot(x - qx, y - qy) for j, (qx, qy) in projected if j != i and colours[j] == colours[i]]
                      + [1e9])
        radius = int(min(80.0, 0.45 * nearest))
        x0, x1 = max(0, int(x) - radius), min(w, int(x) + radius + 1)
        y0, y1 = max(0, int(y) - radius), min(h, int(y) + radius + 1)
        patch = image[y0:y1, x0:x1]
        on = [k for k in range(3) if COLOURS[colours[i]][k]]
        off = [k for k in range(3) if not COLOURS[colours[i]][k]]
        weight = patch[..., on].min(axis=-1) - (patch[..., off].max(axis=-1) if off else 0.0)
        # The marker is the connected blob holding the strongest match: tone mapping desaturates the other colours a
        # little, so their discs can pass the threshold weakly; they are separate blobs.
        from scipy import ndimage
        labels, _ = ndimage.label(weight > 12.0)
        peak = labels.flat[int(np.argmax(weight))]
        weight = np.where((labels == peak) & (peak > 0), weight, 0.0)
        total = weight.sum()
        if total <= 0.0:
            results.append((i, (x, y), None, float("inf")))
            continue
        ys, xs = np.mgrid[y0:y1, x0:x1]
        mx, my = (weight * xs).sum() / total, (weight * ys).sum() / total
        # Expected: the centroid of the disc's own projection (4 x 4 samples per pixel), so the perspective stretch of
        # a disc near the image edge is part of the prediction, not an error.
        ex, ey = disc_centroid(cam, dirs[i], x0, x1, y0, y1)
        results.append((i, (ex, ey), (mx, my), math.hypot(mx - ex, my - ey)))
    return results


def disk_centroid(png):
    import numpy as np
    image = load(png)
    mask = image.min(axis=-1) >= 250.0
    if not mask.any():
        return None
    ys, xs = np.nonzero(mask)
    return xs.mean(), ys.mean(), int(mask.sum())


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_markers():
    print("[Markers: background pixels show the world direction that projects to them]")
    hdr, dirs, colours = marker_panorama()
    cases = [(win, yaw, pitch, fov, "none") for win in WINDOWS for (yaw, pitch) in ORIENTATIONS for fov in FOVS]
    cases += [("wide", yaw, pitch, fov, "taa") for (yaw, pitch) in ORIENTATIONS[:2] for fov in FOVS]
    overall = []
    for win, yaw, pitch, fov, aa in cases:
        name = f"markers_{win}_y{yaw:g}_p{pitch:g}_f{fov:g}_{aa}"
        png, report = render(name, ["--env", hdr] + camera_flags(WINDOWS[win], yaw, pitch, fov, aa))
        if png is None or report is None:
            check(False, f"{name}: rendered")
            continue
        results = measure_markers(png, report, dirs, colours)
        errors = [r[3] for r in results]
        worst = max(errors) if errors else float("inf")
        overall += errors
        vw, vh = report["camera"]["viewport"]
        check(len(results) >= 1 and worst <= POSITION_TOLERANCE_PX,
              f"{name} ({vw}x{vh}): {len(results)} markers, max error {worst:.2f} px, "
              f"mean {sum(errors) / max(1, len(errors)):.2f} px (limit {POSITION_TOLERANCE_PX} px)")
    if overall:
        print(f"  INFO all {len(overall)} marker measurements: max {max(overall):.3f} px, "
              f"mean {sum(overall) / len(overall):.3f} px")
    baseline = OLD_PROJECTION_EXE
    if not baseline.exists():
        print(f"  SKIP old-projection check: {baseline.name} not found")
        return
    print(f"[Markers with {baseline.stem} (old w + 0.1 projection): must miss]")
    worst_all = 0.0
    for win, yaw, pitch, fov in [("wide", 37.0, 25.0, 60.0), ("tall", -120.0, -35.0, 90.0), ("wide", 0.0, 0.0, 30.0)]:
        name = f"base_markers_{win}_y{yaw:g}_p{pitch:g}_f{fov:g}"
        png, report = render(name, ["--env", hdr] + camera_flags(WINDOWS[win], yaw, pitch, fov, "none"), exe=baseline)
        if png is None or report is None:
            check(False, f"{name}: rendered")
            continue
        errors = [r[3] for r in measure_markers(png, report, dirs, colours)]
        worst = max(errors) if errors else 0.0
        worst_all = max(worst_all, worst)
        print(f"  INFO {name}: {len(errors)} markers, max error {worst:.2f} px, "
              f"mean {sum(errors) / max(1, len(errors)):.2f} px")
    check(worst_all > BASELINE_MIN_ERROR_PX,
          f"{baseline.stem} misses the projected directions (max {worst_all:.2f} px > {BASELINE_MIN_ERROR_PX} px)")


def check_sun_disk():
    print("[Sun disk centre on the projected sun direction]")
    black = black_panorama()
    cases = [
        ("hdr", ["--env", black, "--sun", "on", "--sun-mode", "manual"]),
        ("sky", ["--environment", "sky", "--sun-source", "manual"]),
    ]
    for source, flags in cases:
        # Sun offset (azimuth, elevation) from the view centre, which is at azimuth 180 - yaw, elevation -pitch.
        for win, yaw, pitch, fov, daz, delev in [("wide", 20.0, -15.0, 60.0, -12.0, 9.0),
                                                 ("tall", -40.0, -10.0, 90.0, 20.0, 25.0),
                                                 ("wide", 0.0, -20.0, 30.0, 6.0, -4.0)]:
            az, el = 180.0 - yaw + daz, -pitch + delev
            name = f"disk_{source}_{win}_f{fov:g}"
            png, report = render(name, flags + ["--sun-dir", f"{az},{el}", "--sun-angular-radius", "1"] +
                                 camera_flags(WINDOWS[win], yaw, pitch, fov, "none"))
            if png is None or report is None:
                check(False, f"{name}: rendered")
                continue
            a, e = math.radians(az), math.radians(el)
            sun = [math.cos(e) * math.sin(a), math.sin(e), math.cos(e) * math.cos(a)]
            cam = Camera(report)
            ex, ey = cam.project([cam.eye[k] + sun[k] for k in range(3)])
            got = disk_centroid(png)
            if got is None:
                check(False, f"{name}: disk visible at ({ex:.1f}, {ey:.1f})")
                continue
            err = math.hypot(got[0] - ex, got[1] - ey)
            check(err <= POSITION_TOLERANCE_PX,
                  f"{name}: disk centroid ({got[0]:.2f}, {got[1]:.2f}) vs projected ({ex:.2f}, {ey:.2f}): "
                  f"{err:.2f} px ({got[2]} saturated px; limit {POSITION_TOLERANCE_PX} px)")


def check_seams():
    print("[Seams and coverage: a smooth panorama]")
    import numpy as np
    hdr = smooth_panorama()
    for win, yaw, pitch, fov, aa in [("wide", 45.0, 35.0, 90.0, "none"), ("tall", 10.0, -50.0, 90.0, "none"),
                                     ("wide", 45.0, 35.0, 90.0, "taa"), ("wide", 30.0, 20.0, 60.0, "none")]:
        name = f"seams_{win}_y{yaw:g}_p{pitch:g}_f{fov:g}_{aa}"
        png, report = render(name, ["--env", hdr, "--exposure", "0"] + camera_flags(WINDOWS[win], yaw, pitch, fov, aa))
        if png is None or report is None:
            check(False, f"{name}: rendered")
            continue
        image = load(png)
        h, w, _ = image.shape
        lap = np.abs(4.0 * image[1:-1, 1:-1] - image[:-2, 1:-1] - image[2:, 1:-1] - image[1:-1, :-2]
                     - image[1:-1, 2:]).max(axis=-1)
        # Cube-edge pixels: the two largest |components| of the pixel's world ray within one pixel's angle.
        cam = Camera(report)
        ys, xs = np.mgrid[1:h - 1, 1:w - 1]
        nx = (2.0 * (xs + 0.5) / w - 1.0) * cam.t * cam.aspect
        ny = (1.0 - 2.0 * (ys + 0.5) / h) * cam.t
        ray = np.stack([cam.f[k] + nx * cam.r[k] + ny * cam.u[k] for k in range(3)], axis=-1)
        ray /= np.linalg.norm(ray, axis=-1, keepdims=True)
        comp = np.sort(np.abs(ray), axis=-1)
        pixel_angle = 2.0 * cam.t / h
        edge = (comp[..., 2] - comp[..., 1]) < 1.5 * pixel_angle
        edge_mean = float(lap[edge].mean()) if edge.any() else 0.0
        rest_mean = float(lap[~edge].mean())
        ok = (edge.sum() > 50 and image.min() >= HOLE_MIN_VALUE and float(lap.max()) <= SEAM_MAX_LAPLACIAN
              and edge_mean <= SEAM_RATIO * rest_mean + 0.5)
        check(ok, f"{name}: min value {image.min():.0f} (>= {HOLE_MIN_VALUE}), max |Laplacian| {lap.max():.1f} "
                  f"(<= {SEAM_MAX_LAPLACIAN}), mean on {int(edge.sum())} cube-edge px {edge_mean:.3f} vs "
                  f"{rest_mean:.3f} elsewhere (<= {SEAM_RATIO} x + 0.5)")


def check_far_depth():
    print("[Background depth: exactly the far plane]")
    for yaw, pitch, fov in [(0.0, 0.0, 45.0), (35.0, 12.0, 45.0), (35.0, -30.0, 90.0), (120.0, 50.0, 60.0)]:
        name = f"depth_y{yaw:g}_p{pitch:g}_f{fov:g}"
        _, report = render(name, ["--environment", "sky", "--time-of-day-mode", "on", "--exposure-mode", "auto"] +
                           camera_flags(WINDOWS["wide"], yaw, pitch, fov, "none"))
        try:
            meter = report["night"]["exposure"]["viewports"][0]["meter"]
        except (TypeError, KeyError, IndexError):
            check(False, f"{name}: meter report")
            continue
        check(meter["surfaceFraction"] == 0.0,
              f"{name}: background-only image, highlight-meter surface fraction {meter['surfaceFraction']:.4f} (must be 0)")


def check_orthographic(baseline):
    print("[Orthographic view: unchanged projection, byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        return
    hdr, _, _ = marker_panorama()
    for aa in ("none", "taa"):
        flags = ["--env", hdr, "--camera-projection", "orthographic"] + camera_flags(WINDOWS["wide"], 37.0, 25.0, 60.0, aa)
        now, report = render(f"ortho_{aa}", flags)
        before, _ = render(f"base_ortho_{aa}", flags, exe=baseline)
        ortho = report is not None and report["camera"].get("projection") == "orthographic"
        check(ortho and same(now, before), f"orthographic ({aa}): byte-identical to {baseline.stem}"
                                           f" (projection reported: {report['camera'].get('projection') if report else None})")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default=None, help="comma-separated: markers,disk,seams,depth,ortho")
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    only = set(args.only.split(",")) if args.only else None
    if not EXE.exists():
        sys.exit(f"no build at {EXE}")
    OUT.mkdir(parents=True, exist_ok=True)
    if only is None or "markers" in only:
        check_markers()
    if only is None or "disk" in only:
        check_sun_disk()
    if only is None or "seams" in only:
        check_seams()
    if only is None or "depth" in only:
        check_far_depth()
    if only is None or "ortho" in only:
        check_orthographic(baseline)
    finish()


if __name__ == "__main__":
    main()
