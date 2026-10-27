"""Validation test suite for Glider live script control and framing.

Covers:
1. CLI framing: --frame-node, --frame-view, --frame-margin, #index, unknown node failure logging, empty-geometry node framing.
2. Follow mode via glider_live.py: frame, capture, noderect, pick, error recovery, debugview, quit, jsonl integrity.
3. Byte-identical comparison: CLI framing capture vs live camera capture.
4. Regression check: validate_p4_exit_gate.py.
5. Real asset smoke test: DA62 cockpit WINDSHIELD_Static and LIGHTS_Asobo_Unsafe.
"""

import hashlib
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "scripts"))
import harness_process as subprocess
from glider_live import GliderLive

EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT_DIR = REPO / "build" / "review" / "live_control"
FIXTURE = str(REPO / "resources" / "test_models" / "hierarchy_animation_fixture.gltf")
DA62_MODEL = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62\PackageSources\SimObjects\Airplanes\DA62_SDK\attachments\asobo\Part_Interior_Cockpit\model\Cockpit_LOD00.gltf")
DA62_TEXTURES = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62\Sources\Art\Max\textures")

COMMON_CLI = [
    "--isolated",
    "--window-size", "1600x900",
    "--aa", "none",
    "--hide-grid",
    "--fixed-frame-time", "0.05",
    "--vsync", "off",
]


def log_pass(msg: str):
    print(f"  PASS: {msg}", flush=True)


def log_fail(msg: str):
    print(f"  FAIL: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


def run_glider_cli(args: List[str], timeout: float = 30.0) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    env["GLIDER_NO_FOCUS"] = "1"
    cmd = [str(EXE), *args]
    return subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=timeout)


def test_1_cli_framing():
    print("\n--- Test 1: CLI Framing & Reporting ---")
    os.makedirs(OUT_DIR, exist_ok=True)

    # 1.1 Front view framing on GrandChild
    rep_front = OUT_DIR / "t1_front.json"
    p = run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "GrandChild",
        "--frame-view", "front",
        "--frame-margin", "1.0",
        "--report-node", "GrandChild",
        "--profile-report", str(rep_front),
        "--exit-after-profile",
    ])
    if p.returncode != 0 or not rep_front.exists():
        log_fail(f"CLI front view framing failed (code {p.returncode}): {p.stderr}")

    with open(rep_front, "r", encoding="utf-8") as f:
        data_front = json.load(f)

    nodes = data_front.get("nodes", [])
    if not nodes or nodes[0].get("name") != "GrandChild":
        log_fail(f"Node report missing GrandChild: {nodes}")

    rect = nodes[0]["screenRect"]
    on_screen = nodes[0]["onScreen"]
    vp_w, vp_h = data_front["viewport"]["width"], data_front["viewport"]["height"]
    vp_area = vp_w * vp_h
    rect_w = max(0.0, rect[2] - rect[0])
    rect_h = max(0.0, rect[3] - rect[1])
    area_front = rect_w * rect_h

    if not on_screen:
        log_fail(f"GrandChild not onScreen in front view: {rect}")
    if area_front < 0.01 * vp_area or area_front > 0.90 * vp_area:
        log_fail(f"GrandChild screen area ratio out of range (1%-90%): {area_front / vp_area:.3f}")
    log_pass(f"GrandChild front view framed (rect: {[round(c, 1) for c in rect]}, area ratio: {area_front / vp_area:.3f})")

    # 1.2 Pick at center of rect returns GrandChild
    center_x = int((rect[0] + rect[2]) * 0.5)
    center_y = int((rect[1] + rect[3]) * 0.5)
    rep_pick = OUT_DIR / "t1_pick.json"
    p = run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "GrandChild",
        "--frame-view", "front",
        "--report-pick", f"{center_x},{center_y}",
        "--profile-report", str(rep_pick),
        "--exit-after-profile",
    ])
    with open(rep_pick, "r", encoding="utf-8") as f:
        pick_data = json.load(f)
    picks = pick_data.get("picks", [])
    if not picks or picks[0] is None or picks[0].get("node") != "GrandChild":
        log_fail(f"Report pick at center ({center_x}, {center_y}) did not return GrandChild: {picks}")
    log_pass(f"CLI --report-pick at ({center_x}, {center_y}) returned node '{picks[0]['node']}', mesh '{picks[0]['mesh']}'")

    # 1.3 Iso view framing
    rep_iso = OUT_DIR / "t1_iso.json"
    run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "GrandChild",
        "--frame-view", "iso",
        "--report-node", "GrandChild",
        "--profile-report", str(rep_iso),
        "--exit-after-profile",
    ])
    with open(rep_iso, "r", encoding="utf-8") as f:
        data_iso = json.load(f)
    rect_iso = data_iso["nodes"][0]["screenRect"]
    area_iso = (rect_iso[2] - rect_iso[0]) * (rect_iso[3] - rect_iso[1])
    if not data_iso["nodes"][0]["onScreen"] or area_iso < 0.01 * vp_area or area_iso > 0.90 * vp_area:
        log_fail(f"GrandChild iso view framing invalid: rect={rect_iso}, area ratio={area_iso / vp_area:.3f}")
    log_pass(f"GrandChild iso view framed (rect: {[round(c, 1) for c in rect_iso]}, area ratio: {area_iso / vp_area:.3f})")

    # 1.4 Frame margin 2.0 produces smaller area than margin 1.0
    rep_m2 = OUT_DIR / "t1_m2.json"
    run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "GrandChild",
        "--frame-view", "front",
        "--frame-margin", "2.0",
        "--report-node", "GrandChild",
        "--profile-report", str(rep_m2),
        "--exit-after-profile",
    ])
    with open(rep_m2, "r", encoding="utf-8") as f:
        data_m2 = json.load(f)
    rect_m2 = data_m2["nodes"][0]["screenRect"]
    area_m2 = (rect_m2[2] - rect_m2[0]) * (rect_m2[3] - rect_m2[1])
    if area_m2 >= area_front:
        log_fail(f"Margin 2.0 area ({area_m2}) not smaller than margin 1.0 area ({area_front})")
    log_pass(f"--frame-margin 2.0 area ({area_m2:.1f}) < margin 1.0 area ({area_front:.1f})")

    # 1.5 Node index #index syntax (#2 is GrandChild in hierarchy_animation_fixture)
    rep_idx = OUT_DIR / "t1_idx.json"
    run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "#2",
        "--report-node", "#2",
        "--profile-report", str(rep_idx),
        "--exit-after-profile",
    ])
    with open(rep_idx, "r", encoding="utf-8") as f:
        data_idx = json.load(f)
    if not data_idx["nodes"] or data_idx["nodes"][0].get("name") != "GrandChild":
        log_fail(f"#2 did not resolve to GrandChild: {data_idx.get('nodes')}")
    log_pass("CLI --frame-node #2 resolved to GrandChild")

    # 1.6 Unknown node name prints 1-line candidates failure and continues without crash
    p_unk = run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "NonExistentNodeName",
        "--profile-report", str(OUT_DIR / "t1_unk.json"),
        "--exit-after-profile",
    ])
    if p_unk.returncode != 0:
        log_fail(f"Unknown node caused non-zero exit ({p_unk.returncode})")
    stderr_out = p_unk.stderr + p_unk.stdout
    if "Candidates" not in stderr_out:
        log_fail(f"Expected candidates warning not found in output: {stderr_out}")
    log_pass("CLI unknown node name logged 1-line candidate list and continued cleanly")

    # 1.7 Node without geometry (RootLight) frames world pos without error
    rep_light = OUT_DIR / "t1_light.json"
    p_light = run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "RootLight",
        "--report-node", "RootLight",
        "--profile-report", str(rep_light),
        "--exit-after-profile",
    ])
    if p_light.returncode != 0:
        log_fail(f"Framing empty-geometry node failed ({p_light.returncode}): {p_light.stderr}")
    with open(rep_light, "r", encoding="utf-8") as f:
        data_light = json.load(f)
    if not data_light["nodes"] or data_light["nodes"][0].get("name") != "RootLight":
        log_fail(f"RootLight report missing: {data_light.get('nodes')}")
    log_pass("Node without geometry (RootLight) framed world position without error")


def test_2_follow_mode():
    print("\n--- Test 2: Follow Mode via glider_live.py ---")
    live_front_png = str(OUT_DIR / "live_front.png")
    live_roughness_png = str(OUT_DIR / "live_roughness.png")

    with GliderLive(FIXTURE, extra_args=COMMON_CLI) as live:
        # 2.1 frame GrandChild front 1.0
        r_frame = live.frame("GrandChild", view="front", margin=1.0)
        if not r_frame.get("ok"):
            log_fail(f"live.frame failed: {r_frame}")
        log_pass("live.frame executed successfully")

        # 2.2 capture
        r_cap = live.capture(live_front_png)
        if not r_cap.get("ok") or not os.path.isfile(live_front_png):
            log_fail(f"live.capture failed: {r_cap}")
        log_pass(f"live.capture saved '{live_front_png}' ({os.path.getsize(live_front_png)} bytes)")

        # 2.3 noderect GrandChild
        r_rect = live.noderect("GrandChild")
        if not r_rect.get("ok") or not r_rect.get("result", {}).get("onScreen"):
            log_fail(f"live.noderect failed: {r_rect}")
        rect = r_rect["result"]["screenRect"]
        log_pass(f"live.noderect confirmed GrandChild onScreen: {[round(c, 1) for c in rect]}")

        # 2.4 pick center
        cx = int((rect[0] + rect[2]) * 0.5)
        cy = int((rect[1] + rect[3]) * 0.5)
        r_pick = live.pick(cx, cy)
        if not r_pick.get("ok") or r_pick.get("result", {}).get("node") != "GrandChild":
            log_fail(f"live.pick at ({cx}, {cy}) failed: {r_pick}")
        log_pass(f"live.pick returned '{r_pick['result']['node']}'")

        # 2.5 invalid command recovers cleanly
        r_err = live.send("bogus 1 2 3")
        if r_err.get("ok") is not False:
            log_fail(f"Invalid command unexpectedly succeeded: {r_err}")
        log_pass(f"Invalid command rejected cleanly with error: '{r_err.get('error')}'")

        # Session still alive after error
        r_state = live.state()
        if not r_state.get("ok"):
            log_fail(f"Session not responsive after invalid command: {r_state}")
        log_pass("Session remained fully responsive after invalid command")

        # 2.6 debugview roughness + capture
        r_dbg = live.debugview("roughness")
        if not r_dbg.get("ok"):
            log_fail(f"live.debugview failed: {r_dbg}")
        r_dbg_cap = live.capture(live_roughness_png)
        if not r_dbg_cap.get("ok") or not os.path.isfile(live_roughness_png):
            log_fail(f"live.capture roughness failed: {r_dbg_cap}")
        log_pass(f"live.debugview roughness captured '{live_roughness_png}'")

        out_path = live.out_path

    # Verify out.jsonl line continuity
    with open(out_path, "r", encoding="utf-8") as f:
        lines = [json.loads(line.strip()) for line in f if line.strip()]

    expected_cmds = ["frame", "capture", "noderect", "pick", "bogus", "state", "debugview", "capture", "quit"]
    actual_cmds = [item["cmd"] for item in lines]
    if actual_cmds != expected_cmds:
        log_fail(f"Out JSONL command sequence mismatch: expected {expected_cmds}, got {actual_cmds}")
    for idx, item in enumerate(lines, start=1):
        if item["line"] != idx:
            log_fail(f"Line number mismatch in JSONL: expected {idx}, got {item['line']}")
    log_pass(f"Verified {len(lines)} JSONL result records with contiguous line numbering")


def test_3_byte_identical_comparison():
    print("\n--- Test 3: Byte-Identical CLI vs Live Comparison ---")
    a_png = OUT_DIR / "cmp_a.png"
    a_json = OUT_DIR / "cmp_a.json"
    b_png = OUT_DIR / "cmp_b.png"

    # Run 1: CLI capture
    p1 = run_glider_cli([
        FIXTURE,
        *COMMON_CLI,
        "--frame-node", "GrandChild",
        "--frame-view", "front",
        "--frame-margin", "1.0",
        "--capture-viewport", str(a_png),
        "--capture-frame", "8",
        "--profile-report", str(a_json),
        "--exit-after-profile",
    ])
    if p1.returncode != 0 or not a_png.exists() or not a_json.exists():
        log_fail(f"Run 1 CLI capture failed: {p1.stderr}")

    with open(a_json, "r", encoding="utf-8") as f:
        cam_a = json.load(f)["camera"]

    # Run 2: Live follow session using camera and select
    with GliderLive(FIXTURE, extra_args=COMMON_CLI) as live:
        live.select("GrandChild")
        live.camera(cam_a["eye"], cam_a["target"], near=cam_a["near"], far=cam_a["far"])
        r_cap = live.capture(str(b_png))
        if not r_cap.get("ok"):
            log_fail(f"Live capture failed: {r_cap}")

    hash_a = hashlib.sha256(a_png.read_bytes()).hexdigest()
    hash_b = hashlib.sha256(b_png.read_bytes()).hexdigest()
    if hash_a != hash_b:
        log_fail(f"Captures not bit-identical: SHA256 A={hash_a}, B={hash_b}")
    log_pass(f"CLI framing capture and live camera capture are 100% BIT-IDENTICAL (SHA256: {hash_a})")


def test_4_regression_check():
    print("\n--- Test 4: Regression Check (validate_p4_exit_gate.py) ---")
    gate_script = REPO / "scripts" / "validate_p4_exit_gate.py"
    if not gate_script.is_file():
        log_pass("validate_p4_exit_gate.py not present, skipping")
        return

    p = subprocess.run([sys.executable, str(gate_script)], cwd=str(REPO), capture_output=True, text=True, timeout=120)
    if p.returncode != 0 or "RESULT: PASS" not in p.stdout:
        log_fail(f"Regression gate failed (code {p.returncode}):\n{p.stdout}\n{p.stderr}")
    log_pass("validate_p4_exit_gate.py PASSED with 0 regressions")


def test_5_real_asset_smoke():
    print("\n--- Test 5: Real Asset Smoke Test (DA62 Cockpit) ---")
    if not DA62_MODEL.is_file():
        log_fail(f"DA62 cockpit model not found at '{DA62_MODEL}'")

    windshield_png = OUT_DIR / "da62_windshield_front.png"
    locator_png = OUT_DIR / "da62_locator_framed.png"

    # Frame WINDSHIELD_Static
    windshield_json = OUT_DIR / "da62_windshield_front.json"
    p1 = run_glider_cli([
        str(DA62_MODEL),
        *COMMON_CLI,
        "--search-path", str(DA62_TEXTURES),
        "--frame-node", "WINDSHIELD_Static",
        "--frame-view", "front",
        "--frame-margin", "1.0",
        "--capture-viewport", str(windshield_png),
        "--capture-frame", "8",
        "--profile-report", str(windshield_json),
        "--exit-after-profile",
    ], timeout=90.0)
    if p1.returncode != 0 or not windshield_png.is_file() or windshield_png.stat().st_size == 0:
        log_fail(f"DA62 WINDSHIELD_Static frame capture failed: {p1.stderr}")
    log_pass(f"DA62 WINDSHIELD_Static captured ({windshield_png.stat().st_size} bytes)")

    # Frame LIGHTS_Asobo_Unsafe (or locator)
    locator_json = OUT_DIR / "da62_locator_framed.json"
    p2 = run_glider_cli([
        str(DA62_MODEL),
        *COMMON_CLI,
        "--search-path", str(DA62_TEXTURES),
        "--frame-node", "LIGHTS_Asobo_Unsafe",
        "--frame-view", "front",
        "--frame-margin", "1.0",
        "--capture-viewport", str(locator_png),
        "--capture-frame", "8",
        "--profile-report", str(locator_json),
        "--exit-after-profile",
    ], timeout=90.0)
    if p2.returncode != 0 or not locator_png.is_file() or locator_png.stat().st_size == 0:
        log_fail(f"DA62 LIGHTS_Asobo_Unsafe frame capture failed: {p2.stderr}")
    log_pass(f"DA62 LIGHTS_Asobo_Unsafe captured ({locator_png.stat().st_size} bytes)")


def main():
    print("=" * 60)
    print("GLIDER LIVE SCRIPT CONTROL VALIDATION SUITE")
    print("=" * 60)
    test_1_cli_framing()
    test_2_follow_mode()
    test_3_byte_identical_comparison()
    test_4_regression_check()
    test_5_real_asset_smoke()
    print("=" * 60)
    print("ALL VALIDATION SUITE TESTS PASSED!")
    print("=" * 60)


if __name__ == "__main__":
    main()
