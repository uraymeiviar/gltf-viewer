# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Settings scopes: global quality, scene effects, viewport view overrides (docs/SETTINGS_SCOPES.md).

One Glider process at a time; every run uses --isolated (and GLIDER_NO_FOCUS through harness_process) except the
persistence check, which points GLIDER_DATA_DIR at a temporary folder.

Checks:
  * defaults and every legacy flag combination of the moved settings (--aa, --msaa/--post-aa, --ssr*, --ssao*,
    --interior-sky-reflection, --windshield-icing, --decal-tolerance, --shadows, --shadow-*, --sky-occlusion-quality,
    --ocean-quality) render byte-identically to the baseline (single viewport, --capture-viewport);
  * CLI = pipe: the canonical keys (set_environment ssr / ssao / shadows ..., renderer msaa / ssao_quality ...), the
    compatibility keys in their former scopes (set_viewport_setting ssr / ssao_radius / msaa ...) and the viewport
    overrides give the same capture as the CLI flags;
  * overrides: viewport_* off / on / scene behave (off removes the scene's effect, on adds it, scene follows);
  * two viewports always share the quality (an AA / SSAO quality change through either one reaches both);
  * a scene effect toggle reaches every viewport set to Scene and none that overrides it;
  * get_state carries quality (configured / effective), environment.effects and each viewport's overrides and
    effective values;
  * no ImGui ID conflict on the controls this change touched (Viewport Settings Render Modes overrides and Quality
    tab, Scene Inspector effects) with the HDR and the procedural sky; screenshots of each panel go to
    build/review/settings_scopes/;
  * --isolated writes nothing; without it a pipe edit of a non-persisted key writes nothing and set_persist(true) +
    shadows_allowed false writes only that non-default value.

Usage: python scripts/validate_settings_scopes.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b,...]
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
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
import validate_agent_control as agent  # noqa: E402  (quad fixture with a windshield_v3 quad)
import validate_asobo_glass as glass  # noqa: E402  (SSR fixture: a windshield_v3 pane reflecting an emissive box)

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "settings_scopes"
REVIEW = REPO / "build" / "review" / "settings_scopes"
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


def cli_capture(exe, model, name, flags, frame=8):
    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(exe)] + model_args(model) + COMMON + list(flags) +
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
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL,
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 120
        try:
            while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
                if self.process.poll() is not None or time.time() > deadline:
                    raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
                time.sleep(0.1)
            self.pipe = glider_ctl.GliderPipe(self.process.pid)
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

def check_baseline(baseline, quads, ssr):
    print(f"[defaults and legacy flags: current build == baseline {baseline.name}]")
    cases = {
        "default_cube": (None, []),
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
    }
    for name, (model, flags) in cases.items():
        current = cli_capture(EXE, model, f"base_cur_{name}", flags)
        reference = cli_capture(baseline, model, f"base_ref_{name}", flags)
        check(current is not None and same(current, reference), f"{name}: {' '.join(flags) or '(no flags)'}")
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
        pipe["ssr_override"] = session.capture("pipe_ssr_override")
        check(session.call("get_viewport_setting", key="ssr")["value"] is False, "get_viewport_setting ssr = false under viewport_ssr off")
        session.vset("viewport_ssr", "scene")
        session.vset("ssr", False)  # compatibility key: this viewport's override Off
        pipe["ssr_compat"] = session.capture("pipe_ssr_compat")
        check(session.call("get_viewport_setting", key="viewport_ssr")["value"] == "off",
              "set_viewport_setting ssr false sets viewport_ssr off")
        session.vset("viewport_ssr", "scene")
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
    print("[overrides Off / On / Scene, shadows and SSAO (HelloCube on a ground plane)]")
    cli = {
        "plain": cli_capture(EXE, None, "ovr_plain", NO_TAA + SUN),
        "ssao": cli_capture(EXE, None, "ovr_ssao", NO_TAA + SUN + ["--ssao", "on", "--ssao-radius", "0.8", "--ssao-quality", "high"]),
        "noshadow": cli_capture(EXE, None, "ovr_noshadow", NO_TAA + SUN + ["--shadows", "off"]),
    }
    check(not same(cli["plain"], cli["ssao"]) and not same(cli["plain"], cli["noshadow"]),
          "the fixture shows SSAO and the sun shadow")
    session = Session(None, NO_TAA + SUN)
    pipe = {}
    try:
        session.eset("ssao", True)
        session.eset("ssao_radius", 0.8)
        session.vset("ssao_quality", "high")
        pipe["ssao_env"] = session.capture("ovr_pipe_ssao_env")
        session.vset("viewport_ssao", "off")
        pipe["ssao_scene_on_vp_off"] = session.capture("ovr_pipe_ssao_vp_off")
        session.eset("ssao", False)
        session.vset("viewport_ssao", "on")
        pipe["ssao_scene_off_vp_on"] = session.capture("ovr_pipe_ssao_vp_on")
        session.vset("viewport_ssao", "scene")
        pipe["ssao_scene_off"] = session.capture("ovr_pipe_ssao_scene_off")
        # Compatibility viewport keys for the SSAO look (they reach the scene).
        session.eset("ssao_radius", 0.5)
        session.vset("ssao", True)
        session.vset("ssao_radius", 0.8)
        pipe["ssao_compat"] = session.capture("ovr_pipe_ssao_compat")
        session.vset("viewport_ssao", "scene")
        session.eset("ssao_radius", 0.5)
        session.vset("ssao_quality", "medium")
        # Shadows: scene on + viewport off, scene off + viewport on, scene off, compatibility key.
        session.vset("viewport_shadows", "off")
        pipe["shadow_vp_off"] = session.capture("ovr_pipe_shadow_vp_off")
        state = session.call("get_state")
        check(state["viewports"][0]["overrides"]["shadows"] == "off" and state["viewports"][0]["effective"]["shadows"] is False
              and state["sun"]["shadows"]["viewports"][0]["active"] is False,
              "get_state: viewport_shadows off -> effective false, no active shadow map")
        session.eset("shadows", False)
        session.vset("viewport_shadows", "on")
        pipe["shadow_scene_off_vp_on"] = session.capture("ovr_pipe_shadow_vp_on")
        session.vset("viewport_shadows", "scene")
        pipe["shadow_scene_off"] = session.capture("ovr_pipe_shadow_scene_off")
        state = session.call("get_state")
        check(state["environment"]["effects"]["shadows"] is False and state["sun"]["shadows"]["scene"] is False,
              "get_state: environment.effects.shadows and sun.shadows.scene false")
        session.eset("shadows", True)
        session.vset("shadows", False)  # former renderer key: now the scene's switch
        pipe["shadow_compat"] = session.capture("ovr_pipe_shadow_compat")
        check(session.call("get_environment", key="shadows")["value"] is False,
              "set_viewport_setting shadows false (former renderer key) reaches the scene")
        session.eset("shadows", True)
        pipe["plain"] = session.capture("ovr_pipe_plain")
        errors = session.close()
        check(errors == 0, f"override session: {errors} GL errors")
    except Exception as error:  # noqa: BLE001
        check(False, f"override session failed: {error}")
        session.process.kill()
        return
    check(same(cli["ssao"], pipe["ssao_env"]), "set_environment ssao + radius, renderer ssao_quality == CLI")
    check(same(cli["plain"], pipe["ssao_scene_on_vp_off"]), "scene SSAO on + viewport_ssao off == no SSAO")
    check(same(cli["ssao"], pipe["ssao_scene_off_vp_on"]), "scene SSAO off + viewport_ssao on == SSAO")
    check(same(cli["plain"], pipe["ssao_scene_off"]), "viewport_ssao scene follows the scene (off)")
    check(same(cli["ssao"], pipe["ssao_compat"]), "compatibility ssao / ssao_radius viewport keys == CLI")
    check(same(cli["noshadow"], pipe["shadow_vp_off"]), "scene shadows on + viewport_shadows off == --shadows off")
    check(same(cli["plain"], pipe["shadow_scene_off_vp_on"]), "scene shadows off + viewport_shadows on == shadows")
    check(same(cli["noshadow"], pipe["shadow_scene_off"]), "viewport_shadows scene follows the scene (off)")
    check(same(cli["noshadow"], pipe["shadow_compat"]), "former renderer key shadows false == --shadows off")
    check(same(cli["plain"], pipe["plain"]), "restored: the plain capture again")


def check_two_viewports():
    print("[two viewports: shared quality, scene toggles reach the viewports set to Scene]")
    lines = ["wait 10", "vpcreate", "wait 6", "vpsplit 2", "wait 10", "get_state",
             # Viewport 2 is active: a quality change through it reaches viewport 1 too.
             "set_viewport_setting msaa 4", "set_viewport_setting ssao_quality high", "wait 6", "get_state",
             # Scene SSAO and shadows on, viewport 2 overrides SSAO off.
             "set_viewport_setting viewport_ssao off", "set_environment ssao true", "wait 10", "get_state",
             "set_environment shadows false", "wait 6", "get_state",
             "set_viewport_setting viewport_shadows on", "wait 6", "get_state",
             "vpfocus 1", "set_viewport_setting aa taa", "wait 6", "get_state", "quit"]
    proc, rows = follow(None, "two_viewports", SUN, lines)
    states = [r["result"] for r in rows if r.get("cmd") == "get_state" and r.get("ok")]
    if not check(len(states) == 6, f"two-viewport script: 6 states ({len(states)}; exit {proc.returncode}; "
                                   f"errors {[r for r in rows if not r.get('ok')][:2]})"):
        return

    def vp(state, vid):
        return next(v for v in state["viewports"] if v["id"] == vid)

    def ssao(state, vid):
        return next(v for v in state["ssao"]["viewports"] if v["viewport"] == vid)

    def shadow(state, vid):
        return next(v for v in state["sun"]["shadows"]["viewports"] if v["viewport"] == vid)

    s0, s1, s2, s3, s4, s5 = states
    check(len(s0["viewports"]) == 2 and vp(s0, 1)["effective"]["postAA"] == vp(s0, 2)["effective"]["postAA"] == "taa",
          "a new viewport renders with the global AA (TAA)")
    check(all(vp(s1, i)["effective"]["msaa"] == "msaa4" for i in (1, 2)) and
          all(ssao(s1, i)["quality"] == "high" for i in (1, 2)),
          f"msaa 4 / ssao_quality high set through viewport 2 reach both "
          f"({[vp(s1, i)['effective']['msaa'] for i in (1, 2)]}, {[ssao(s1, i)['quality'] for i in (1, 2)]})")
    check(ssao(s2, 1)["enabled"] and ssao(s2, 1)["active"] and not ssao(s2, 2)["enabled"] and not ssao(s2, 2)["active"]
          and ssao(s2, 2)["override"] == "off",
          "scene SSAO on: viewport 1 (Scene) active, viewport 2 (Off) not")
    check(shadow(s2, 1)["active"] and shadow(s2, 2)["active"], "scene shadows on: both viewports have a shadow map")
    check(not shadow(s3, 1)["active"] and not shadow(s3, 2)["active"], "scene shadows off: neither viewport")
    check(not shadow(s4, 1)["active"] and shadow(s4, 2)["active"] and vp(s4, 2)["overrides"]["shadows"] == "on",
          "viewport_shadows on in viewport 2: only viewport 2 draws shadows")
    check(all(vp(s5, i)["effective"]["msaa"] == "none" and vp(s5, i)["effective"]["postAA"] == "taa" for i in (1, 2)),
          "aa taa through viewport 1 reaches viewport 2")
    check(all(s["glErrors"] == 0 for s in states), "two-viewport script: zero GL errors")


UI_TAGS_RENDER = ["vp.override.shadows", "vp.override.ssr", "vp.override.ssao"]
UI_TAGS_QUALITY = ["quality.msaa", "quality.post_aa", "quality.ssr", "quality.ssao", "quality.ssao_resolution",
                   "quality.sky_occlusion", "quality.ocean"]
UI_TAGS_QUALITY_LOW = ["scene.ibl.cube", "quality.shadows"]  # IBL filter and shadow gate, lower in the Quality tab
UI_TAGS_SCENE = ["scene.effects.shadows", "scene.effects.ssr", "scene.effects.interior_sky", "scene.effects.ssao",
                 "scene.effects.ssao_radius", "scene.effects.ssao_intensity", "scene.effects.icing"]


def check_ui(quads):
    print("[UI: Viewport Settings (Render Modes, Quality, Camera, Post), Scene Inspector; ID conflicts; screenshots]")
    REVIEW.mkdir(parents=True, exist_ok=True)
    shot = lambda name: f"shot {(REVIEW / name).as_posix()}"  # noqa: E731
    commands = ["wait 24", "set_environment ssao true", "clickp hier.viewport 1", "wait 8"]
    for tag in UI_TAGS_RENDER:
        commands += [f"hoverp {tag} 0", "wait 3", f"snap {tag}"]
    commands += [shot("viewport_render_modes.png"), "clickp vp.tab.quality 0", "wait 8"]
    for tag in UI_TAGS_QUALITY:
        commands += [f"hoverp {tag} 0", "wait 3", f"snap {tag}"]
    for tag in UI_TAGS_QUALITY_LOW:
        # scene.ibl.cube carries the cube size as its index; quality.shadows its value (1 = allowed).
        index = 1024 if tag == "scene.ibl.cube" else 1
        commands += [f"hoverp {tag} {index}", "wait 3", f"snap {tag}"]
    commands += [shot("viewport_quality.png"), "hoverp quality.msaa 0", "wheel -20", "wait 6",
                 shot("viewport_quality_scrolled.png"), "move 1700 600", "wheel 40", "wait 4",
                 "clickp vp.tab.camera 0", "wait 6", "set_viewport_setting ground ocean",
                 "wait 12", "hoverp vp.ocean.quality 0", "wait 3", "snap vp.ocean.quality", shot("viewport_camera_ocean.png"),
                 "set_viewport_setting ground grid", "clickp vp.tab.post 0", "wait 6", shot("viewport_post.png"),
                 "clickp hier.scene 0", "wait 8", shot("scene_inspector_top.png"), "hoverp scene.environment 0", "wheel -12",
                 "wait 6"]
    for tag in UI_TAGS_SCENE:
        commands += [f"hoverp {tag} 0", "wait 3", f"snap hdr_{tag}"]
    commands += [shot("scene_inspector_hdr.png"), "set_environment environment sky", "wait 16", "hoverp scene.sun_time 0",
                 "wheel -12", "wait 6"]
    for tag in UI_TAGS_SCENE:
        commands += [f"hoverp {tag} 0", "wait 3", f"snap sky_{tag}"]
    commands += [shot("scene_inspector_sky.png"), "snap final", ""]
    script, report = OUT / "ui.txt", OUT / "ui.json"
    script.write_text("\n".join(commands), encoding="utf-8")
    report.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(quads), "--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05",
                             "--aa", "none", "--vsync", "off", "--ui-script", str(script), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    (OUT / "ui.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    snaps = data.get("snaps", [])
    conflicts = [s["label"] for s in snaps if s.get("idConflict", -1) != 0 or s.get("hoveredIdCount", 0) > 1]
    check(result.returncode == 0 and not data.get("failed", True) and not data.get("errors"),
          f"UI script ran (exit {result.returncode}, failed {data.get('failed')}, errors {data.get('errors')})")
    wanted = len(UI_TAGS_RENDER) + len(UI_TAGS_QUALITY) + len(UI_TAGS_QUALITY_LOW) + 1 + 2 * len(UI_TAGS_SCENE)
    check(len(snaps) >= wanted, f"{len(snaps)} hover snapshots ({wanted} controls)")
    check(not conflicts, f"no ImGui ID conflict on the touched controls {conflicts}")
    check(max((s["glErrors"] for s in snaps), default=-1) == 0, "UI script: 0 GL errors")
    for name in ("viewport_render_modes", "viewport_quality", "viewport_quality_scrolled", "viewport_camera_ocean", "viewport_post",
                 "scene_inspector_top", "scene_inspector_hdr", "scene_inspector_sky"):
        check((REVIEW / f"{name}.png").exists(), f"screenshot {name}.png")


def check_persistence(quads):
    print("[persistence: --isolated writes nothing; only non-default values]")
    data_dir = Path(tempfile.mkdtemp(prefix="glider_scopes_"))
    try:
        env = {"GLIDER_DATA_DIR": str(data_dir)}
        _, rows = follow(quads, "persist_isolated", QUAD_VIEW,
                         ["wait 4", "set_persist true", "set_viewport_setting msaa 4", "set_environment ssao true",
                          "set_viewport_setting viewport_ssr off", "set_viewport_setting shadows_allowed false",
                          "set_viewport_setting shadow_resolution 4096", "wait 4", "quit"], env_extra=env)
        check(rows and all(r.get("ok") for r in rows), f"isolated script ran ({[r for r in rows if not r.get('ok')][:2]})")
        check(not any(data_dir.iterdir()), f"--isolated: nothing written to GLIDER_DATA_DIR ({sorted(p.name for p in data_dir.iterdir())})")

        session = Session(quads, QUAD_VIEW, env_extra=env, isolated=False)
        settings = data_dir / "settings.ini"
        before = settings.read_text(encoding="utf-8") if settings.exists() else None
        try:
            for key, value in (("msaa", "4"), ("ssao_quality", "high"), ("viewport_ssao", "on")):
                session.vset(key, value)
            session.eset("ssr", False)
            session.eset("shadows", False)
            after = settings.read_text(encoding="utf-8") if settings.exists() else None
            check(after == before, "pipe edits of quality / scene / override keys write no settings.ini")
            refused = session.request("set_viewport_setting", key="shadows_allowed", value=False)
            check(not refused["ok"] and refused["error"]["code"] == "persist_required",
                  "shadows_allowed is a saved preference (needs set_persist)")
            session.call("set_persist", enabled=True)
            session.vset("shadows_allowed", False)
            text = settings.read_text(encoding="utf-8") if settings.exists() else ""
            section = [line for line in text.splitlines() if line and not line.startswith(";")]
            check("[RenderQuality]" in text and "shadows=0" in text and "msaa" not in text and "ssao" not in text,
                  f"set_persist + shadows_allowed false writes [RenderQuality] shadows=0 only ({section})")
            session.vset("shadows_allowed", True)
            text = settings.read_text(encoding="utf-8") if settings.exists() else ""
            check("shadows=" not in text, "back to the default: the value is removed again")
            errors = session.close()
            check(errors == 0, f"persistence session: {errors} GL errors")
        except Exception as error:  # noqa: BLE001
            check(False, f"persistence session failed: {error}")
            session.process.kill()
    finally:
        shutil.rmtree(data_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=str(REPO / "build" / "baselines" / "0e4fcfc.exe"))
    parser.add_argument("--only", default=None, help="comma-separated: baseline,pipe,overrides,viewports,ui,persistence")
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
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    for failure in failures:
        print("  FAIL " + failure)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
