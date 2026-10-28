# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Agent control channel: Phase 1 (named pipe, MCP shim) and Phase 3 (shared command registry).

docs/AGENT_CONTROL_PLAN.md. One Glider process at a time; every run uses --isolated except the
persistence check, which points GLIDER_DATA_DIR at a temporary folder.

Checks:
  * exit gate: over the pipe, isolate a node, frame it from iso, capture; the node's pixels dominate the image;
  * the same capture through the pipe, --ui-script and the CLI flags is byte-identical;
  * set_viewport_setting / set_environment / set_material_param change the capture exactly as the matching CLI
    flags (--ssr off, --asobo-disable, --exposure, --set-asobo-param, ...) and get_* returns what was set;
  * probe on an unlit quad of known colour and on the --clear-color background returns the linear value within
    PROBE_TOLERANCE, and does not change the next capture; probe_region agrees;
  * pipe commands write no settings.ini (GLIDER_DATA_DIR in a temporary folder, no --isolated);
  * a second simultaneous client is refused with a "busy" error;
  * scripts/glider_mcp.py: initialize, tools/list and a tools/call capture over stdio return valid JSON-RPC and
    an image content item;
  * --agent-guide writes the guide with the generated command table;
  * with --baseline-exe: CLI captures that use the flags now routed through the registry are byte-identical to
    the baseline binary (the channel off changes nothing);
  * zero GL errors in every pipe session.

Usage: python scripts/validate_agent_control.py [--baseline-exe build/baselines/<sha>.exe]
"""

import argparse
import base64
import hashlib
import json
import os
import shutil
import struct
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "agent_control"
HIERARCHY = REPO / "resources" / "test_models" / "hierarchy_animation_fixture.gltf"
COMMON = ["--isolated", "--window-size", "1600x900", "--aa", "none", "--hide-grid", "--fixed-frame-time", "0.05",
          "--vsync", "off"]
# The quad fixture: fixed camera, black background, linear tone mapping input.
VIEW = ["--clear-color", "0.1,0.2,0.3", "--bloom", "off", "--auto-exposure", "off", "--tone-mapping", "neutral",
        "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "7"]
FLAT_COLOR = (0.25, 0.5, 0.75)
CLEAR_COLOR = (0.1, 0.2, 0.3)
# RGBA16F stores 11 significant bits: 0.75 is exact, 0.1 rounds to 0.0999755859375.
PROBE_TOLERANCE = 2e-3
FRESNEL = "ASOBO_material_fresnel_fade"

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


# ---------------------------------------------------------------------------------------------
# Fixture: three quads facing +Z. "Flat" is unlit (known linear colour), "Glass" is an unlit BLEND quad
# with ASOBO_material_fresnel_fade (offset 0 = opaque), "Shield" carries ASOBO_material_windshield_v3 (SSR).
# ---------------------------------------------------------------------------------------------

def write_quad_fixture(path):
    positions, normals, indices = [], [], []
    quads = [(-1.6, 0.6), (1.6, 0.6), (0.0, -1.4)]
    for cx, cy in quads:
        for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            positions.append((cx + dx * 1.2, cy + dy * 0.8, 0.0))
            normals.append((0.0, 0.0, 1.0))
        indices.append([0, 1, 2, 0, 2, 3])  # each quad's accessors address its own four vertices
    blob = b"".join(struct.pack("<3f", *p) for p in positions) + b"".join(struct.pack("<3f", *n) for n in normals)
    position_bytes = len(positions) * 12
    index_offset = len(blob)
    for quad in indices:
        blob += struct.pack("<6H", *quad)
    # Per quad: POSITION (own bounds, so node_screen_rect is the quad), NORMAL, indices.
    accessors = []
    for q in range(len(quads)):
        corners = positions[q * 4:q * 4 + 4]
        accessors.append({"bufferView": 0, "byteOffset": q * 48, "componentType": 5126, "count": 4, "type": "VEC3",
                          "min": [min(p[i] for p in corners) for i in range(3)],
                          "max": [max(p[i] for p in corners) for i in range(3)]})
        accessors.append({"bufferView": 1, "byteOffset": q * 48, "componentType": 5126, "count": 4, "type": "VEC3"})
        accessors.append({"bufferView": 2, "byteOffset": q * 12, "componentType": 5123, "count": 6, "type": "SCALAR"})
    materials = [
        {"name": "Flat", "doubleSided": True, "extensions": {"KHR_materials_unlit": {}},
         "pbrMetallicRoughness": {"baseColorFactor": [*FLAT_COLOR, 1.0], "metallicFactor": 0.0}},
        {"name": "Glass", "doubleSided": True, "alphaMode": "BLEND",
         "extensions": {"KHR_materials_unlit": {}, FRESNEL: {"fresnelFactor": 1.0, "fresnelOpacityOffset": 0.0}},
         "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.3, 0.2, 1.0], "metallicFactor": 0.0}},
        {"name": "Shield", "doubleSided": True, "alphaMode": "BLEND",
         "extensions": {"ASOBO_material_windshield_v3": {}},
         "pbrMetallicRoughness": {"baseColorFactor": [0.6, 0.7, 0.8, 0.4], "metallicFactor": 0.0, "roughnessFactor": 0.05}},
    ]
    document = {
        "asset": {"version": "2.0", "generator": "validate_agent_control.py"},
        "extensionsUsed": sorted({"KHR_materials_unlit", FRESNEL, "ASOBO_material_windshield_v3"}),
        "scene": 0,
        "scenes": [{"nodes": [0, 1, 2]}],
        "nodes": [{"name": name, "mesh": i} for i, name in enumerate(("Flat", "Glass", "Shield"))],
        "meshes": [{"name": f"{name}Mesh", "primitives": [{"attributes": {"POSITION": 3 * i, "NORMAL": 3 * i + 1},
                                                            "indices": 3 * i + 2, "material": i}]}
                   for i, name in enumerate(("Flat", "Glass", "Shield"))],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": position_bytes, "target": 34962},
            {"buffer": 0, "byteOffset": position_bytes, "byteLength": position_bytes, "target": 34962},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(blob) - index_offset, "target": 34963},
        ],
        "buffers": [{"byteLength": len(blob),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")}],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=1), encoding="utf-8")
    return path


# ---------------------------------------------------------------------------------------------
# Glider runs
# ---------------------------------------------------------------------------------------------

def cli_capture(exe, model, name, flags):
    """--capture-viewport at frame 8 (the pipe's default settle) and exit."""
    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    result = subprocess.run([str(exe), str(model)] + COMMON + list(flags) +
                            ["--capture-viewport", str(png), "--capture-frame", "8",
                             "--profile-report", str(OUT / f"{name}.json"), "--exit-after-profile"],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        print(result.stdout[-2000:], result.stderr[-2000:])
    return png if png.exists() else None


def script_capture(model, name, lines, flags=()):
    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    script = OUT / f"{name}.script"
    script.write_text("\n".join(lines + [f"capture {png}"]) + "\n", encoding="utf-8")
    result = subprocess.run([str(EXE), str(model)] + COMMON + list(flags) +
                            ["--ui-script", str(script), "--ui-report", str(OUT / f"{name}.report.json")],
                            cwd=REPO, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        print(result.stdout[-2000:], result.stderr[-2000:])
    return png if png.exists() else None


class Session:
    """A Glider with --control-pipe and a connected client."""

    def __init__(self, model, flags=(), env_extra=None, isolated=True):
        args = [str(EXE), str(model)] + (COMMON if isolated else [a for a in COMMON if a != "--isolated"])
        args += list(flags) + ["--control-pipe"]
        env = os.environ.copy()
        env.update(env_extra or {})
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL,
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 120
        while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
            if self.process.poll() is not None or time.time() > deadline:
                raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
            time.sleep(0.1)
        self.pipe = glider_ctl.GliderPipe(self.process.pid)

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def request(self, cmd, **args):
        return self.pipe.request(cmd, args)

    def capture(self, name, **args):
        path = OUT / f"{name}.png"
        path.unlink(missing_ok=True)
        self.call("capture", path=str(path), **args)
        return path if path.exists() else None

    def close(self, quit_app=True):
        gl_errors = None
        try:
            if quit_app:
                gl_errors = self.call("get_state")["glErrors"]
                self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:
                self.process.kill()
        return gl_errors


def teal_fraction(path, rect=None):
    from PIL import Image
    image = Image.open(path).convert("RGB")
    if rect:
        image = image.crop(tuple(int(round(v)) for v in rect))
    pixels = list(image.getdata())
    return sum(1 for r, g, b in pixels if g > r + 40 and b > r + 40) / max(1, len(pixels))


def orange_fraction(path):
    from PIL import Image
    pixels = list(Image.open(path).convert("RGB").getdata())
    return sum(1 for r, g, b in pixels if r > g + 40 and g > b + 20) / max(1, len(pixels))


def close_to(values, expected, tolerance):
    return all(abs(a - b) <= tolerance for a, b in zip(values, expected))


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_exit_gate_and_paths():
    print("[exit gate and capture paths: hierarchy fixture]")
    session = Session(HIERARCHY)
    try:
        nodes = session.call("list_nodes", filter="grand")
        check(nodes["matched"] == 1 and nodes["nodes"][0]["name"] == "GrandChild", "list_nodes filters by name")
        missing = session.request("frame_node", node="GrandChildX")
        check(not missing["ok"] and missing["error"]["code"] == "node_not_found" and missing["error"].get("candidates"),
              "an unknown node fails with node_not_found and candidates")
        iso = session.call("isolate", node="GrandChild")
        check(iso["node"]["name"] == "GrandChild", "isolate GrandChild")
        framed = session.call("frame_node", node="GrandChild", view="iso", margin=1.0)
        check("eye" in framed["camera"], "frame_node returns the camera")
        gate = session.capture("gate_iso", overlays=False)
        rect = session.call("node_screen_rect", node="GrandChild")["screenRect"]
        center = (int((rect[0] + rect[2]) / 2), int((rect[1] + rect[3]) / 2))
        picked = session.call("pick", x=center[0], y=center[1])
        state = session.call("get_state")
        check(state["isolation"]["node"]["name"] == "GrandChild", "get_state reports the isolation")
        check(state["selectedNode"]["name"] == "GrandChild", "get_state reports the selection")
        session.call("exit_isolate")
        session.call("clear_selection")
        check(session.call("get_state")["isolation"] is None, "exit_isolate clears the isolation")
        gl_errors = session.close()
    except Exception:
        session.close(quit_app=False)
        raise
    if check(gate is not None, "pipe capture written"):
        teal = teal_fraction(gate)
        teal_in_rect = teal_fraction(gate, rect)
        check(teal > 0.15 and orange_fraction(gate) < 0.001,
              f"exit gate: the isolated node dominates the capture (teal {teal:.2f} of the image, other parts 0)")
        check(teal_in_rect > 0.5, f"exit gate: its screen rect is mostly the node ({teal_in_rect:.2f})")
    check(picked and picked.get("node") == "GrandChild", f"pick at the rect centre returns GrandChild ({picked})")
    check(gl_errors == 0, f"no GL errors in the pipe session ({gl_errors})")

    print("[same capture through pipe, --ui-script and CLI]")
    cli = cli_capture(EXE, HIERARCHY, "path_cli", ["--frame-node", "GrandChild", "--frame-view", "iso"])
    script = script_capture(HIERARCHY, "path_script", ["frame GrandChild iso"])
    script_new = script_capture(HIERARCHY, "path_script_registry", ["frame_node GrandChild iso"])
    session = Session(HIERARCHY)
    try:
        session.call("frame_node", node="GrandChild", view="iso")
        pipe = session.capture("path_pipe")
        session.close()
    except Exception:
        session.close(quit_app=False)
        raise
    check(cli is not None and same(cli, pipe), "pipe capture == CLI capture (byte-identical)")
    check(same(cli, script), "--ui-script `frame` capture == CLI capture (byte-identical)")
    check(same(cli, script_new), "--ui-script `frame_node` (registry name) capture == CLI capture")


def check_settings_and_probe(fixture):
    print("[settings: pipe and script against the CLI flags (quad fixture)]")
    references = {
        "plain": [],
        "ssr_off": ["--ssr", "off"],
        "fresnel_disabled": ["--asobo-disable", FRESNEL],
        "exposure": ["--exposure", "1.5"],
        "material_param": ["--set-asobo-param", "Glass", FRESNEL, "fresnelOpacityOffset", "0.5"],
        "wire": ["--wireframe-overlay", "--wireframe-color", "1,0,0,0.5", "--wireframe-blend", "additive"],
        "tone_night": ["--tone-mapping", "hill", "--time-of-day", "night", "--msfs-emissive-scale", "0.01"],
        "debug_view": ["--debug-view", "geometrynormal"],
    }
    cli = {name: cli_capture(EXE, fixture, f"set_cli_{name}", VIEW + flags) for name, flags in references.items()}
    check(not same(cli["plain"], cli["fresnel_disabled"]), "the fixture shows --asobo-disable of the fresnel fade")
    check(not same(cli["plain"], cli["exposure"]), "the fixture shows --exposure")
    check(not same(cli["plain"], cli["material_param"]), "the fixture shows the fresnelOpacityOffset edit")

    session = Session(fixture, VIEW)
    try:
        results = {}
        results["plain"] = session.capture("set_pipe_plain")

        set_ssr = session.call("set_viewport_setting", key="ssr", value=False)
        check(set_ssr["value"] is False and session.call("get_viewport_setting", key="ssr")["value"] is False,
              "get_viewport_setting returns ssr = false")
        results["ssr_off"] = session.capture("set_pipe_ssr_off")
        session.call("set_viewport_setting", key="ssr", value=True)

        session.call("set_viewport_setting", key=FRESNEL, value=False)
        check(session.call("get_viewport_setting", key=FRESNEL)["value"] is False, f"get_viewport_setting {FRESNEL} = false")
        results["fresnel_disabled"] = session.capture("set_pipe_fresnel_disabled")
        session.call("set_viewport_setting", key=FRESNEL, value=True)

        session.call("set_environment", key="exposure", value=1.5)
        check(session.call("get_environment", key="exposure")["value"] == 1.5, "get_environment returns exposure = 1.5")
        results["exposure"] = session.capture("set_pipe_exposure")
        session.call("set_environment", key="exposure", value=0)

        edited = session.call("set_material_param", material="Glass", extension=FRESNEL, param="fresnelOpacityOffset", value=0.5)
        check(edited["value"] == 0.5, "set_material_param returns the new value")
        check(session.call("get_material_param", material="Glass", extension=FRESNEL, param="fresnelOpacityOffset")["value"] == 0.5,
              "get_material_param returns fresnelOpacityOffset = 0.5")
        results["material_param"] = session.capture("set_pipe_material_param")
        session.call("set_material_param", material="Glass", extension=FRESNEL, param="fresnelOpacityOffset", value=0)

        session.call("set_viewport_setting", key="wireframe_overlay", value=True)
        session.call("set_viewport_setting", key="wireframe_color", value=[1, 0, 0, 0.5])
        session.call("set_viewport_setting", key="wireframe_blend", value="additive")
        results["wire"] = session.capture("set_pipe_wire")
        session.call("set_viewport_setting", key="wireframe_overlay", value=False)
        session.call("set_viewport_setting", key="wireframe_color", value=[1, 1, 1, 1])
        session.call("set_viewport_setting", key="wireframe_blend", value="normal")

        session.call("set_environment", key="tone_mapping", value="hill")
        session.call("set_environment", key="time_of_day", value="night")
        session.call("set_environment", key="msfs_emissive_scale", value=0.01)
        results["tone_night"] = session.capture("set_pipe_tone_night")
        session.call("set_environment", key="tone_mapping", value="neutral")
        session.call("set_environment", key="time_of_day", value="day")
        session.call("set_environment", key="msfs_emissive_scale", value=0.001)

        session.call("set_render_mode", mode="geometry_normal")
        results["debug_view"] = session.capture("set_pipe_debug_view")
        session.call("set_render_mode", mode="shaded")
        results["plain_again"] = session.capture("set_pipe_plain_again")

        bad = session.request("set_viewport_setting", key="interior_sky_reflection", value=3)
        check(not bad["ok"] and bad["error"]["code"] == "bad_value", "an out-of-range value is refused (bad_value)")
        unknown = session.request("set_viewport_setting", key="no_such_key", value=1)
        check(not unknown["ok"] and unknown["error"]["code"] == "unknown_setting", "an unknown key is refused")
        listed = session.call("list_settings")["settings"]
        keys = {entry["key"] for entry in listed}
        wanted = {"aa", "msaa", "post_aa", "ssr", "ssr_quality", "interior_sky_reflection", "windshield_icing",
                  "decal_tolerance_mm", "grid", "grid_mode", "render_mode", "wireframe_overlay", "wireframe_color",
                  "wireframe_blend", "asobo", FRESNEL, "show_invisible_materials", "exposure", "tone_mapping",
                  "hdr_rotation", "time_of_day", "msfs_emissive_scale", "msfs_emissive"}
        check(wanted <= keys, f"list_settings covers the CLI settings (missing: {sorted(wanted - keys)})")
        check(all("value" in e and "default" in e and "type" in e for e in listed), "list_settings has type, value and default")

        print("[probe]")
        rect = session.call("node_screen_rect", node="Flat")["screenRect"]
        cx, cy = int((rect[0] + rect[2]) / 2), int((rect[1] + rect[3]) / 2)
        before = session.capture("probe_before")
        flat = session.call("probe", x=cx, y=cy)
        background = session.call("probe", x=3, y=3)
        region = session.call("probe_region", x0=cx - 10, y0=cy - 10, x1=cx + 10, y1=cy + 10)
        after = session.capture("probe_after")
        check(close_to(flat["hdr"], FLAT_COLOR, PROBE_TOLERANCE),
              f"probe on the unlit quad returns its linear colour {FLAT_COLOR} within {PROBE_TOLERANCE} ({flat['hdr'][:3]})")
        check(flat["pick"] and flat["pick"]["node"] == "Flat" and flat["pick"]["material"] == "Flat",
              "probe picks the node and material")
        check(flat["world"] is not None and abs(flat["world"][2]) < 1e-3 and abs(flat["world"][0] + 1.6) < 0.05,
              f"probe reconstructs the world position on the quad plane ({flat['world']})")
        check(close_to(background["hdr"], CLEAR_COLOR, PROBE_TOLERANCE) and background["pick"] is None,
              f"probe on the background returns the --clear-color value ({background['hdr'][:3]})")
        check(close_to(region["rgb"]["mean"], FLAT_COLOR, PROBE_TOLERANCE) and
              region["luminance"]["max"] - region["luminance"]["min"] < PROBE_TOLERANCE,
              "probe_region over the quad: mean = its colour, flat luminance")
        check(same(before, after), "probe does not change the next capture")

        gl_errors = session.close()
    except Exception:
        session.close(quit_app=False)
        raise
    check(gl_errors == 0, f"no GL errors in the settings session ({gl_errors})")
    for name in references:
        check(same(cli[name], results[name]), f"pipe {name} == CLI {' '.join(references[name]) or '(no flags)'}")
    check(same(cli["plain"], results["plain_again"]), "restoring every setting gives the plain capture again")

    script = script_capture(fixture, "set_script_ssr_off", ["set_viewport_setting ssr off"], VIEW)
    check(same(cli["ssr_off"], script), "--ui-script set_viewport_setting ssr off == CLI --ssr off")
    script = script_capture(fixture, "set_script_material",
                            [f"set_material_param Glass {FRESNEL} fresnelOpacityOffset 0.5"], VIEW)
    check(same(cli["material_param"], script), "--ui-script set_material_param == CLI --set-asobo-param")
    script = script_capture(fixture, "set_script_exposure", ["set_environment exposure 1.5"], VIEW)
    check(same(cli["exposure"], script), "--ui-script set_environment exposure 1.5 == CLI --exposure 1.5")
    return cli


def check_persistence_busy_and_mcp(fixture):
    print("[no settings.ini from pipe commands; one client at a time; MCP shim]")
    data_dir = Path(tempfile.mkdtemp(prefix="glider_agent_"))
    try:
        session = Session(fixture, VIEW, env_extra={"GLIDER_DATA_DIR": str(data_dir)}, isolated=False)
        # Opening the startup model is the user's own action and may record a recent file; the pipe
        # commands below must leave that file exactly as it is.
        settings = data_dir / "settings.ini"
        startup = (settings.read_bytes(), settings.stat().st_mtime_ns) if settings.exists() else None
        try:
            session.call("set_viewport_setting", key="ssr", value=False)
            session.call("set_environment", key="exposure", value=2)
            refused = session.request("set_viewport_setting", key="grid_mode", value="plane")
            check(not refused["ok"] and refused["error"]["code"] == "persist_required",
                  "a saved preference needs set_persist(true)")
            session.call("open_model", path=str(HIERARCHY))
            session.call("reset")

            second = None
            try:
                other = glider_ctl.GliderPipe(session.process.pid, connect_timeout=5)
                second = other.read_reply(10)
                other.close()
            except glider_ctl.GliderError as error:
                second = {"error": {"code": error.code}}
            check(second and not second.get("ok", False) and second["error"]["code"] == "busy",
                  f"a second client is refused with busy ({second})")
            check(session.call("get_state")["glErrors"] == 0, "the first client still works after the refusal")

            # MCP shim on the same instance: the test client lets go first (one client at a time).
            session.pipe.close()
            check_mcp(session.process.pid)
            session.pipe = glider_ctl.GliderPipe(session.process.pid)
            gl_errors = session.close()
        except Exception:
            session.close(quit_app=False)
            raise
        written = sorted(p.name for p in data_dir.rglob("*"))
        after = (settings.read_bytes(), settings.stat().st_mtime_ns) if settings.exists() else None
        check(written in ([], ["settings.ini"]) and after == startup,
              f"pipe commands (settings, open_model, reset, quit) left settings.ini untouched (files: {written})")
        check(gl_errors == 0, f"no GL errors in the persistence session ({gl_errors})")
    finally:
        shutil.rmtree(data_dir, ignore_errors=True)


def check_mcp(pid):
    process = subprocess.Popen([sys.executable, str(REPO / "scripts" / "glider_mcp.py"), "--pid", str(pid)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    messages = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize",
         "params": {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "validate", "version": "1"}}},
        {"jsonrpc": "2.0", "method": "notifications/initialized"},
        {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
        {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "capture", "arguments": {"settle_frames": 2}}},
        {"jsonrpc": "2.0", "id": 4, "method": "ping"},
    ]
    stdin = "".join(json.dumps(m) + "\n" for m in messages).encode("utf-8")
    try:
        stdout, stderr = process.communicate(stdin, timeout=120)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate()
    replies = []
    valid = True
    for line in stdout.decode("utf-8").splitlines():
        try:
            replies.append(json.loads(line))
        except ValueError:
            valid = False
    by_id = {r.get("id"): r for r in replies}
    check(valid and len(replies) == 4 and all(r.get("jsonrpc") == "2.0" for r in replies),
          f"MCP: stdout holds only JSON-RPC replies ({len(replies)} replies, one per request)")
    init = by_id.get(1, {}).get("result", {})
    check(init.get("protocolVersion") == "2025-06-18" and "tools" in init.get("capabilities", {}) and
          "Command reference" in init.get("instructions", ""), "MCP initialize: version, tools capability, guide as instructions")
    tools = {t["name"]: t for t in by_id.get(2, {}).get("result", {}).get("tools", [])}
    check({"capture", "frame_node", "probe", "set_viewport_setting"} <= set(tools) and
          tools.get("capture", {}).get("inputSchema", {}).get("type") == "object", f"MCP tools/list from describe ({len(tools)} tools)")
    content = by_id.get(3, {}).get("result", {}).get("content", [])
    images = [c for c in content if c.get("type") == "image" and c.get("mimeType") == "image/png"]
    png_ok = bool(images) and base64.b64decode(images[0]["data"])[:8] == b"\x89PNG\r\n\x1a\n"
    check(png_ok and any(c.get("type") == "text" for c in content), "MCP tools/call capture returns PNG image content and text")
    check(by_id.get(4, {}).get("result") == {}, "MCP ping")
    if not png_ok:
        print(stderr.decode("utf-8", "replace")[-2000:])


def check_agent_guide():
    print("[--agent-guide]")
    guide = OUT / "GLIDER_AGENT_GUIDE.md"
    guide.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), "--agent-guide", str(guide)], cwd=REPO, capture_output=True, timeout=60)
    text = guide.read_text(encoding="utf-8") if guide.exists() else ""
    check(result.returncode == 0 and "## Command reference" in text and "| `frame_node` |" in text and
          "| `exposure` |" in text, "--agent-guide writes the guide with the generated command and settings tables")
    skill_dir = OUT / "skill"
    shutil.rmtree(skill_dir, ignore_errors=True)
    subprocess.run([str(EXE), "--agent-guide", "--format", "skill", str(skill_dir)], cwd=REPO, capture_output=True, timeout=60)
    skill = skill_dir / "glider-control" / "SKILL.md"
    check(skill.exists() and skill.read_text(encoding="utf-8").startswith("---"), "--agent-guide --format skill writes SKILL.md")


def check_baseline(baseline, fixture):
    print(f"[CLI flags routed through the registry: current build == baseline {baseline.name}]")
    cases = {
        "plain": [],
        "ssr": ["--ssr", "off", "--ssr-quality", "high", "--interior-sky-reflection", "0.25", "--windshield-icing", "0.5"],
        "asobo": ["--asobo-disable", FRESNEL, "--show-invisible-materials", "--decal-tolerance", "2"],
        "asobo_off": ["--asobo", "off", "--msfs-emissive", "off"],
        "environment": ["--exposure", "1.5", "--tone-mapping", "narkowicz", "--hdr-rotation", "45", "--time-of-day", "night",
                        "--msfs-emissive-scale", "0.01", "--bloom", "on", "--bloom-intensity", "0.2"],
        "material": ["--set-asobo-param", "Glass", FRESNEL, "fresnelOpacityOffset", "0.5"],
        "wire": ["--wireframe-overlay", "--wireframe-color", "0,1,0,0.5", "--wireframe-blend", "invert", "--msaa", "4",
                 "--post-aa", "fxaa"],
        "debug": ["--debug-view", "roughness", "--force-double-sided", "--invert-winding"],
        "render_mode": ["--render-mode", "wireframe", "--aa", "msaa2"],
    }
    for name, flags in cases.items():
        current = cli_capture(EXE, fixture, f"base_cur_{name}", VIEW + flags)
        reference = cli_capture(baseline, fixture, f"base_ref_{name}", VIEW + flags)
        check(same(current, reference), f"{name}: {' '.join(flags) or '(no flags)'}")
    for name, flags in {"select": ["--select-node", "GrandChild"], "isolate": ["--isolate-node", "GrandChild"],
                        "frame": ["--frame-node", "grand", "--frame-view", "left", "--frame-margin", "1.5"]}.items():
        current = cli_capture(EXE, HIERARCHY, f"base_cur_{name}", flags)
        reference = cli_capture(baseline, HIERARCHY, f"base_ref_{name}", flags)
        check(same(current, reference), f"{name}: {' '.join(flags)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="baseline binary for the CLI-equivalence captures")
    args = parser.parse_args()
    if not EXE.exists():
        sys.exit(f"no build at {EXE}")
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = write_quad_fixture(OUT / "agent_quads.gltf")
    check_agent_guide()
    check_exit_gate_and_paths()
    check_settings_and_probe(fixture)
    check_persistence_busy_and_mcp(fixture)
    if args.baseline_exe:
        check_baseline(Path(args.baseline_exe).resolve(), fixture)
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    for failure in failures:
        print("  FAIL " + failure)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
