# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Three settings layers and vendor tuning (docs/SETTINGS_SCOPES.md).

Outside the restricted build sandbox, run one Glider at a time. All launches are isolated
and use the harness no-focus behavior except explicit GLIDER_DATA_DIR persistence tests.

baseline: exact captures for default / legacy flags (Low's removed background gate is
compared with an explicit equivalent background-off scene).
pipe: canonical + compatibility CLI/pipe image equality and registry ranges.
overrides: Quality off levels, Shaded vs Shaded+post and deprecated warned no-ops.
viewports: shared ground, height, actual single ocean allocation/time, independent grid/shading.
ui: all layer panels and presets, conditional controls, gear/status, vendor tuning, ID conflicts;
screenshots in build/review/settings_layers.
persistence: old gates + budgets, canonical precedence, save/restart, session-only flags,
default pruning, no persisted scene/viewport state, --isolated writes nothing.
extensions: extension switches, compatibility scopes, UI commits and persistence rules.

Usage: python scripts/validate_settings_scopes.py --baseline-exe build/baselines/7f9ff37.exe
       [--only baseline,pipe,overrides,viewports,ui,persistence,extensions]
"""

import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from glider_harness import connect_pipe  # noqa: E402
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import validate_agent_control as agent  # noqa: E402  (quad fixture with a windshield_v3 quad)
import validate_asobo_glass as glass  # noqa: E402  (SSR fixture: a windshield_v3 pane reflecting an emissive box)

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "settings_scopes"
REVIEW = REPO / "build" / "review" / "settings_layers"
COMMON = ["--isolated", "--window-size", "1600x900", "--hide-grid", "--fixed-frame-time", "0.05", "--vsync", "off"]
QUAD_VIEW = agent.VIEW
# HelloCube (the embedded startup model) on a ground plane under a low sun: shadow and SSAO contact both visible.
SUN = ["--ground", "plane", "--sun", "on", "--sun-dir", "135,35", "--camera-yaw", "30", "--camera-pitch", "25"]
# The SSR fixture's view (validate_asobo_glass.py render()).
SSR_VIEW = ["--clear-color", "0.015,0.02,0.025", "--exposure", "0", "--camera-target", "0,0,0", "--camera-yaw", "0",
            "--camera-pitch", "0", "--camera-distance", "4"]
# CLI = pipe comparisons render without TAA (a pipe capture settles from a different history than a CLI frame 8).
NO_TAA = ["--aa", "none"]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label, flush=True)
    if not condition:
        failures.append(label)
    return condition


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() if path and Path(path).exists() else None


def same(a, b):
    return digest(a) is not None and digest(a) == digest(b)


def model_args(model):
    return [str(model)] if model else []


def cli_capture(exe, model, name, flags, frame=8, show_grid=False, window=None):
    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    common = [arg for arg in COMMON if arg != "--hide-grid"] if show_grid else list(COMMON)
    if window:
        common[common.index("--window-size") + 1] = window
    result = subprocess.run([str(exe)] + model_args(model) + common + list(flags) +
                            ["--capture-viewport", str(png), "--capture-frame", str(frame),
                             "--profile-report", str(OUT / f"{name}.json"), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        print(result.stdout[-1500:], result.stderr[-1500:])
    return png if png.exists() else None


class Session:
    """A Glider with --control-pipe and a connected client."""

    def __init__(self, model, flags=(), env_extra=None, isolated=True):
        common = COMMON if isolated else [a for a in COMMON if a != "--isolated"]
        args = [str(EXE)] + model_args(model) + common + list(flags) + ["--control-pipe"]
        env = os.environ.copy()
        env.update(env_extra or {})
        self.log = open(OUT / f"session_{time.time_ns()}.log", "w", encoding="utf-8")
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL,
                                        stdout=self.log, stderr=self.log)
        print(f"    session pid={self.process.pid}",flush=True)
        # connect_pipe retries while the process lives. No os.path.exists poll on the pipe first: every
        # stat opens and closes a client connection, which used to kill Glider's listening instance.
        try:
            self.pipe = connect_pipe(self.process)
        except BaseException:
            self.process.kill()
            raise

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def request(self, cmd, **args):
        return self.pipe.request(cmd, args)

    def vset(self, key, value):
        return self.call("set_viewport_setting", key=key, value=value)

    def eset(self, key, value):
        return self.call("set_environment", key=key, value=value)

    def capture(self, name):
        path = OUT / f"{name}.png"
        path.unlink(missing_ok=True)
        self.call("capture", path=str(path))
        return path if path.exists() else None

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:  # noqa: BLE001
                self.process.kill()
        self.log.close()
        return errors


def follow(model, name, flags, lines, timeout=300, env_extra=None):
    """--ui-script-follow with every line written in advance; returns the parsed JSONL results."""
    OUT.mkdir(parents=True, exist_ok=True)
    script = OUT / f"{name}.txt"
    results = Path(str(script) + ".out.jsonl")
    results.unlink(missing_ok=True)
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    env = os.environ.copy()
    env.update(env_extra or {})
    proc = subprocess.run([str(EXE)] + model_args(model) + COMMON + list(flags) + ["--ui-script-follow", str(script)],
                          cwd=REPO, capture_output=True, text=True, timeout=timeout, env=env)
    rows = []
    if results.exists():
        rows = [json.loads(line) for line in results.read_text(encoding="utf-8").splitlines() if line.strip()]
    return proc, rows


# ---------------------------------------------------------------------------------------------

PRESET_TOOLBAR_CASES = {"preset_low", "preset_ultra"}


def check_baseline(baseline, quads, ssr):
    print(f"[defaults and legacy flags: current build == baseline {baseline.name}]")
    cases = {
        "default_cube": (None, []),
        "default_startup_grid": (None, []),
        "default_quads": (quads, QUAD_VIEW),
        "sun_ground": (None, SUN),
        "aa_msaa4": (None, ["--aa", "msaa4"]),
        "aa_none": (None, ["--aa", "none"]),
        "msaa_post": (None, ["--msaa", "4", "--post-aa", "fxaa"]),
        "aa_then_msaa": (None, ["--msaa", "2", "--aa", "fxaa"]),
        "ssr_flags": (quads, QUAD_VIEW + ["--ssr", "off", "--ssr-quality", "high", "--interior-sky-reflection", "0.25",
                                          "--windshield-icing", "0.5"]),
        "ssr_quality": (quads, QUAD_VIEW + ["--ssr-quality", "high", "--interior-sky-reflection", "0.1"]),
        "ssr_pane": (ssr, SSR_VIEW),
        "ssr_pane_off": (ssr, SSR_VIEW + ["--ssr", "off"]),
        "ssr_pane_look": (ssr, SSR_VIEW + ["--ssr-quality", "low", "--interior-sky-reflection", "0", "--windshield-icing", "0.4",
                                           "--msaa", "4", "--post-aa", "fxaa"]),
        "decal": (quads, QUAD_VIEW + ["--decal-tolerance", "2", "--sky-portal-strength", "1.5"]),
        "ssao": (None, SUN + ["--ssao", "on", "--ssao-radius", "0.8", "--ssao-intensity", "1.5", "--ssao-quality", "high",
                              "--ssao-resolution", "half"]),
        "ssao_default": (None, SUN + ["--ssao", "on"]),
        "shadows_off": (None, SUN + ["--shadows", "off"]),
        "shadow_tuning": (None, SUN + ["--shadow-filter", "soft", "--shadow-resolution", "4096", "--shadow-cascades", "4",
                                       "--shadow-normal-bias", "2", "--shadow-split-lambda", "0.5"]),
        "sky_occlusion": (None, ["--environment", "sky", "--sky-occlusion", "on", "--sky-occlusion-quality", "low",
                                 "--sky-time-slicing", "off", "--aa", "none"]),
        "ocean": (None, ["--ground", "ocean", "--ocean-quality", "low", "--ocean-time", "3", "--aa", "none"]),
        "preset_low": (None, ["--quality-preset", "low"]),
        "preset_ultra": (quads, QUAD_VIEW + ["--quality-preset", "ultra"]),
        # glTF extension switches (now an app setting): the same flags render like the baseline's per-viewport switches.
        "asobo_off_ssr": (ssr, SSR_VIEW + ["--asobo", "off"]),
        "asobo_disable_ssr": (ssr, SSR_VIEW + ["--asobo-disable", "ASOBO_material_windshield_v3"]),
        "asobo_off_quads": (quads, QUAD_VIEW + ["--asobo", "off"]),
        "asobo_disable_decal": (quads, QUAD_VIEW + ["--asobo-disable", "ASOBO_material_geometry_decal"]),
        "asobo_disable_two": (quads, QUAD_VIEW + ["--asobo-disable", "ASOBO_material_geometry_decal", "--asobo-disable",
                                                  "ASOBO_material_blend_gbuffer"]),
        "asobo_invisible": (quads, QUAD_VIEW + ["--show-invisible-materials"]),
    }
    for name, (model, flags) in cases.items():
        current_flags = flags + (["--clear-color", "0.12,0.12,0.14"] if name == "preset_low" else [])
        # With an explicit preset the baseline drew its Quality combo in the viewport toolbar row
        # (7f9ff37 Viewports.cpp:224), 4 px taller than the small buttons: its viewport is 918x792. The
        # combo now lives in the status bar, so the current build gets the same 918x792 viewport from a
        # 4 px shorter window; the byte comparison is then of the same 3D image at the same size.
        window = "1600x896" if name in PRESET_TOOLBAR_CASES else None
        current = cli_capture(EXE, model, f"base_cur_{name}", current_flags, show_grid=name == "default_startup_grid",
                              window=window)
        reference = cli_capture(baseline, model, f"base_ref_{name}", flags, show_grid=name == "default_startup_grid")
        check(current is not None and same(current, reference), f"{name}: {' '.join(flags) or '(no flags)'}")
        if name in {"default_cube", "default_startup_grid", "preset_ultra", "preset_low"}:
            REVIEW.mkdir(parents=True,exist_ok=True)
            if current: shutil.copyfile(current, REVIEW/f"{name}_after.png")
            if reference: shutil.copyfile(reference, REVIEW/f"{name}_before.png")
        if name == "preset_low":
            visible_background = cli_capture(EXE,model,"low_background_scene_on",flags,window=window)
            check(visible_background is not None and not same(visible_background,reference),
                  "Low no longer gates scene background; explicit background off still equals the old image exactly")
            if visible_background: shutil.copyfile(visible_background,REVIEW/"low_background_gate_removed.png")
    bad = subprocess.run([str(EXE), "--isolated", "--shadows", "maybe", "--dump-frame", str(OUT / "bad.png"),
                          "--exit-after-dump"], cwd=REPO, capture_output=True, text=True, timeout=120)
    check(bad.returncode != 0, f"--shadows maybe is refused (exit {bad.returncode})")


def check_cli_equals_pipe(ssr):
    print("[CLI = pipe: canonical keys, compatibility keys, viewport overrides]")
    cli = {
        "plain": cli_capture(EXE, ssr, "cli_plain", NO_TAA + SSR_VIEW),
        "ssr_off": cli_capture(EXE, ssr, "cli_ssr_off", NO_TAA + SSR_VIEW + ["--ssr", "off"]),
        "ssr_look": cli_capture(EXE, ssr, "cli_ssr_look", NO_TAA + SSR_VIEW + ["--ssr-quality", "high", "--interior-sky-reflection",
                                                                         "0.1", "--windshield-icing", "0.6"]),
        "aa": cli_capture(EXE, ssr, "cli_aa", NO_TAA + SSR_VIEW + ["--msaa", "4", "--post-aa", "fxaa"]),
    }
    check(not same(cli["plain"], cli["ssr_off"]) and not same(cli["plain"], cli["ssr_look"]) and not same(cli["plain"], cli["aa"]),
          "the SSR fixture shows --ssr off, the SSR look and the AA flags")
    session = Session(ssr, NO_TAA + SSR_VIEW)
    pipe = {}
    try:
        pipe["plain"] = session.capture("pipe_plain")
        session.eset("ssr", False)
        pipe["ssr_env"] = session.capture("pipe_ssr_env")
        session.eset("ssr", True)
        session.vset("viewport_ssr", "off")
        unchanged = session.capture("pipe_ssr_noop")
        check(same(cli["plain"], unchanged), "deprecated viewport_ssr off is a warned no-op")
        session.eset("ssr", False)
        pipe["ssr_override"] = session.capture("pipe_ssr_override")
        check(session.call("get_viewport_setting", key="ssr")["value"] is False, "get_viewport_setting ssr = false under viewport_ssr off")
        session.vset("viewport_ssr", "scene")
        session.vset("ssr", False)  # compatibility key: this viewport's override Off
        pipe["ssr_compat"] = session.capture("pipe_ssr_compat")
        check(session.call("get_viewport_setting", key="viewport_ssr")["value"] == "scene",
              "compatibility SSR sets global Quality; deprecated override stays scene")
        session.vset("viewport_ssr", "scene")
        session.eset("ssr", True)
        pipe["plain_again"] = session.capture("pipe_plain_again")
        # SSR look and quality through the compatibility keys in their former (viewport) scope.
        session.vset("ssr_quality", "high")
        session.vset("interior_sky_reflection", 0.1)
        session.vset("windshield_icing", 0.6)
        pipe["ssr_look_compat"] = session.capture("pipe_ssr_look_compat")
        env = session.call("get_environment", key="interior_sky_reflection")["value"]
        check(abs(env - 0.1) < 1e-6, f"set_viewport_setting interior_sky_reflection reaches the scene ({env})")
        session.vset("ssr_quality", "medium")
        session.vset("interior_sky_reflection", 0.5)
        session.vset("windshield_icing", 0.0)
        # The same through the canonical scopes.
        session.vset("ssr_quality", "high")  # renderer key (set_viewport_setting falls back to renderer keys)
        session.eset("interior_sky_reflection", 0.1)
        session.eset("windshield_icing", 0.6)
        pipe["ssr_look_env"] = session.capture("pipe_ssr_look_env")
        session.vset("ssr_quality", "medium")
        session.eset("interior_sky_reflection", 0.5)
        session.eset("windshield_icing", 0.0)
        session.vset("msaa", "4")
        session.vset("post_aa", "fxaa")
        pipe["aa"] = session.capture("pipe_aa")
        state = session.call("get_state")
        check(state["quality"]["configured"]["msaa"] == "4" and state["quality"]["configured"]["postAA"] == "fxaa",
              f"get_state quality.configured carries the AA ({state['quality']['configured']['msaa']}, "
              f"{state['quality']['configured']['postAA']})")
        session.vset("aa", "none")
        check(session.call("get_viewport_setting", key="aa")["value"] == "none" and
              session.call("get_viewport_setting", key="msaa")["value"] == "0", "aa none sets msaa 0 + post AA none")
        bad = session.request("set_viewport_setting", key="interior_sky_reflection", value=3)
        check(not bad["ok"] and bad["error"]["code"] == "bad_value", "the compatibility key keeps the range (3 refused)")
        bad = session.request("set_viewport_setting", key="viewport_ssao", value="maybe")
        check(not bad["ok"], "viewport_ssao maybe is refused")
        listed = session.call("list_settings")["settings"]
        by_scope = {}
        for entry in listed:
            by_scope.setdefault(entry["scope"], set()).add(entry["key"])
        check({"viewport_shadows", "viewport_ssr", "viewport_ssao"} <= by_scope.get("viewport", set()),
              "list_settings: viewport_shadows, viewport_ssr, viewport_ssao (viewport)")
        check({"shadows", "ssr", "ssao", "ssao_radius", "ssao_intensity", "interior_sky_reflection", "windshield_icing",
               "sky_portal_strength", "decal_tolerance_mm"} <= by_scope.get("environment", set()),
              "list_settings: the scene effects (environment)")
        check({"aa", "msaa", "post_aa", "ssr_quality", "ssao_quality", "ssao_resolution", "sky_occlusion_quality",
               "ocean_quality", "shadows_allowed"} <= by_scope.get("renderer", set()),
              "list_settings: the global quality (renderer)")
        check({"aa", "msaa", "post_aa", "ssr", "ssr_quality", "ssao", "ssao_radius", "ssao_quality", "interior_sky_reflection",
               "windshield_icing", "decal_tolerance_mm", "ocean_quality"} <= by_scope.get("viewport", set())
              and "shadows" in by_scope.get("renderer", set()) and "sky_occlusion_quality" in by_scope.get("environment", set()),
              "list_settings: every former key still listed in its former scope (compatibility)")
        errors = session.close()
        check(errors == 0, f"pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"pipe session failed: {error}")
        session.process.kill()
        return
    check(same(cli["plain"], pipe["plain"]) and same(cli["plain"], pipe["plain_again"]), "pipe plain == CLI plain (and again)")
    for name in ("ssr_env", "ssr_override", "ssr_compat"):
        check(same(cli["ssr_off"], pipe[name]), f"pipe {name} == CLI --ssr off")
    check(same(cli["ssr_look"], pipe["ssr_look_compat"]), "compatibility viewport keys == CLI SSR quality / interior / icing")
    check(same(cli["ssr_look"], pipe["ssr_look_env"]), "canonical renderer / scene keys == CLI SSR quality / interior / icing")
    check(same(cli["aa"], pipe["aa"]), "msaa 4 + post_aa fxaa over the pipe == CLI --msaa 4 --post-aa fxaa")
    script = None
    _, rows = follow(ssr, "script_ssr", NO_TAA + SSR_VIEW, ["wait 4", "set_environment ssr false",
                                                    f"capture {(OUT / 'script_ssr.png').as_posix()}", "quit"])
    script = OUT / "script_ssr.png"
    check(same(cli["ssr_off"], script), "--ui-script set_environment ssr false == CLI --ssr off")


def check_overrides():
    print("[Quality levels off; Shaded omits post effects and keeps shadows; compatibility no-ops]")
    cli = {name: cli_capture(EXE, None, "layers_" + name, NO_TAA + SUN + flags) for name, flags in {
        "plain": [], "ao": ["--ssao", "on"], "noshadow": ["--shadows", "off"], "no_post": ["--bloom", "off", "--ssr", "off", "--ssao", "off"]}.items()}
    session = Session(None, NO_TAA + SUN)
    try:
        session.vset("ssao_level", "mid")
        check(same(cli["ao"], session.capture("level_ao")), "SSAO mid equals legacy --ssao on")
        session.vset("ssao_level", "off")
        check(same(cli["plain"], session.capture("level_ao_off")), "SSAO off byte-identical to default off")
        session.call("set_persist", enabled=True)
        session.vset("shadow_map", "off")
        check(same(cli["noshadow"], session.capture("level_shadow_off")), "shadow map off equals --shadows off")
        session.vset("shadow_map", "2048")
        for key in ("viewport_shadows", "viewport_ssr", "viewport_ssao"):
            session.vset(key, "off")
        check(same(cli["plain"], session.capture("legacy_noops")), "all deprecated overrides leave the global effects unchanged")
        session.vset("ssao_level", "high")
        session.vset("reflections", "high")
        session.vset("lens_flare", "both")
        session.vset("post_aa", "fxaa")
        session.vset("render_mode", "shaded")
        state = session.call("get_state")
        effective = state["viewports"][0]["effective"]
        check(not effective["ssr"] and not effective["ssao"] and not effective["bloom"] and effective["lensFlare"] == 0
              and effective["postAA"] == "none" and effective["shadows"], "Shaded disables all five post effects and retains shadows")
        check(same(cli["no_post"], session.capture("shaded")), "Shaded equals the same scene with those post effects disabled")
        session.vset("render_mode", "shaded_post")
        state = session.call("get_state")
        check(state["viewports"][0]["postEffects"] and state["viewports"][0]["effective"]["ssao"], "Shaded + post restores the configured effects")
        session.vset("reflections", "off")
        session.vset("ssao_level", "off")
        session.vset("ibl_cube", "off")
        state = session.call("get_state")
        check(state["quality"]["configured"]["iblCube"] == 0 and not state["viewports"][0]["effective"]["ssr"]
              and not state["viewports"][0]["effective"]["ssao"], "levels off disable IBL, SSR and SSAO")
        check(session.close() == 0, "layer session: zero GL errors")
    except Exception as error:
        check(False, f"layer session failed: {error}")
        session.process.kill(); session.process.wait()


def check_two_viewports():
    print("[two viewports: shared scene ground/ocean, global Quality, independent Shaded and grid]")
    lines = ["wait 10", "set_viewport_setting grid true", "vpcreate", "wait 6", "vpsplit 2", "wait 10", "get_state",
             "set_viewport_setting msaa 4", "set_viewport_setting ssao_level high", "wait 6", "get_state",
             "set_viewport_setting ground plane", "set_viewport_setting ground_height_mode custom",
             "set_viewport_setting ground_height 1.25", "wait 6", "get_state",
             "set_viewport_setting ground ocean", "set_viewport_setting ocean_quality low",
             "set_viewport_setting ocean_mode live", "set_viewport_setting ocean_time 2", "wait 10", "get_state",
             "set_viewport_setting render_mode shaded", "set_viewport_setting grid false", "wait 6", "get_state", "quit"]
    proc, rows = follow(None, "two_viewports_layers", SUN, lines)
    states = [r["result"] for r in rows if r.get("cmd") == "get_state" and r.get("ok")]
    if not check(proc.returncode == 0 and len(states) == 5, f"five two-viewport states, exit {proc.returncode}"): return
    s0,s1,s2,s3,s4 = states
    check(len(s0["viewports"]) == 2 and all(v["effective"]["postAA"] == "taa" for v in s0["viewports"]), "new views share global TAA")
    check(all(v["effective"]["msaa"] == "msaa4" and v["effective"]["ssao"] for v in s1["viewports"]), "Quality edit reaches both views")
    check(all(v["ground"]["mode"] == "plane" and abs(v["ground"]["height"]-1.25) < 1e-6 for v in s2["viewports"]), "both views share plane and custom height")
    check(s3["environment"]["oceanSimulationShared"] and all(v["ground"]["ocean"]["active"] for v in s3["viewports"]), "both views draw one shared scene ocean")
    oceans = [v["ground"]["ocean"] for v in s3["viewports"]]
    check(oceans[0]["time"] == oceans[1]["time"] and oceans[0]["memoryBytes"] == oceans[1]["memoryBytes"], "ocean time and simulation allocation are shared")
    views = sorted(s4["viewports"], key=lambda v:v["id"])
    check(views[0]["effective"]["ssao"] and not views[1]["effective"]["ssao"] and views[0]["postEffects"] and not views[1]["postEffects"], "Shaded is per view; other viewport keeps post effects")
    check(all(state["glErrors"] == 0 for state in states), "two-view layer script: zero GL errors")
    check(views[0]["ground"]["gridVisible"] and not views[1]["ground"]["gridVisible"], "grid overlay visibility is independent per view")
    check(s3["environment"]["oceanSimulationShared"] and s3["environment"]["oceanSimulationCount"] == 1,
          "two views reference the same non-null ocean simulation, not equal-sized duplicate allocations")


def run_ui_script(model, name, commands, extra=(), env_extra=None, timeout=300):
    OUT.mkdir(parents=True, exist_ok=True)
    script, report = OUT / f"{name}.txt", OUT / f"{name}.json"
    script.write_text("\n".join(commands) + "\n", encoding="utf-8")
    report.unlink(missing_ok=True)
    env = os.environ.copy()
    env.update(env_extra or {})
    result = subprocess.run([str(EXE)] + model_args(model) + ["--window-size", "1920x1080", "--fixed-frame-time", "0.05",
                            "--aa", "none", "--vsync", "off"] + list(extra) +
                            ["--ui-script", str(script), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=timeout, env=env)
    (OUT / f"{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    return result, data


def check_ui(quads):
    print("[UI: every layer, preset tabs, gear/status combo, vendor tuning; ImGui conflicts]")
    REVIEW.mkdir(parents=True,exist_ok=True)
    shot=lambda name:f"shot {(REVIEW/(name+'.png')).as_posix()}"
    commands=["wait 24","clickp vp.toolbar.settings 1","wait 8","snap viewport_render",shot("viewport_settings"),
              "hoverp vp.render_mode 0","wait 3","snap render_hover","hoverp vp.toolbar.settings 1","wait 3","snap gear_hover",
              shot("toolbar_gear"),"hoverp status.quality 0","wait 3","snap status_hover",shot("status_quality_combo"),
              "clickp vp.tab.camera 0","wait 6","snap viewport_camera",shot("viewport_camera"),
              "clickp vp.tab.render 0","wait 6","clickp vp.open_quality 0","wait 8","snap quality_presets",shot("quality_presets")]
    for i in range(1,5):
        commands += ["scroll QualityPage top",f"clickp quality.tab {i}","wait 6",f"hoverp quality.reset {i}","wait 3",f"snap preset_{i}"]
    commands += ["scroll QualityPage top","clickp quality.summary 0","clickp settings.quality.current 0","wait 6",
                 "hoverp quality.ibl_cube 0","wait 3","snap ibl_level_hover",shot("quality_current")]
    # Actual off-level UI edits must hide dependent controls, not merely disable them.
    commands += ["clickp quality.ibl_cube 0","wait 3","clickp quality.ibl.option 0","wait 6","snap ibl_ui_off",
                 "clickp quality.ibl_cube 0","wait 3","clickp quality.ibl.option 1024","wait 6","snap ibl_ui_on"]
    for fraction in ("top","0.25","0.5","0.75","bottom"):
        commands += [f"scroll QualityPage {fraction}","wait 6",f"snap quality_current_{fraction}",shot("quality_current_"+fraction)]
    commands += ["set_viewport_setting ssao_level high", "scroll QualityPage 0.5", "wait 6",
                 "hoverp quality.ssao_resolution 0", "wait 3", "snap ao_ui_on",
                 "set_viewport_setting ssao_level off", "wait 6", "snap ao_ui_off",
                 "scroll QualityPage bottom", "wait 6", "hoverp quality.shadow.details 0", "wait 3", "snap shadow_ui_on",
                 "set_viewport_setting shadow_map off", "wait 6", "snap shadow_ui_off",
                 "set_viewport_setting shadow_map 2048", "wait 6"]
    commands += ["clickp settings.tab.extensions 0","wait 8","scroll ExtensionsPage top",
                 "hoverp settings.ext.interior_sky_reflection 0","wait 3","snap vendor_reflection",
                 "hoverp settings.ext.decal_tolerance 0","wait 3","snap vendor_decal",shot("gltf_extensions_tuning"),
                 "scroll ExtensionsPage bottom","wait 6","snap vendor_switches",shot("gltf_extensions_switches"),
                 "clickp settings.ext.close 0","wait 6","clickp hier.scene 0","wait 6","snap scene_top",shot("scene_inspector"),
                 "clickp scene.environment 0","clickp scene.sun_time 0","wait 6","set_environment ground plane","wait 6"]
    for tag in ("type","height","material","albedo","roughness","detail","detail_size","joints","haze"):
        commands += [f"hoverp scene.ground.{tag} 0","wait 3",f"snap plane_{tag}"]
    commands += [shot("scene_ground_plane"),"set_environment ground ocean","set_viewport_setting ocean_quality low","wait 10"]
    for tag in ("wind_speed","wind_dir","choppiness","swell","foam","color","period","fetch","reset"):
        commands += [f"hoverp scene.ocean.{tag} 0","wait 3",f"snap ocean_{tag}"]
    commands += [shot("scene_ground_ocean"),"set_environment ground off","wait 6",
                 "hoverp scene.effects.ssao_radius 0","wait 3","snap ao_radius","hoverp scene.effects.ssao_intensity 0",
                 "wait 3","snap ao_intensity","hoverp scene.effects.icing 0","wait 3","snap icing",
                 "clickp scene.sky 0","wait 6","hoverp scene.sky.refraction 0","wait 3","snap refraction",shot("scene_atmosphere"),"snap final"]
    result,data=run_ui_script(quads,"ui_layers",commands,extra=["--isolated","--show-telemetry"])
    snaps=data.get("snaps",[])
    check(result.returncode==0 and not data.get("failed",True) and not data.get("errors"),f"layer UI script completed ({data.get('errors')})")
    conflicts=[snap["label"] for snap in snaps if snap.get("idConflict",-1)!=0 or snap.get("hoveredIdCount",0)>1]
    check(not conflicts,f"no ImGui ID conflict on layer controls {conflicts}")
    check(len(snaps)>=40,f"all layer panels exercised ({len(snaps)} snapshots)")
    check(max((snap["glErrors"] for snap in snaps),default=-1)==0,"layer UI: zero GL errors")
    by_label={snap["label"]:snap for snap in snaps}
    tags=lambda label:{p["tag"] for p in by_label.get(label,{}).get("probes",[])}
    check("vp.render_mode" in tags("viewport_render") and not any(t.startswith("vp.override.") for t in tags("viewport_render")),"viewport shows render mode and no Scene/Off/On controls")
    check("quality.ibl.details" not in tags("ibl_ui_off") and "quality.ibl.details" in tags("ibl_ui_on"),
          "IBL off hides dependent filter controls, non-off restores them")
    check("quality.ssao_resolution" in tags("ao_ui_on") and "quality.ssao_resolution" not in tags("ao_ui_off"),
          "SSAO off hides resolution, non-off restores it")
    check("quality.shadow.details" in tags("shadow_ui_on") and "quality.shadow.details" not in tags("shadow_ui_off"),
          "shadow map off hides cascade/tuning controls")
    check("vp.tab.post" not in tags("viewport_render") and "vp.ground.type" not in tags("viewport_camera"),"viewport has no lens tab or ground appearance")
    check({"settings.ext.interior_sky_reflection","settings.ext.decal_tolerance"}<=tags("vendor_reflection"),"ASOBO tuning appears with its vendor group")
    for filename in ("viewport_settings","toolbar_gear","status_quality_combo","quality_presets","quality_current","scene_inspector","scene_ground_plane","scene_ground_ocean","gltf_extensions_tuning"):
        check((REVIEW/(filename+".png")).exists(),f"review screenshot {filename}.png")


def check_persistence(quads):
    print("[Quality and ASOBO tuning migration/persistence; CLI session isolation; default pruning]")
    data_dir = OUT / "persistence_layers"
    if data_dir.exists(): shutil.rmtree(data_dir)
    data_dir.mkdir(parents=True)
    env = {"GLIDER_DATA_DIR": str(data_dir)}
    ini = data_dir / "settings.ini"
    _,rows = follow(quads,"layers_isolated",QUAD_VIEW,["wait 4","set_persist true","set_viewport_setting reflections off",
        "set_viewport_setting decal_tolerance_mm 2.5","quit"],env_extra=env)
    check(rows and all(r.get("ok") for r in rows) and not ini.exists(), "--isolated writes no persistent layer state")
    ini.write_text("; Glider settings file (schema 1)\n[RenderQuality]\nshadows=0\nshadowResolution=4096\n"
                   "[QualityProfiles]\nselection=2\nprofile1.ssr=0\nprofile1.ssrQuality=2\nprofile1.ibl=0\nprofile1.bloom=1\nprofile1.ssaoQuality=2\n",encoding="utf-8")
    # QUAD_VIEW carries --bloom off, which is a Quality session override now; the migration session leaves
    # it out so get_state shows the migrated profile's bloom rather than the CLI override.
    migration_view = [a for i, a in enumerate(QUAD_VIEW) if a != "--bloom" and (i == 0 or QUAD_VIEW[i - 1] != "--bloom")]
    session = Session(quads,migration_view,env_extra=env,isolated=False)
    try:
        q = session.call("get_state")["quality"]["configured"]
        check(q["shadowMap"] == 0 and q["shadowResolution"] == 4096 and q["reflections"] == "off" and q["ssrQuality"] == "high"
              and q["iblCube"] == 0 and q["bloom"], "old gates + budgets migrate without losing choices")
        before = ini.read_text(encoding="utf-8")
        session.vset("reflections","high")
        check(ini.read_text(encoding="utf-8") == before, "session edit does not save")
        session.call("set_persist",enabled=True)
        session.vset("ssao_level","high")
        session.vset("msaa","0")
        session.vset("interior_sky_reflection",0.2)
        session.vset("decal_tolerance_mm",2.5)
        check(session.close() == 0, "migration session: zero GL errors")
        saved = ini.read_text(encoding="utf-8")
        check("msaaIdx=0" in saved, "MSAA Off is persisted when it overrides the selected preset's nonzero MSAA")
        check("[GltfExtensions]" in saved and "decal_tolerance_mm=2.5" in saved and "ssaoEnabled=1" in saved, "only canonical non-default values saved")
        session = Session(quads,QUAD_VIEW,env_extra=env,isolated=False)
        q = session.call("get_state")["quality"]["configured"]
        check(q["reflections"] == "off" and q["ssaoLevel"] == "high" and q["shadowMap"] == 0, "restart restores persisted Quality, not session SSR edit")
        check(abs(session.call("get_viewport_setting",key="interior_sky_reflection")["value"]-0.2)<1e-6 and
              session.call("get_viewport_setting",key="decal_tolerance_mm")["value"] == 2.5, "ASOBO tuning survives restart")
        session.close()
        session = Session(quads,QUAD_VIEW+["--ssr","on","--decal-tolerance","3"],env_extra=env,isolated=False)
        session.call("set_persist",enabled=True)
        session.vset("lens_flare_intensity",2)
        session.close()
        session = Session(quads,QUAD_VIEW,env_extra=env,isolated=False)
        q=session.call("get_state")["quality"]["configured"]
        check(q["reflections"] == "off" and q["lensFlareIntensity"] == 2 and
              session.call("get_viewport_setting",key="decal_tolerance_mm")["value"] == 2.5,
              "unrelated save never persists CLI SSR/vendor overrides")
        session.call("set_persist",enabled=True)
        for key,value in (("ssao_level","off"),("interior_sky_reflection",0.5),("decal_tolerance_mm",1.2),("lens_flare_intensity",1)):
            session.vset(key,value)
        session.eset("ground","plane")
        session.eset("ssao_radius",0.7)
        session.close()
        text=ini.read_text(encoding="utf-8")
        check("[GltfExtensions]" not in text and "ssaoEnabled=" not in text and "lensFlareIntensity=" not in text,
              "default values are pruned; vendor section becomes empty and is removed")
        check("ground=" not in text and "ssaoRadius=" not in text, "scene surface and look values are never saved")
        session = Session(quads,QUAD_VIEW,env_extra=env,isolated=False)
        check(session.call("get_state")["environment"]["ground"]["mode"] == "off" and
              abs(session.call("get_environment",key="ssao_radius")["value"]-0.5)<1e-6, "scene ground/AO appearance reset after restart")
        session.close()
        precedence = data_dir / "precedence"
        precedence.mkdir()
        (precedence/"settings.ini").write_text("; Glider settings file (schema 1)\n[QualityProfiles]\nselection=3\n"
            "profile2.ssrEnabled=0\nprofile2.ssr=1\nprofile2.ssaoEnabled=1\n",encoding="utf-8")
        session = Session(quads,QUAD_VIEW,env_extra={"GLIDER_DATA_DIR":str(precedence)},isolated=False)
        q = session.call("get_state")["quality"]["configured"]
        check(q["reflections"] == "off" and q["ssaoLevel"] != "off", "new explicit profile fields win over later legacy gates")
        session.call("set_persist",enabled=True)
        session.vset("lens_flare_intensity",0.5)
        session.close()
        session = Session(quads,QUAD_VIEW,env_extra={"GLIDER_DATA_DIR":str(precedence)},isolated=False)
        check(session.call("get_state")["quality"]["configured"]["reflections"] == "off", "canonical migration survives a second restart")
        session.close()
    except Exception as error:
        check(False,f"layer persistence failed: {error}")
        session.process.kill(); session.process.wait()


EXT = "ASOBO_material_windshield_v3"
EXT2 = "ASOBO_material_detail_map"


def check_extensions(ssr):
    print("[glTF extension switches: an app setting; CLI = pipe = script; one set for every viewport]")
    base = NO_TAA + SSR_VIEW
    cli = {
        "plain": cli_capture(EXE, ssr, "ext_cli_plain", base),
        "off": cli_capture(EXE, ssr, "ext_cli_off", base + ["--asobo", "off"]),
        "dis": cli_capture(EXE, ssr, "ext_cli_dis", base + ["--asobo-disable", EXT]),
    }
    check(not same(cli["plain"], cli["off"]) and not same(cli["plain"], cli["dis"]),
          "the SSR fixture shows --asobo off and --asobo-disable ASOBO_material_windshield_v3")
    session = Session(ssr, base)
    pipe = {}
    try:
        pipe["plain"] = session.capture("ext_pipe_plain")
        state = session.call("get_state")["gltfExtensions"]
        check(state["vendors"]["ASOBO"]["enabled"] and state["disabled"] == [] and state["saved"] == []
              and state["extensions"].get(EXT) is True and state["vendors"]["ASOBO"]["total"] >= 10,
              f"get_state gltfExtensions by default: vendors, extensions keyed by name, nothing disabled ({state['vendors']})")
        session.vset("asobo", False)  # the former viewport key sets the global value
        pipe["off"] = session.capture("ext_pipe_off")
        state = session.call("get_state")
        check(state["gltfExtensions"]["vendors"]["ASOBO"]["enabled"] is False and state["gltfExtensions"]["disabled"] == ["ASOBO"],
              f"get_state: asobo false -> vendors.ASOBO.enabled false, disabled {state['gltfExtensions']['disabled']}")
        check(all(v["effective"]["asobo"]["enabled"] is False for v in state["viewports"]),
              "every viewport mirrors the global master switch")
        session.vset("asobo", True)
        session.vset(EXT, False)
        pipe["dis"] = session.capture("ext_pipe_dis")
        state = session.call("get_state")
        check(state["gltfExtensions"]["disabled"] == [EXT] and state["gltfExtensions"]["extensions"][EXT] is False
              and state["viewports"][0]["effective"]["asobo"]["disabled"] == [EXT],
              "get_state: one extension off -> disabled [name], extensions[name] false, the viewport mirrors it")
        session.vset(EXT, True)
        pipe["plain_again"] = session.capture("ext_pipe_plain_again")
        listed = session.call("list_settings")["settings"]
        by_scope = {}
        for entry in listed:
            by_scope.setdefault(entry["scope"], set()).add(entry["key"])
        keys = {"asobo", "show_invisible_materials", EXT, EXT2}
        check(keys <= by_scope.get("renderer", set()), "list_settings: the extension keys are canonical renderer (app) settings")
        check(keys <= by_scope.get("viewport", set()), "list_settings: the former viewport keys are still listed (compatibility)")
        got = session.request("set_viewport_setting", key=EXT, value=False)
        check(got["ok"], "an extension switch needs no set_persist (this run only)")
        session.vset(EXT, True)
        bad = session.request("set_viewport_setting", key="asobo", value="maybe")
        check(not bad["ok"], "asobo maybe is refused")
        check(session.call("get_viewport_setting", key=EXT)["value"] is True, "get_viewport_setting reads the global value")
        errors = session.close()
        check(errors == 0, f"extension pipe session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"extension pipe session failed: {error}")
        session.process.kill()
        return
    check(same(cli["plain"], pipe["plain"]) and same(cli["plain"], pipe["plain_again"]), "pipe default == CLI default (and again)")
    check(same(cli["off"], pipe["off"]), "set_viewport_setting asobo false == CLI --asobo off")
    check(same(cli["dis"], pipe["dis"]), "set_viewport_setting ASOBO_material_windshield_v3 false == CLI --asobo-disable")
    _, rows = follow(ssr, "ext_script", base, ["wait 4", "set_viewport_setting asobo false",
                                               f"capture {(OUT / 'ext_script.png').as_posix()}", "quit"])
    check(same(cli["off"], OUT / "ext_script.png"), "--ui-script set_viewport_setting asobo false == CLI --asobo off")

    # Two viewports always share the set: whichever one is active, the other follows.
    lines = ["wait 10", "vpcreate", "wait 6", "vpsplit 2", "wait 10", "get_state",
             "set_viewport_setting asobo false", "wait 6", "get_state",
             "set_viewport_setting asobo true", f"set_viewport_setting {EXT} false", "wait 6", "get_state",
             "vpfocus 1", f"set_viewport_setting {EXT} true", "wait 6", "get_state", "quit"]
    proc, rows = follow(ssr, "ext_two_viewports", SSR_VIEW, lines)
    states = [r["result"] for r in rows if r.get("cmd") == "get_state" and r.get("ok")]
    if not check(len(states) == 4, f"two-viewport script: 4 states ({len(states)}; exit {proc.returncode}; "
                                   f"errors {[r for r in rows if not r.get('ok')][:2]})"):
        return
    s0, s1, s2, s3 = states
    asobo = lambda state, vid: next(v for v in state["viewports"] if v["id"] == vid)["effective"]["asobo"]  # noqa: E731
    check(len(s0["viewports"]) == 2 and all(asobo(s0, i) == {"enabled": True, "disabled": []} for i in (1, 2)),
          "two viewports: both apply every extension by default")
    check(all(asobo(s1, i)["enabled"] is False for i in (1, 2)),
          "asobo false set through viewport 2 (active) switches viewport 1 off too")
    check(all(asobo(s2, i) == {"enabled": True, "disabled": [EXT]} for i in (1, 2)),
          "an extension switched off through viewport 2 is off in viewport 1 as well")
    check(all(asobo(s3, i) == {"enabled": True, "disabled": []} for i in (1, 2)),
          "switched on again through viewport 1: on in both")
    check(all(s["glErrors"] == 0 for s in states), "two-viewport extension script: zero GL errors")


def read_ini(path):
    return path.read_text(encoding="utf-8") if path.exists() else ""


def extension_lines(text):
    """The [GltfExtensions] section's lines (empty when it is absent)."""
    lines, inside = [], False
    for line in text.splitlines():
        if line.startswith("["):
            inside = line.strip() == "[GltfExtensions]"
            continue
        if inside and line.strip() and not line.startswith(";"):
            lines.append(line.strip())
    return lines


def check_extension_persistence(ssr):
    print("[glTF extension switches: settings.ini [GltfExtensions]; --isolated and command-line flags write nothing]")
    data_dir = Path(tempfile.mkdtemp(prefix="glider_ext_"))
    try:
        env = {"GLIDER_DATA_DIR": str(data_dir)}
        settings = data_dir / "settings.ini"
        _, rows = follow(ssr, "ext_isolated", SSR_VIEW,
                         ["wait 4", "set_persist true", f"set_viewport_setting {EXT} false", "set_viewport_setting asobo false",
                          "set_viewport_setting show_invisible_materials true", "wait 4", "quit"], env_extra=env)
        check(rows and all(r.get("ok") for r in rows), f"isolated extension script ran ({[r for r in rows if not r.get('ok')][:2]})")
        check(not any(data_dir.iterdir()), f"--isolated: extension edits write nothing ({sorted(p.name for p in data_dir.iterdir())})")

        # 1. Pipe edits are session-only; under set_persist(true) the key you set is saved.
        session = Session(ssr, SSR_VIEW, env_extra=env, isolated=False)
        try:
            session.vset(EXT, False)
            check(extension_lines(read_ini(settings)) == [], "a pipe edit without set_persist saves no [GltfExtensions] section")
            session.vset(EXT, True)
            session.call("set_persist", enabled=True)
            session.vset(EXT, False)
            check(extension_lines(read_ini(settings)) == [f"{EXT}=0"],
                  f"set_persist + one extension off writes [GltfExtensions] {EXT}=0 only ({extension_lines(read_ini(settings))})")
            session.vset("asobo", False)
            check(sorted(extension_lines(read_ini(settings))) == sorted([f"{EXT}=0", "ASOBO=0"]),
                  f"the ASOBO master switch is stored as ASOBO=0 ({extension_lines(read_ini(settings))})")
            errors = session.close()
            check(errors == 0, f"persistence session 1: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"extension persistence session 1 failed: {error}")
            session.process.kill()
            return

        # 2. Restart: still set. Reset (everything on again, set_persist) writes nothing and removes the section.
        session = Session(ssr, SSR_VIEW, env_extra=env, isolated=False)
        try:
            state = session.call("get_state")["gltfExtensions"]
            check(state["disabled"] == sorted([EXT, "ASOBO"]) and state["saved"] == state["disabled"]
                  and state["vendors"]["ASOBO"]["enabled"] is False and state["extensions"][EXT] is False,
                  f"after a restart the switches are still off ({state['disabled']})")
            check(session.call("get_viewport_setting", key="asobo")["value"] is False, "get_viewport_setting asobo: still false after the restart")
            session.call("set_persist", enabled=True)
            session.vset("asobo", True)
            session.vset(EXT, True)
            check(extension_lines(read_ini(settings)) == [], f"back to the defaults: no [GltfExtensions] section ({read_ini(settings)!r})")
            errors = session.close()
            check(errors == 0, f"persistence session 2: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"extension persistence session 2 failed: {error}")
            session.process.kill()
            return

        # 3. A command-line flag applies to the run and is never saved, not even when something else is saved.
        session = Session(ssr, SSR_VIEW + ["--asobo", "off", "--asobo-disable", EXT], env_extra=env, isolated=False)
        try:
            state = session.call("get_state")["gltfExtensions"]
            check(state["disabled"] == sorted([EXT, "ASOBO"]) and state["saved"] == [] and state["differsFromSaved"] is True,
                  f"--asobo off + --asobo-disable: effective {state['disabled']}, saved {state['saved']}")
            session.call("set_persist", enabled=True)
            session.vset("shadows_allowed", False)  # another save writes settings.ini
            text = read_ini(settings)
            check("shadows=0" in text and extension_lines(text) == [], f"another save does not store the flags ({extension_lines(text)})")
            session.vset(EXT2, False)  # commits that key only
            check(extension_lines(read_ini(settings)) == [f"{EXT2}=0"],
                  f"set_persist commits only the key that was set ({extension_lines(read_ini(settings))})")
            session.vset("shadows_allowed", True)
            errors = session.close()
            check(errors == 0, f"persistence session 3: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"extension persistence session 3 failed: {error}")
            session.process.kill()
            return
        session = Session(ssr, SSR_VIEW, env_extra=env, isolated=False)
        try:
            state = session.call("get_state")["gltfExtensions"]
            check(state["disabled"] == [EXT2], f"restart without flags: only the committed key is off ({state['disabled']})")
            session.call("set_persist", enabled=True)
            session.vset(EXT2, True)
            session.close()
        except Exception as error:  # noqa: BLE001
            check(False, f"extension persistence session 4 failed: {error}")
            session.process.kill()
            return

        # 4. The Settings tab: a click saves the switch, Reset to defaults removes the section.
        click = ["wait 10", "clickp settings.tab.extensions 0", "wait 6", f"clickp settings.ext.{EXT2} 0", "wait 6", "snap clicked"]
        result, data = run_ui_script(ssr, "ext_ui_click", click, extra=["--show-search-paths"] + SSR_VIEW, env_extra=env)
        check(result.returncode == 0 and not data.get("failed", True), f"UI click script ran ({data.get('errors')})")
        check(extension_lines(read_ini(settings)) == [f"{EXT2}=0"],
              f"a click in File > Settings > glTF Extensions saves {EXT2}=0 ({extension_lines(read_ini(settings))})")
        reset = ["wait 10", "clickp settings.tab.extensions 0", "wait 6", "snap before", "clickp settings.ext.reset 0", "wait 6", "snap after"]
        result, data = run_ui_script(ssr, "ext_ui_reset", reset, extra=["--show-search-paths"] + SSR_VIEW, env_extra=env)
        check(result.returncode == 0 and not data.get("failed", True), f"UI reset script ran ({data.get('errors')})")
        check(extension_lines(read_ini(settings)) == [], f"Reset to defaults removes [GltfExtensions] ({read_ini(settings)!r})")
    finally:
        shutil.rmtree(data_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=str(REPO / "build" / "baselines" / "7f9ff37.exe"))
    parser.add_argument("--only", default=None, help="comma-separated: baseline,pipe,overrides,viewports,ui,persistence,extensions")
    args = parser.parse_args()
    only = set(args.only.split(",")) if args.only else None

    def wanted(name):
        return only is None or name in only

    if not EXE.exists():
        sys.exit(f"no build at {EXE}")
    OUT.mkdir(parents=True, exist_ok=True)
    quads = agent.write_quad_fixture(OUT / "scopes_quads.gltf")
    glass.OUT = OUT
    ssr = glass.ssr_fixture("scopes")
    if wanted("baseline"):
        baseline = Path(args.baseline_exe).resolve()
        if baseline.exists():
            check_baseline(baseline, quads, ssr)
        else:
            check(False, f"baseline {baseline} exists")
    if wanted("pipe"):
        check_cli_equals_pipe(ssr)
    if wanted("overrides"):
        check_overrides()
    if wanted("viewports"):
        check_two_viewports()
    if wanted("ui"):
        check_ui(quads)
    if wanted("persistence"):
        check_persistence(quads)
    if wanted("extensions"):
        check_extensions(ssr)
        check_extension_persistence(ssr)
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    for failure in failures:
        print("  FAIL " + failure)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
