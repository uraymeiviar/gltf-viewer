# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Checks for the v1.2.0 review fixes (review-v120 reports). Serial only.

  * clock:  the playing time-of-day clock and the procedural sky's IBL swaps no longer resynchronise the model: the
            renderer sync count (get_state sync.syncs) stays flat while frames, the clock and the sky swaps advance;
            the same run renders byte-identical to the baseline (TAA, --sky-time-slicing off, so the run is
            deterministic on both binaries).
  * ssao:   closing a viewport frees its SSAO targets (get_state ssao.viewports[].memoryBytes 0, active false).
  * skyocc: isolating a node without sky-occlusion casters (a BLEND glass box) gives skyOcclusion.active false,
            noCasters true, no volume, and no bake-scene rebuild per frame (sceneBuilds flat); exit isolate bakes again.
  * tod:    pipe time_of_day_mode on, then get_environment sun / sun_source / sun_mode / time_of_day_mode, matches the
            Inspector path (sun_source clock, selectSunSource); the following "off" matches Sun Source Manual.
  * albedo: the sky's automatic ground albedo is the plane of the lowest-id viewport drawing one; focusing another
            viewport (here one showing the grid) neither changes it nor rebuilds the atmosphere LUTs.
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import validate_night as night  # noqa: E402
from shadow_fixtures import Builder  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same  # noqa: E402

OUT = REPO / "build" / "review_fixes"
CLOCK = ["--environment", "sky", "--time-of-day-mode", "on", "--time-preset", "afternoon", "--time-play", "on",
         "--time-scale", "600"]


def hull_and_glass():
    """An opaque open box (sky-occlusion caster) and a separate BLEND glass box beside it (never a caster)."""
    b = Builder("review fixes: hull and glass")
    hull = b.material("Hull", color=(0.8, 0.8, 0.8, 1.0))
    b.box("Floor", (-1.05, -0.05, -1.05), (1.05, 0.0, 1.05), hull)
    b.box("Back", (-1.05, 0.0, -1.05), (1.05, 2.0, -1.0), hull)
    b.box("Left", (-1.05, 0.0, -1.0), (-1.0, 2.0, 1.0), hull)
    glass = b.material("Glass", color=(0.6, 0.8, 1.0, 0.3), alpha_mode="BLEND")
    b.box("Glass", (1.5, 0.0, -0.5), (2.5, 1.0, 0.5), glass)
    return b.write(OUT / "hull_glass.gltf")


def settle(session, frames):
    session.call("capture", path=str(OUT / "settle.png"), settle_frames=frames)  # overlays=False would mark ViewportSettings


def check_clock(model, baseline):
    session = night.Session(model, CLOCK, common=night.COMMON)
    try:
        settle(session, 16)
        before = session.call("get_state")
        settle(session, 40)
        after = session.call("get_state")
        frames = after["frame"] - before["frame"]
        syncs = after["sync"]["syncs"] - before["sync"]["syncs"]
        swaps = after["sky"]["stats"]["swaps"] - before["sky"]["stats"]["swaps"]
        moved = after["timeOfDay"]["julianDay"] != before["timeOfDay"]["julianDay"]
        check(frames >= 40 and syncs == 0 and moved,
              f"clock playing: {frames} frames, clock {before['timeOfDay']['time']} -> {after['timeOfDay']['time']}, "
              f"renderer syncs +{syncs} (flat)")
        check(swaps > 0, f"clock playing: the time-sliced sky swapped its IBL {swaps} times without a renderer sync")
        check(session.close() == 0, "clock session: zero GL errors")
    except BaseException:
        session.process.kill()
        session.process.wait(30)
        raise
    # Same image as the baseline (which resynchronised every frame): TAA on, synchronous sky filtering, Manual exposure
    # (pre-exposure 1; with the physical Auto the TAA history is now rescaled on a pre-exposure change, finding 5).
    common = [flag for flag in night.COMMON]
    common[common.index("--aa") + 1] = "taa"
    flags = CLOCK + ["--sky-time-slicing", "off", "--exposure-mode", "manual"]
    now, report = night.render(model, "review_clock_now", flags, common=common, frame=48)
    ref, _ = night.render(model, "review_clock_ref", flags, exe=baseline, common=common, frame=48)
    check(now is not None and ref is not None and same(now, ref),
          f"clock playing, TAA, Manual exposure, 48 frames: byte-identical to {baseline.name}")
    # The first frames attach the procedural sky (Reattach: one RenderSettings sync); none after that.
    frames = (report or {}).get("frames", [])
    late = sum(f["counters"].get("updateGltfCalls", 0) for f in frames if f["frame"] > 3)
    changes = sorted({f["counters"].get("sceneChanges", "") for f in frames if f["frame"] > 3})
    check(report is not None and late == 0 and len(frames) > 40,
          f"clock playing: updateGltf calls after the sky attach {late} (0); frame changes {changes}")
    # Finding 5: the physical Auto steps its pre-exposure (0.25 -> 0.5 in this run); the TAA history follows it.
    _, auto = night.render(model, "review_clock_auto", CLOCK + ["--sky-time-slicing", "off"], common=common, frame=48)
    vp = (auto or {}).get("night", {}).get("exposure", {}).get("viewports", [{}])[0]
    check(vp.get("taaHistoryRescales", 0) >= 1,
          f"clock playing, Auto exposure: pre-exposure {vp.get('preExposure')}, TAA history rescaled "
          f"{vp.get('taaHistoryRescales')} time(s)")


def follow_script(model, name, flags, lines, timeout=300):
    """Runs Glider with --ui-script-follow and returns the parsed JSONL results."""
    script = OUT / f"{name}.txt"
    results = Path(str(script) + ".out.jsonl")
    results.unlink(missing_ok=True)
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    proc = subprocess.run([str(EXE), str(model)] + night.COMMON + list(flags) + ["--ui-script-follow", str(script)],
                          cwd=REPO, capture_output=True, text=True, timeout=timeout)
    out = []
    if results.exists():
        for line in results.read_text(encoding="utf-8").splitlines():
            if line.strip():
                out.append(json.loads(line))
    return proc, out


def states(results):
    found = []
    for entry in results:
        result = entry.get("result") if isinstance(entry, dict) else None
        if isinstance(result, dict) and "ssao" in result:
            found.append(result)
    return found


def check_ssao(model):
    _, results = follow_script(model, "ssao_close", ["--ssao", "on"],
                               ["wait 20", "get_state", "vpcreate", "wait 30", "get_state", "vptoggle 2", "wait 20", "get_state", "quit"])
    snaps = states(results)
    if len(snaps) != 3:
        check(False, f"SSAO close: expected 3 states, got {len(snaps)} ({results[-3:]})")
        return

    def vp(state, vid):
        return next((v for v in state["ssao"]["viewports"] if v["viewport"] == vid), None)

    open_vp2 = vp(snaps[1], 2)
    closed_vp2 = vp(snaps[2], 2)
    check(open_vp2 is not None and open_vp2["active"] and open_vp2["memoryBytes"] > 0,
          f"viewport 2 with SSAO: active, {open_vp2 and open_vp2['memoryBytes']} bytes")
    check(closed_vp2 is not None and not closed_vp2["active"] and closed_vp2["memoryBytes"] == 0,
          f"viewport 2 closed: SSAO memory {closed_vp2 and closed_vp2['memoryBytes']} bytes (0), active "
          f"{closed_vp2 and closed_vp2['active']}")
    check(snaps[2]["glErrors"] == 0, "SSAO close: zero GL errors")


def check_sky_occlusion(model):
    session = night.Session(model, ["--environment", "sky", "--sky-occlusion", "on", "--sky-time-slicing", "off"])
    try:
        settle(session, 24)
        state = session.call("get_state")["skyOcclusion"]
        check(state["active"] and state["upToDate"] and not state["noCasters"] and state["volume"] is not None,
              f"whole model: volume baked and applied (active {state['active']}, upToDate {state['upToDate']})")
        session.call("isolate", node="Glass")
        settle(session, 8)
        first = session.call("get_state")["skyOcclusion"]
        settle(session, 24)
        second = session.call("get_state")["skyOcclusion"]
        check(not first["active"] and first["noCasters"] and first["volume"] is None and not first["baking"],
              f"isolated glass (no casters): active {first['active']}, noCasters {first['noCasters']}, volume "
              f"{first['volume']}, baking {first['baking']}")
        check(second["sceneBuilds"] == first["sceneBuilds"] and not second["active"],
              f"isolated glass: bake-scene builds {first['sceneBuilds']} -> {second['sceneBuilds']} over 24 frames (flat)")
        session.call("exit_isolate")
        settle(session, 24)
        back = session.call("get_state")["skyOcclusion"]
        check(back["active"] and back["upToDate"] and not back["noCasters"],
              f"exit isolate: baked again (active {back['active']}, upToDate {back['upToDate']})")
        check(session.close() == 0, "sky occlusion session: zero GL errors")
    except BaseException:
        session.process.kill()
        session.process.wait(30)
        raise


def check_ground_albedo(model):
    """Finding 6/7: the sky's automatic ground albedo comes from the lowest-id viewport drawing a plane, not the focus."""
    lines = ["wait 20", "get_state", "vpcreate", "wait 10", "set_viewport_setting ground grid", "wait 20", "get_state",
             "vpfocus 1", "wait 20", "get_state", "vpfocus 2", "wait 20", "get_state", "quit"]
    _, results = follow_script(model, "ground_albedo", ["--environment", "sky", "--ground", "plane", "--ground-preset", "grass"],
                               lines)
    snaps = states(results)
    if len(snaps) != 4:
        check(False, f"ground albedo: expected 4 states, got {len(snaps)}")
        return
    albedos = [tuple(s["sky"]["groundAlbedo"]) for s in snaps]
    sources = [s["sky"]["groundAlbedoSource"] for s in snaps]
    builds = [s["sky"]["stats"]["lutAtmosphereBuilds"] for s in snaps]
    active = [s["activeViewport"] for s in snaps]
    check(len(set(albedos)) == 1 and set(sources) == {"plane"},
          f"ground albedo with viewports {active}: {sources}, {albedos[0]} in every state")
    check(builds[1] == builds[2] == builds[3],
          f"focus changes between the plane and the grid viewport: atmosphere LUT builds {builds[1:]} (no re-filter)")
    check(snaps[-1]["glErrors"] == 0, "ground albedo: zero GL errors")


SUN_KEYS = ("sun", "sun_source", "sun_mode", "time_of_day_mode")


def sun_settings(session):
    return {key: session.call("get_environment", key=key) for key in SUN_KEYS}


def check_time_of_day_mode(model):
    for environment in ("hdr", "sky"):
        results = {}
        for path, steps in (("pipe", [("time_of_day_mode", True), ("time_of_day_mode", False)]),
                            ("ui", [("sun_source", "clock"), ("sun_source", "manual")])):
            session = night.Session(model, ["--environment", environment, "--sky-time-slicing", "off"])
            try:
                snaps = []
                for key, value in steps:
                    session.env(key, value)
                    settle(session, 2)
                    snaps.append(sun_settings(session))
                results[path] = snaps
                check(session.close() == 0, f"{environment} {path}: zero GL errors")
            except BaseException:
                session.process.kill()
                session.process.wait(30)
                raise
        # The legacy "sun" key is the retained HDR enable: with the sky, the Inspector selects only the sky's source
        # while time_of_day_mode selects both environments (AGENTS.md --sun-source), so only HDR compares "sun".
        keys = ("sun", "sun_source") if environment == "hdr" else ("sun_source",)
        for index, label in ((0, "on = Sun Source Time of Day"), (1, "off = Sun Source Manual")):
            pipe = {key: results["pipe"][index][key]["value"] for key in keys}
            ui = {key: results["ui"][index][key]["value"] for key in keys}
            check(pipe == ui, f"{environment}: time_of_day_mode {label}: pipe {pipe}, Inspector path {ui}")
        check(results["pipe"][0]["sun_source"]["value"] == "clock", f"{environment}: time_of_day_mode on selects clock")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-exe", type=Path, required=True)
    parser.add_argument("--only", choices=("clock", "ssao", "skyocc", "tod", "albedo"))
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    night.OUT.mkdir(parents=True, exist_ok=True)
    model = hull_and_glass()
    if args.only in (None, "clock"):
        check_clock(model, args.baseline_exe)
    if args.only in (None, "ssao"):
        check_ssao(model)
    if args.only in (None, "skyocc"):
        check_sky_occlusion(model)
    if args.only in (None, "albedo"):
        check_ground_albedo(model)
    if args.only in (None, "tod"):
        check_time_of_day_mode(model)
    finish()


if __name__ == "__main__":
    main()
