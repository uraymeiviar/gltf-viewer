# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""P6.1 validation source reuse: GLB validation reads ranges instead of the whole file.

Sections:
  reports    For each asset (valid GLBs incl. the embedded HelloCube, text glTFs and
             malformed GLB containers the loader still accepts), the complete validation
             report (issues, codes, messages, pointers, target mapping, counts; no timings)
             must be identical between the candidate and --validation-full-read (the
             original whole-file read). Whether the asset loaded at all must match too.
  benchmark  Large GLB (scripts/generate_load_fixture.py): validation read time, bytes
             read, total validation time and peak working set, 5 alternating pairs.
"""

import argparse
import json
import statistics
import struct
import harness_process as subprocess
import sys
import time
from pathlib import Path

import psutil

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "p6.1"
RES = "OpenGL-Renderer/Tools/tools/operations/tests/resources"
LARGE = "build/p6/fixtures/large/load.glb"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["reports", "benchmark"])
parser.add_argument("--pairs", type=int, default=5)
args = parser.parse_args()
failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def make_cases():
    folder = OUT / "cases"
    folder.mkdir(parents=True, exist_ok=True)
    fox = (REPO / RES / "Fox/glb/Fox.glb").read_bytes()
    magic, version, length = struct.unpack_from("<4sII", fox, 0)
    j_len, j_type = struct.unpack_from("<I4s", fox, 12)
    b_off = 20 + j_len
    cases = {}

    def put(name, data):
        path = folder / f"{name}.glb"
        path.write_bytes(data)
        cases[name] = path

    put("ok_copy", fox)
    put("length_larger", fox[:8] + struct.pack("<I", length + 100) + fox[12:])
    put("length_smaller", fox[:8] + struct.pack("<I", length - 4) + fox[12:])
    put("trailing_bytes", fox + b"\0" * 16)
    put("truncated_bin", fox[:len(fox) - 1000])
    put("bin_wrong_type", fox[:b_off + 4] + b"XXXX" + fox[b_off + 8:])
    put("version_1", fox[:4] + struct.pack("<I", 1) + fox[8:])
    put("bad_magic", b"gLTF" + fox[4:])
    put("chunk_past_end", fox[:12] + struct.pack("<I", len(fox)) + fox[16:])
    put("json_chunk_type", fox[:16] + b"JSOX" + fox[20:])
    put("tiny_8", fox[:8])
    put("tiny_16", fox[:16])
    # JSON chunk not padded to 4 bytes: drop trailing spaces, so the BIN chunk header moves.
    js = fox[20:b_off].rstrip(b" ")
    if len(js) % 4 == 0:
        js = js + b"\n"
    unpadded = fox[:12] + struct.pack("<I4s", len(js), b"JSON") + js + fox[b_off:]
    put("json_unpadded", unpadded[:8] + struct.pack("<I", len(unpadded)) + unpadded[12:])
    bad_json = bytearray(fox)
    bad_json[20] = ord("[")
    put("json_syntax_error", bytes(bad_json))
    # Semantic errors (normal count mismatch, out-of-range index); the script writes build/invalid_test.glb.
    subprocess.run([sys.executable, str(REPO / "scripts" / "make_invalid_test_glb.py")], cwd=REPO, capture_output=True, check=True)
    put("semantic_errors", (REPO / "build" / "invalid_test.glb").read_bytes())
    return cases


def run(asset, label, flags, profile=False):
    folder = OUT / "runs"
    folder.mkdir(parents=True, exist_ok=True)
    report = folder / f"{label}.json"
    prof = folder / f"{label}_profile.json"
    for f in (report, prof):
        f.unlink(missing_ok=True)
    cmd = [str(EXE), str(asset), "--isolated", "--window-size", "1280x800", "--validation-report", str(report),
           "--profile", "--capture-viewport", str(folder / f"{label}.png"), "--capture-frame", "3",
           "--profile-report", str(prof), "--exit-after-profile"] + flags
    proc = subprocess.Popen(cmd, cwd=REPO, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    ps = psutil.Process(proc.pid)
    peak = 0
    while proc.poll() is None:
        try:
            peak = max(peak, ps.memory_info().peak_wset)
        except psutil.Error:
            break
        time.sleep(0.02)
    out = proc.stdout.read()
    loaded = "Loaded successfully" in out
    data = report.read_text(encoding="utf-8") if report.exists() else None
    startup = json.loads(prof.read_text(encoding="utf-8"))["startup"] if prof.exists() else {}
    return {"loaded": loaded, "report": data, "startup": startup, "peak": peak, "exit": proc.returncode}


if args.only in (None, "reports"):
    print("[complete reports: candidate vs --validation-full-read]")
    assets = {"hellocube_embedded": "resources/sampleGltf/HelloCube.glb",
              "fox": f"{RES}/Fox/glb/Fox.glb",
              "barramundi_ktx2": f"{RES}/BarramundiFish/ktx2/BarramundiFish.mixed.glb",
              "shoe_variants": next(str(p.relative_to(REPO)) for p in (REPO / RES / "MaterialsVariantsShoe").rglob("*.glb")),
              "large_glb": LARGE, "large_gltf": "build/p6/fixtures/large/load.gltf",
              "fixture_gltf": "resources/test_models/production_passes_fixture.gltf"}
    assets.update({f"case_{k}": str(v.relative_to(REPO)) for k, v in make_cases().items()})
    loaded_count = 0
    for name, asset in assets.items():
        ref = run(asset, f"{name}_full", ["--validation-full-read"])
        cand = run(asset, f"{name}_range", [])
        same = ref["loaded"] == cand["loaded"] and ref["report"] == cand["report"]
        summary = "not loaded" if not cand["loaded"] else (
            f"{json.loads(cand['report'])['errors']} errors, {json.loads(cand['report'])['warnings']} warnings, "
            f"{len(json.loads(cand['report'])['issues'])} issues" if cand["report"] else "loaded, no report")
        check(same, f"{name}: identical report ({summary})")
        loaded_count += cand["loaded"]
        if cand["loaded"] and asset.endswith(".glb") and cand["startup"]:
            br, bf = cand["startup"].get("validationBytesRead"), ref["startup"].get("validationBytesRead")
            print(f"      bytes read: full {bf} -> ranges {br}")
    check(loaded_count >= 10, f"{loaded_count} assets loaded and validated")

if args.only in (None, "reports"):
    print("[validator alone (--validate-file), every container case incl. those the loader rejects]")
    folder = OUT / "direct"
    folder.mkdir(parents=True, exist_ok=True)
    for name, path in sorted(make_cases().items()) + [("large_glb", REPO / LARGE)]:
        reports = {}
        for mode, flags in (("full", ["--validation-full-read"]), ("range", [])):
            out = folder / f"{name}_{mode}.json"
            out.unlink(missing_ok=True)
            subprocess.run([str(EXE), "--validate-file", str(path), "--validation-report", str(out)] + flags,
                           cwd=REPO, capture_output=True, timeout=120)
            reports[mode] = out.read_text(encoding="utf-8") if out.exists() else None
        doc = json.loads(reports["range"]) if reports["range"] else None
        codes = sorted({i["code"] for i in doc["issues"]}) if doc else "no report"
        check(reports["full"] is not None and reports["full"] == reports["range"], f"direct {name}: identical report {codes}")

if args.only in (None, "benchmark"):
    print(f"[large GLB, {args.pairs} alternating pairs: --validation-full-read vs ranges]")
    rows = {"full": [], "range": []}
    for i in range(args.pairs):
        for mode in (("full", "range") if i % 2 == 0 else ("range", "full")):
            r = run(LARGE, f"bench_{mode}_{i}", ["--validation-full-read"] if mode == "full" else [])
            s = r["startup"]
            stages = {st["name"]: st["ms"] for st in s.get("stages", [])}
            rows[mode].append({"validation": s["validationMs"], "read": stages.get("validation.read"),
                               "bytes": s["validationBytesRead"], "peak": r["peak"] / 2 ** 20,
                               "total": s["totalStartupMs"]})
            print(f"    {mode} {i}: validation {s['validationMs']:.1f} ms (read {stages.get('validation.read', 0):.1f}), "
                  f"bytes {s['validationBytesRead']}, peak {r['peak'] / 2 ** 20:.0f} MiB, startup {s['totalStartupMs']:.0f} ms")
    for key, unit in (("validation", "ms"), ("read", "ms"), ("peak", "MiB"), ("total", "ms")):
        p, c = [r[key] for r in rows["full"]], [r[key] for r in rows["range"]]
        noise = (max(p) - min(p)) / statistics.median(p)
        deltas = [(a - b) / a for a, b in zip(p, c)]
        med = statistics.median(deltas)
        print(f"    {key}: full {min(p):.1f}-{max(p):.1f} -> ranges {min(c):.1f}-{max(c):.1f} {unit}, "
              f"median paired {med:+.1%} improvement, {sum(d > 0 for d in deltas)}/{len(deltas)}, noise {noise:.1%}")
        if key == "validation":
            check(med > max(0.03, noise) and sum(d > 0 for d in deltas) >= len(deltas) - 1,
                  "validation time: improvement exceeds 3% and noise")
        if key == "peak":
            check(med > 0, "peak working set does not grow")
    (OUT / "bench_summary.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
