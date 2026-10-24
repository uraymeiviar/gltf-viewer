# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P6.2 bounded parallel image decode validation.

Images were already decoded in parallel (std::async per image on MSVC's thread pool).
P6.2 adds (a) a fix for libktx's unguarded Basis transcoder initialisation (the first
KTX2 transcode runs alone) and (b) a decode memory budget in imageformats::decodeImage.

Sections:
  equivalence  Every asset is rendered under several budgets (--decode-budget-mib) on the
               candidate, on the binary before the KTX fix (build/p6.2/pre) and on the
               binary before the budget (build/p6.2/pre_budget). Viewport captures must be
               byte-identical across all of them. Small budgets must make decodes wait and
               keep the estimated in-flight bytes within the budget (a single larger image
               runs alone). Assets: 8 x 8192^2 PNG, 96 x 1024^2 PNG (GLB and glTF), 48 KTX2
               (Basis), BarramundiFish (KTX2), Fox, MaterialsVariantsShoe, DDS and TGA
               scenarios, and a failure fixture (corrupt PNG, corrupt KTX2, missing file)
               that must load, not hang, under a tiny budget.
  tradeoff     Peak working set and decode time on the 8 x 8192^2 asset per budget.
"""

import argparse
import hashlib
import json
import shutil
import statistics
import harness_process as subprocess
import sys
import time
from pathlib import Path

import psutil

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
PRE = REPO / "build" / "p6.2" / "pre" / "Glider.exe"
PRE_BUDGET = REPO / "build" / "p6.2" / "pre_budget" / "Glider.exe"
OUT = REPO / "build" / "p6.2"
FIX = REPO / "build" / "p6" / "fixtures"
RES = "OpenGL-Renderer/Tools/tools/operations/tests/resources"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["equivalence", "tradeoff"])
parser.add_argument("--runs", type=int, default=3)
args = parser.parse_args()
failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fixtures():
    if not (FIX / "big_textures" / "big.gltf").exists():
        subprocess.run([sys.executable, str(REPO / "scripts/generate_big_texture_fixture.py"), str(FIX / "big_textures"), "8", "8192"], check=True)
    if not (FIX / "many_images" / "load.glb").exists():
        subprocess.run([sys.executable, str(REPO / "scripts/generate_load_fixture.py"), str(FIX / "many_images"), "--nodes", "200",
                        "--images", "96", "--size", "1024"], check=True)
    subprocess.run([sys.executable, str(REPO / "scripts/generate_ktx2_stress_fixture.py"), str(FIX / "ktx2_stress"), "48"], check=True)
    formats = OUT / "formats"
    formats.mkdir(parents=True, exist_ok=True)
    for script in ("setup_msfs_dds_test.py", "setup_tga_test.py"):
        subprocess.run([sys.executable, str(REPO / "scripts" / script)], cwd=formats, check=True, capture_output=True,
                       env={**__import__("os").environ, "PYTHONPATH": str(REPO / "scripts")})
    # Failure fixture: the KTX2 stress asset with a corrupt PNG, a corrupt KTX2 and a missing file.
    broken = FIX / "broken_images"
    if broken.exists():
        shutil.rmtree(broken)
    shutil.copytree(FIX / "ktx2_stress", broken)
    doc = json.loads((broken / "stress.gltf").read_text(encoding="utf-8"))
    (broken / "corrupt.png").write_bytes(b"\x89PNG\r\n\x1a\n" + b"\x00\x00\x00\x0dIHDR" + b"\xff" * 64)
    doc["images"][0] = {"uri": "corrupt.png"}
    doc["textures"][0] = {"sampler": 0, "source": 0}
    (broken / "tex_1.ktx2").write_bytes((broken / "tex_1.ktx2").read_bytes()[:200])
    doc["images"][2]["uri"] = "missing.ktx2"
    (broken / "broken.gltf").write_text(json.dumps(doc), encoding="utf-8")
    dds = next(formats.rglob("test_dds_scenario/model/*.gltf"))
    tga = next(formats.rglob("test_tga_scenario/model/*.gltf"))
    return {
        "big_png": (FIX / "big_textures/big.gltf", ["--camera-distance", "6"], [0, 2048, 512]),
        "many_png_glb": (FIX / "many_images/load.glb", [], [0, 64]),
        "many_png_gltf": (FIX / "many_images/load.gltf", [], [0, 64]),
        "ktx2_stress": (FIX / "ktx2_stress/stress.gltf", ["--camera-distance", "9"], [0, 16]),
        "barramundi": (REPO / RES / "BarramundiFish/ktx2/BarramundiFish.mixed.glb", [], [0, 16]),
        "fox": (REPO / RES / "Fox/glb/Fox.glb", [], [0, 1]),
        "shoe": (next((REPO / RES / "MaterialsVariantsShoe").rglob("*.glb")), [], [0, 16]),
        "dds": (dds, [], [0, 1]),
        "tga": (tga, [], [0, 1]),
        "broken": (broken / "broken.gltf", ["--camera-distance", "9"], [0, 1]),
    }


def run(exe, asset, flags, label, timeout=300):
    folder = OUT / "runs"
    folder.mkdir(parents=True, exist_ok=True)
    cap, prof = folder / f"{label}.png", folder / f"{label}.json"
    for f in (cap, prof):
        f.unlink(missing_ok=True)
    cmd = [str(exe), str(asset), "--isolated", "--window-size", "1280x800", "--aa", "none", "--profile",
           "--capture-viewport", str(cap), "--capture-frame", "5", "--profile-report", str(prof), "--exit-after-profile"] + flags
    proc = subprocess.Popen(cmd, cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ps = psutil.Process(proc.pid)
    peak, start = 0, time.time()
    while proc.poll() is None:
        if time.time() - start > timeout:
            proc.kill()
            return None
        try:
            peak = max(peak, ps.memory_info().peak_wset)
        except psutil.Error:
            break
        time.sleep(0.01)
    if not cap.exists() or not prof.exists():
        return None
    startup = json.loads(prof.read_text(encoding="utf-8"))["startup"]
    return {"sha": sha(cap), "startup": startup, "peak": peak, "png": cap}


if args.only in (None, "equivalence"):
    print("[decoded output identical across budgets and builds]")
    for name, (asset, extra, budgets) in fixtures().items():
        ref = run(PRE, asset, extra, f"{name}_pre")
        ref_b = run(PRE_BUDGET, asset, extra, f"{name}_prebudget")
        if ref is None or ref_b is None:
            check(False, f"{name}: reference runs completed")
            continue
        for budget in budgets:
            cand = run(EXE, asset, extra + ["--decode-budget-mib", str(budget)], f"{name}_b{budget}", timeout=120)
            if cand is None:
                check(False, f"{name} budget {budget} MiB: completed without hanging")
                continue
            s = cand["startup"]
            ok = cand["sha"] == ref["sha"] == ref_b["sha"]
            check(ok, f"{name} budget {budget} MiB: capture identical to pre-KTX-fix and pre-budget binaries "
                      f"(waits {s['decodeBudgetWaits']}, peak in flight {s['decodePeakInFlightCount']} / "
                      f"{s['decodePeakInFlightBytes'] / 2 ** 20:.0f} MiB)")
            if budget > 0 and s["decodePeakInFlightCount"] > 1:
                check(s["decodePeakInFlightBytes"] <= budget * 2 ** 20,
                      f"{name} budget {budget} MiB: estimated in-flight bytes stayed within the budget")
            if name in ("big_png", "many_png_glb", "ktx2_stress") and 0 < budget <= 64:
                check(s["decodeBudgetWaits"] > 0, f"{name} budget {budget} MiB: decodes waited for budget")

if args.only in (None, "tradeoff"):
    asset = FIX / "big_textures/big.gltf"
    print(f"[8 x 8192^2 PNG: peak working set and decode time per budget, {args.runs} runs each]")
    rows = {}
    for budget in (0, 4096, 2048, 1024, 512):
        res = [run(EXE, asset, ["--camera-distance", "6", "--decode-budget-mib", str(budget)], f"tradeoff_{budget}_{i}") for i in range(args.runs)]
        peaks = [r["peak"] / 2 ** 30 for r in res]
        decode = [r["startup"]["loadDecodeMs"] for r in res]
        rows[budget] = {"peak_gib": peaks, "decode_ms": decode, "in_flight": res[0]["startup"]["decodePeakInFlightCount"]}
        print(f"    budget {budget or 'unlimited'} MiB: peak {min(peaks):.2f}-{max(peaks):.2f} GiB, decode "
              f"{min(decode):.0f}-{max(decode):.0f} ms, max concurrent decodes {rows[budget]['in_flight']}")
    (OUT / "tradeoff.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")
    base = statistics.median(rows[0]["peak_gib"])
    check(statistics.median(rows[1024]["peak_gib"]) < base, "a 1 GiB budget lowers the peak working set")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
