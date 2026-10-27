"""Focused loading/adaptive checks; never copies executables or runs the regression runner.

Run after make_heavy_fixture.py. --measure also records baseline/current stage reports.
Uses actual VRAM capping and allocation-branch injection; never weakens existing checks.
"""
import argparse
import json
import os
from pathlib import Path

import harness_process as subprocess
from measure_loading import ROOT, COCKPIT, run

OUT = ROOT/"build/review/loading"
EXE = ROOT/"build/bin/Release/Glider.exe"
BASE = ROOT/"build/baselines/3176a16.exe"
HEAVY = ROOT/"build/review/heavy/heavy.gltf"
CABIN = Path(r"F:\new-fss-aircraft-ejet\msfs-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cabin-LOD00.gltf")


def check(ok, message):
    print(("PASS " if ok else "FAIL ")+message, flush=True)
    if not ok:
        raise AssertionError(message)


def hooked(name, env, model=HEAVY, extra=()):
    report, png, log = (OUT/f"{name}.{ext}" for ext in ("json", "png", "log"))
    args = [str(EXE), str(model), "--isolated", "--vsync", "off", "--capture-viewport", str(png),
            "--capture-frame", "12", "--profile-report", str(report), "--exit-after-profile", *extra]
    with log.open("wb") as stream:
        result = subprocess.run(args, cwd=ROOT, env={**os.environ, **env}, stdout=stream, stderr=stream, timeout=600)
    check(result.returncode == 0 and png.exists(), f"{name}: exits 0 and renders")
    text = log.read_text(encoding="utf-8", errors="replace")
    check("GL automation errors: 0" in text, f"{name}: zero GL errors")
    return json.loads(report.read_text(encoding="utf-8"))["startup"], text


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--measure", action="store_true")
    p.add_argument("--cabin", action="store_true")
    p.add_argument("--wings", action="store_true")
    a = p.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if a.measure:
        for model in (COCKPIT, HEAVY):
            run(BASE, model, "baseline")
            run(EXE, model, "final")
    for model in ("e195", "heavy"):
        baseline = OUT/f"baseline_{model}.png"
        current = OUT/f"final_{model}.png"
        check(baseline.exists() and current.exists(), f"{model}: measurement captures exist (use --measure)")
        check(baseline.read_bytes() == current.read_bytes(), f"{model}: isolated capture byte-identical to 3176a16")
        report = json.loads((OUT/f"final_{model}.json").read_text(encoding="utf-8"))["startup"]
        check(report["autoQuality"] is False, f"{model}: Auto quality disabled for captures")
        check(report["loadingProgressFrames"] > 0, f"{model}: progress frames rendered")
    auto = run(EXE, COCKPIT, "adaptive", auto="on")["startup"]
    check(auto["autoQuality"] and auto["autoQualityStep"] == 6, "E195 reaches configured quality within six seconds")
    limited = run(EXE, HEAVY, "vram1024", auto="on", vram=1024)["startup"]
    check(limited["reducedTextures"] >= 16 and limited["textureMaxDimension"] < 8192, "1 GiB cap reduces all sixteen 8K textures")
    check(limited["autoQualityStep"] < 6 and "VRAM" in limited["autoQualityStatus"], "1 GiB cap visibly limits adaptive quality")
    check(limited["allocationFailures"] == 0, "preflight prevents GL allocation failures")
    failure, text = hooked("texture_allocation", {"GLIDER_TEST_TEXTURE_ALLOCATION_FAILURE": "1"})
    check(failure["allocationFailures"] == 1 and "1x1 fallback" in text, "allocation failure degrades to a complete texture")
    _, text = hooked("upload_allocation", {"GLIDER_TEST_LOAD_FAILURE": "upload"})
    check("load stopped safely" in text, "geometry allocation failure leaves a usable empty scene")
    malformed = OUT/"malformed.gltf"
    malformed.write_text("{ this is not JSON", encoding="utf-8")
    _, text = hooked("malformed", {}, model=malformed)
    check("load stopped safely" in text, "malformed input clears renderer pointers safely")
    _, text = hooked("queued", {"GLIDER_TEST_QUEUE_LOAD": "resources/sampleGltf/HelloCube.glb"})
    check("queued a model during image decode" in text and "Loaded successfully: resources/sampleGltf/HelloCube.glb" in text,
          "a request during decode is queued and loaded safely")
    pins, _ = hooked("explicit_quality", {"GLIDER_TEST_VRAM_MB": "1024"}, model=Path("resources/sampleGltf/HelloCube.glb"),
        extra=("--auto-quality", "on", "--ibl-size", "1024", "--render-scale", "1", "--msaa", "4", "--post-aa", "taa", "--ssr", "on", "--bloom", "on"))
    check(pins["effectiveIblCubeSize"] == 1024 and pins["effectiveRenderScale"] == 1, "explicit IBL/scale values are pinned")
    check(json.loads((OUT/"explicit_quality.json").read_text())["viewport"]["antiAliasing"] == "msaa4+taa", "explicit AA is pinned")
    if a.cabin:
        check(CABIN.exists(), "maintainer cabin asset is available")
        run(EXE, CABIN, "cabin")
    if a.wings:
        model = CABIN.parent/"E195-Exterior-Wings-LOD00.gltf"
        run(BASE, model, "wings_baseline")
        run(EXE, model, "wings")
        check((OUT/"wings_baseline_e195.png").read_bytes() == (OUT/"wings_e195.png").read_bytes(), "wings: isolated baseline parity")
    print("All loading checks passed", flush=True)


if __name__ == "__main__":
    main()
