"""Focused check of three interactive and memory costs, each against the baseline build.

  * ocean: dragging a Live ocean setting builds the spectrum on a worker thread: the frames during the drag stay well
    under the baseline's, the sea keeps showing, and the settled image equals a fresh launch with the final value.
  * sky: a clock running at 1x real time does not refilter every frame; stopping it ends on the exact sun (the LUT's
    sun equals the current sun, refined); a fast clock still updates.
  * images: the decoded pixels leave the CPU heap after the upload (startup report, private memory on the E195 cockpit
    when present), the rendered image is unchanged, and the inspector previews a used and an unused image as before.
  * loop: the same drag on a Looping ocean: the worker computes the spectrum, the loop job runs in slices afterwards.
  * panorama: a re-filter of the same HDR reuses the decoded pixels with the same image as a fresh launch; another
    environment, idle time and low memory free them.
  * traversal: frames with an unchanged model reuse the scene traversal; hiding a node still takes effect at once.
  * filter: the compiled shadow filter changes at run time to the image of a fresh launch.

Run: python scripts/validate_interactive_costs.py [--baseline-exe build/baselines/<sha>.exe]
         [--only ocean,loop,sky,images,panorama,traversal,filter]
"""

import argparse
import base64
import json
import os
import statistics
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
from glider_harness import start_pipe_session  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same  # noqa: E402

OUT = REPO / "build" / "interactive_costs"
INTERACTIVE = ["--isolated", "--window-size", "1280x720", "--vsync", "off", "--control-pipe"]
E195 = Path(r"F:/new-fss-aircraft-ejet/msfs24-projects/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf")


def wait_until(predicate, seconds=60.0):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(0.05)
    return False


def close(process, pipe):
    errors = pipe.call("get_state")["glErrors"]
    pipe.call("quit")
    pipe.close()
    process.wait(60)
    return errors


# --- ocean ---------------------------------------------------------------------------------------

OCEAN = ["--ground", "ocean", "--hide-grid", "--ocean-mode", "live", "--ocean-quality", "high", "--ocean-time", "6",
         "--aa", "none", "--exposure-mode", "curve"]
FINAL_WIND = 14.0


def ocean_state(pipe):
    return pipe.call("get_state")["ground"]["ocean"]


def ocean_drag(exe, label, flags=None, settle=0.0):
    process, pipe = start_pipe_session([exe] + INTERACTIVE + (flags or OCEAN) + ["--ocean-wind-speed", "6"])
    try:
        assert wait_until(lambda: ocean_state(pipe)["ready"]), "the first set"
        time.sleep(settle)
        times = []
        for step in range(40):
            start = time.perf_counter()
            pipe.call("set_viewport_setting", key="ocean_wind_speed", value=6.0 + (FINAL_WIND - 6.0) * step / 39.0)
            times.append((time.perf_counter() - start) * 1000.0)
            time.sleep(0.015)  # about one slider change per frame at 60 Hz, so builds finish during the drag
        settled = wait_until(lambda: not ocean_state(pipe)["busy"] and ocean_state(pipe)["precompute"]["sets"] > 0)
        time.sleep(0.5)  # the last build, if the busy flag was read just before it started
        assert wait_until(lambda: not ocean_state(pipe)["busy"])
        state = ocean_state(pipe)
        png = OUT / f"ocean_{label}.png"
        pipe.call("capture", path=str(png), settle_frames=24, overlays=False)
        times.sort()
        return {"median": statistics.median(times), "max": times[-1], "p90": times[int(len(times) * 0.9)], "state": state, "png": png,
                "settled": settled, "errors": close(process, pipe)}
    except Exception:
        process.kill()
        raise


def check_ocean(baseline):
    print("[ocean: a Live setting dragged at High]")
    new = ocean_drag(EXE, "new")
    old = ocean_drag(baseline, "baseline")
    print(f"  INFO per set call: baseline median {old['median']:.1f}, p90 {old['p90']:.1f}, max {old['max']:.1f} ms; "
          f"new median {new['median']:.1f}, p90 {new['p90']:.1f}, max {new['max']:.1f} ms")
    background = new["state"].get("liveBackground", {})
    print(f"  INFO new: {background}, sets {new['state']['precompute']['sets']}, baseline sets {old['state']['precompute']['sets']}")
    check(new["median"] < old["median"] * 0.5, f"the drag's median round trip halves at least: {old['median']:.1f} -> {new['median']:.1f} ms")
    check(background.get("builds", 0) >= 2, f"the worker built spectra during the drag: {background}")
    check(new["p90"] < old["median"] * 0.5, f"nine in ten of the drag's round trips stay under half the baseline's typical one: {new['p90']:.1f} ms")
    check(new["state"]["liveSet"] and new["state"]["windSpeed"] == FINAL_WIND, "the settled sea has the final wind")
    check(new["errors"] == 0, f"{new['errors']} GL errors")

    process, pipe = start_pipe_session([EXE] + INTERACTIVE + OCEAN + ["--ocean-wind-speed", str(FINAL_WIND)])
    try:
        assert wait_until(lambda: ocean_state(pipe)["ready"])
        fresh = OUT / "ocean_fresh.png"
        pipe.call("capture", path=str(fresh), settle_frames=24, overlays=False)
        close(process, pipe)
    except Exception:
        process.kill()
        raise
    check(same(new["png"], fresh), "the settled image equals a fresh launch with the final wind")


LOOP = ["--ground", "ocean", "--hide-grid", "--ocean-mode", "loop", "--ocean-quality", "high", "--ocean-time", "6",
        "--aa", "none", "--exposure-mode", "curve"]


def check_loop(baseline):
    print("[ocean: a Looping setting dragged at High]")
    # The settle time keeps the first frames after the launch (a stall of about 100 ms) out of the drag's times.
    new = ocean_drag(EXE, "loop_new", LOOP, settle=3.0)
    old = ocean_drag(baseline, "loop_baseline", LOOP, settle=3.0)
    print(f"  INFO per set call: baseline median {old['median']:.1f}, p90 {old['p90']:.1f}, max {old['max']:.1f} ms; "
          f"new median {new['median']:.1f}, p90 {new['p90']:.1f}, max {new['max']:.1f} ms")
    background = new["state"].get("liveBackground", {})
    print(f"  INFO new: {background}, sets {new['state']['precompute']['sets']}, baseline sets {old['state']['precompute']['sets']}")
    check(1 <= background.get("builds", 0) <= 3, f"the worker computes the value the drag rests on, not every value: {background}")
    check(new["max"] < old["max"] * 0.5, f"the drag's worst round trip halves at least: {old['max']:.1f} -> {new['max']:.1f} ms")
    check(not new["state"]["liveSet"] and new["state"]["windSpeed"] == FINAL_WIND, "the settled loop has the final wind")
    check(new["errors"] == 0, f"{new['errors']} GL errors")
    process, pipe = start_pipe_session([EXE] + INTERACTIVE + LOOP + ["--ocean-wind-speed", str(FINAL_WIND)])
    try:
        assert wait_until(lambda: ocean_state(pipe)["ready"] and not ocean_state(pipe)["busy"])
        fresh = OUT / "ocean_loop_fresh.png"
        pipe.call("capture", path=str(fresh), settle_frames=24, overlays=False)
        close(process, pipe)
    except Exception:
        process.kill()
        raise
    check(same(new["png"], fresh), "the settled image equals a fresh launch with the final wind")


# --- panorama ------------------------------------------------------------------------------------

PANORAMA = ["--aa", "none", "--hide-grid"]


def panorama(pipe):
    return pipe.call("get_state")["environment"]["decodedPanorama"]


def check_panorama():
    print("[decoded HDR panorama]")
    process, pipe = start_pipe_session([EXE] + INTERACTIVE + PANORAMA)
    try:
        assert wait_until(lambda: panorama(pipe)["decodes"] == 1), "the first decode"
        first = panorama(pipe)
        check(first["bytes"] > 0 and first["reuses"] == 0, f"the first load decodes and keeps the pixels: {first}")
        start = time.perf_counter()
        pipe.call("set_environment", key="sun", value=True)
        check(wait_until(lambda: panorama(pipe)["reuses"] == 1), "switching the sun re-filters from the kept pixels")
        print(f"  INFO re-filter from the kept pixels: {(time.perf_counter() - start) * 1000.0:.0f} ms")
        check(panorama(pipe)["decodes"] == 1, "no second decode")
        reused = OUT / "panorama_reused.png"
        pipe.call("capture", path=str(reused), settle_frames=24, overlays=False)
        pipe.call("set_environment", key="environment", value="sky")
        check(wait_until(lambda: panorama(pipe)["bytes"] == 0), "the procedural sky frees the pixels")
        pipe.call("set_environment", key="environment", value="hdr")
        check(wait_until(lambda: panorama(pipe)["decodes"] == 2), "back to the HDR: decoded again")
        check(close(process, pipe) == 0, "no GL errors")
    except Exception:
        process.kill()
        raise
    process, pipe = start_pipe_session([EXE] + INTERACTIVE + PANORAMA + ["--sun", "on"])
    try:
        assert wait_until(lambda: panorama(pipe)["decodes"] == 1)
        fresh = OUT / "panorama_fresh.png"
        pipe.call("capture", path=str(fresh), settle_frames=24, overlays=False)
        close(process, pipe)
    except Exception:
        process.kill()
        raise
    check(same(reused, fresh), "the re-filtered image equals a fresh launch")
    for name, variable, value in (("idle time", "GLIDER_TEST_PANORAMA_IDLE_SECONDS", "3"), ("low memory", "GLIDER_TEST_PANORAMA_LOW_MEMORY", "1")):
        process, pipe = start_pipe_session([EXE] + INTERACTIVE + PANORAMA, env={**os.environ, variable: value})
        try:
            assert wait_until(lambda: panorama(pipe)["decodes"] == 1)
            check(wait_until(lambda: panorama(pipe)["bytes"] == 0, 20.0), f"{name} frees the pixels")
            pipe.call("set_environment", key="sun", value=True)
            check(wait_until(lambda: panorama(pipe)["decodes"] == 2), f"after {name}: the next re-filter decodes again")
            close(process, pipe)
        except Exception:
            process.kill()
            raise


# --- traversal -----------------------------------------------------------------------------------

def two_triangles():
    positions = struct.pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0)
    uri = "data:application/octet-stream;base64," + base64.b64encode(positions).decode()
    document = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0, 1]}],
                "nodes": [{"name": "Left", "mesh": 0, "translation": [-1.2, 0, 0]}, {"name": "Right", "mesh": 0}],
                "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
                "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]}],
                "bufferViews": [{"buffer": 0, "byteLength": 36}], "buffers": [{"byteLength": 36, "uri": uri}]}
    path = OUT / "two_triangles.gltf"
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


def traversal_run(model, name, lines, flags=()):
    script = OUT / f"{name}.txt"
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    script.write_text("\n".join(lines + [f"shot {png.relative_to(REPO).as_posix()}", "wait 2", "snap end"]) + "\n", encoding="utf-8")
    report = OUT / f"{name}.json"
    report.unlink(missing_ok=True)
    subprocess.run([str(EXE), str(model), "--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off",
                    "--aa", "none", *flags, "--ui-script", str(script), "--ui-report", str(report)],
                   cwd=REPO, capture_output=True, timeout=300)
    data = json.loads(report.read_text(encoding="utf-8"))
    return png, data["snaps"][-1]["revisions"]


def check_traversal():
    print("[scene traversal across frames]")
    model = two_triangles()
    # An orbit: the camera moves, so every frame draws, and the model stays the same.
    orbit = ["wait 8"] + ["drag 700 400 716 400", "wait 3"] * 15
    moved, rev = traversal_run(model, "traversal_orbit", orbit)
    print(f"  INFO an orbit: {rev.get('traversalBuilds')} builds, {rev.get('traversalReuses')} reuses, {rev.get('sceneRenders')} scene renders")
    check(rev.get("sceneRenders", 0) >= 15, f"the orbit draws: {rev.get('sceneRenders')} scene renders")
    check(rev.get("traversalBuilds", 99) <= 5, f"it traverses a few times only: {rev.get('traversalBuilds')}")
    check(rev.get("traversalReuses", 0) >= rev.get("sceneRenders", 0) - 5, f"the other frames reuse it: {rev.get('traversalReuses')}")
    separate, off = traversal_run(model, "traversal_orbit_separate", orbit, ["--disable-shared-traversal"])
    check(off.get("traversalReuses", 1) == 0, "--disable-shared-traversal still traverses per viewport")
    check(same(moved, separate), "byte-identical with and without the reuse")
    hide = orbit + ["clickp hier.node_eye 0", "wait 10", "drag 700 400 716 400", "wait 10"]
    hidden, _ = traversal_run(model, "traversal_hidden", hide)
    fresh, _ = traversal_run(model, "traversal_hidden_separate", hide, ["--disable-shared-traversal"])
    check(not same(hidden, moved), "hiding a node after frames of reuse changes the image")
    check(same(hidden, fresh), "and it equals the run that traverses every time")


# --- shadow filter -------------------------------------------------------------------------------

def shadow_capture(name, flags, change=None):
    process, pipe = start_pipe_session([EXE] + INTERACTIVE + ["--aa", "none", "--hide-grid", "--sun", "on"] + flags)
    try:
        png = OUT / f"{name}.png"
        pipe.call("capture", path=str(OUT / f"{name}_first.png"), settle_frames=12, overlays=False)
        if change:
            pipe.call("set_persist", enabled=True)  # --isolated still writes nothing
            pipe.call("set_viewport_setting", key="shadow_filter", value=change)
        pipe.call("capture", path=str(png), settle_frames=12, overlays=False)
        check(close(process, pipe) == 0, f"{name}: no GL errors")
        return png
    except Exception:
        process.kill()
        raise


def check_shadow_filter():
    print("[compiled shadow filter]")
    images = {}
    for name in ("hard", "pcf", "soft"):
        images[name] = shadow_capture(f"filter_{name}", ["--shadow-filter", name])
    check(not same(images["hard"], images["pcf"]) and not same(images["pcf"], images["soft"]), "the three filters differ")
    for start, target in (("pcf", "hard"), ("hard", "soft"), ("soft", "pcf")):
        changed = shadow_capture(f"filter_{start}_to_{target}", ["--shadow-filter", start], target)
        check(same(changed, images[target]), f"changed at run time, {start} to {target}: the image of a fresh launch")


# --- sky -----------------------------------------------------------------------------------------

SKY = ["--environment", "sky", "--time-of-day-mode", "on", "--time-preset", "afternoon", "--time-play", "on"]


def sky_state(pipe):
    return pipe.call("get_state")["sky"]


def sky_delta(pipe, seconds=4.0):
    before = sky_state(pipe)["stats"]
    time.sleep(seconds)
    after = sky_state(pipe)["stats"]
    return {key: after[key] - before[key] for key in ("slicedFrames", "fastJobs", "lutSkyViewBuilds")}


def sky_clock(exe, scale):
    process, pipe = start_pipe_session([exe] + INTERACTIVE + SKY + ["--time-scale", str(scale)])
    time.sleep(2.0)
    return process, pipe, sky_delta(pipe)


def check_sky(baseline):
    print("[sky: the clock running at 1x real time]")
    process, pipe, old = sky_clock(baseline, 1)
    close(process, pipe)
    process, pipe, new = sky_clock(EXE, 1)
    print(f"  INFO in 4 s: baseline {old}, new {new}")
    check(old["slicedFrames"] >= 20, f"the baseline does sky work on most frames: {old['slicedFrames']} (the premise of this check)")
    check(new["slicedFrames"] * 5 <= old["slicedFrames"], f"frames with sky GPU work: {old['slicedFrames']} -> {new['slicedFrames']}")
    check(new["lutSkyViewBuilds"] <= 2, f"sky-view LUT builds at 1x: {new['lutSkyViewBuilds']} (0.1 degree takes 24 s)")
    sun_a = sky_state(pipe)["sunDirection"]
    lut_a = sky_state(pipe)["refine"]["lutSunDirection"]
    check(sum((x - y) ** 2 for x, y in zip(sun_a, lut_a)) ** 0.5 <= 0.002, f"while it runs the LUT stays within 0.1 degrees: {lut_a} / {sun_a}")

    pipe.call("set_environment", key="time_playing", value=False)
    converged = wait_until(lambda: sky_state(pipe)["refine"]["state"].lower() == "converged", 30.0)
    state = sky_state(pipe)
    check(converged, f"a stopped clock converges: {state['refine']['state']}")
    check(state["refine"]["lutSunDirection"] == state["sunDirection"],
          f"it ends on the exact sun: LUT {state['refine']['lutSunDirection']}, sun {state['sunDirection']}")
    check(close(process, pipe) == 0, "no GL errors")

    print("[sky: a fast clock still follows]")
    process, pipe, fast = sky_clock(EXE, 3600)
    print(f"  INFO in 4 s at 3600x: {fast}")
    check(fast["lutSkyViewBuilds"] >= 20, f"sky-view LUT builds at 3600x: {fast['lutSkyViewBuilds']}")
    check(fast["fastJobs"] >= 2, f"IBL jobs at 3600x: {fast['fastJobs']}")
    check(close(process, pipe) == 0, "no GL errors")


# --- images --------------------------------------------------------------------------------------

def png_bytes(width, height, color):
    import io
    from PIL import Image
    buffer = io.BytesIO()
    Image.new("RGBA", (width, height), color).save(buffer, "PNG")
    return buffer.getvalue()


def image_fixture():
    """A cube with a used image (0) and an image no material uses (1)."""
    source = (REPO / "resources/sampleGltf/HelloCube.glb").read_bytes()
    json_length = struct.unpack_from("<I", source, 12)[0]
    doc = json.loads(source[20:20 + json_length])
    geometry = source[28 + json_length:][:doc["buffers"][0]["byteLength"]]
    doc["nodes"] = [{"mesh": 0}]
    doc["scenes"] = [{"nodes": [0]}]
    for key in ("cameras", "extensions", "extensionsUsed"):
        doc.pop(key, None)
    data = "data:image/png;base64,"
    doc["images"] = [{"uri": data + base64.b64encode(png_bytes(256, 256, (40, 200, 90, 255))).decode()},
                     {"uri": data + base64.b64encode(png_bytes(128, 64, (220, 40, 60, 255))).decode()}]
    doc["textures"] = [{"source": 0}, {"source": 1}]
    doc["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    doc["buffers"][0] = {"uri": "data:application/octet-stream;base64," + base64.b64encode(geometry).decode(),
                         "byteLength": len(geometry)}
    path = OUT / "image_fixture.gltf"
    path.write_text(json.dumps(doc), encoding="utf-8")
    return path


def capture_report(exe, model, label):
    png, report = OUT / f"{label}.png", OUT / f"{label}.json"
    subprocess.run([str(exe), str(model), "--isolated", "--window-size", "1280x720", "--vsync", "off",
                    "--fixed-frame-time", "0.05", "--capture-viewport", str(png), "--capture-frame", "12",
                    "--profile-report", str(report), "--exit-after-profile"],
                   cwd=REPO, capture_output=True, text=True, timeout=600)
    return png, json.loads(report.read_text(encoding="utf-8"))["startup"]


def preview_shot(exe, model, image, label):
    script, png = OUT / f"{label}.txt", OUT / f"{label}.png"
    script.write_text(f"wait 12\nshot {png.as_posix()}\nwait 2\n", encoding="utf-8")
    png.unlink(missing_ok=True)
    subprocess.run([str(exe), str(model), "--isolated", "--window-size", "1280x720", "--vsync", "off",
                    "--select-image", str(image), "--ui-script", str(script), "--ui-report", str(OUT / f"{label}.ui.json")],
                   cwd=REPO, capture_output=True, text=True, timeout=180)
    return png


def private_memory(exe, model):
    import psutil
    process, pipe = start_pipe_session([exe, model] + INTERACTIVE)
    try:
        assert wait_until(lambda: pipe.call("get_state").get("modelLoaded", True), 300)
        time.sleep(3.0)
        bytes_ = psutil.Process(process.pid).memory_info().private
        close(process, pipe)
        return bytes_
    except Exception:
        process.kill()
        raise


def check_images(baseline):
    print("[images: pixels leave the CPU heap, and nothing else changes]")
    model = image_fixture()
    new_png, new = capture_report(EXE, model, "fixture_new")
    old_png, old = capture_report(baseline, model, "fixture_baseline")
    check(new["decodedImageBytes"] == old["decodedImageBytes"] > 0, f"the decoded size is still reported: {new['decodedImageBytes']}")
    check(new["retainedDecodedImageBytes"] == 0, f"pixels kept after the upload: {new['retainedDecodedImageBytes']}")
    check(same(new_png, old_png), "the rendered image is unchanged")
    for image, name in ((0, "a used image"), (1, "an image no material uses")):
        a = preview_shot(EXE, model, image, f"preview{image}_new")
        b = preview_shot(baseline, model, image, f"preview{image}_baseline")
        check(a.exists() and b.exists() and same(a, b), f"the inspector shows {name} as before")

    if not E195.exists():
        print("  INFO the E195 cockpit is not here: the memory comparison is skipped")
        return
    from e195_assets import textured_copy
    copy = textured_copy(E195, OUT)
    new_png, new = capture_report(EXE, copy, "e195_new")
    old_png, old = capture_report(baseline, copy, "e195_baseline")
    check(new["retainedDecodedImageBytes"] == 0, f"E195: pixels kept after the upload: {new['retainedDecodedImageBytes']}")
    check(same(new_png, old_png), "E195: the rendered image is unchanged")
    print(f"  INFO E195: decoded {new['decodedImageBytes'] / 1048576:.0f} MiB, encoded {new['encodedImageBytes'] / 1048576:.0f} MiB, "
          f"upload {old['rendererUploadMs']:.0f} -> {new['rendererUploadMs']:.0f} ms")
    a, b = private_memory(baseline, copy), private_memory(EXE, copy)
    print(f"  INFO E195 private memory: baseline {a / 1048576:.0f} MiB, new {b / 1048576:.0f} MiB")
    check(b < a, f"E195: private memory falls: {a / 1048576:.0f} -> {b / 1048576:.0f} MiB")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", default=str(REPO / "build" / "baselines" / "59a8387.exe"))
    parser.add_argument("--only", default="ocean,loop,sky,images,panorama,traversal,filter")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    wanted = set(args.only.split(","))
    baseline = Path(args.baseline_exe)
    if "ocean" in wanted:
        check_ocean(baseline)
    if "loop" in wanted:
        check_loop(baseline)
    if "panorama" in wanted:
        check_panorama()
    if "traversal" in wanted:
        check_traversal()
    if "filter" in wanted:
        check_shadow_filter()
    if "sky" in wanted:
        check_sky(baseline)
    if "images" in wanted:
        check_images(baseline)
    finish()


if __name__ == "__main__":
    main()
