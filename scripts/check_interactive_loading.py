"""Measure PMDG with a private copy of saved settings and same-process File/Open queue.

No regression suites, cache purges or executable copies. The original settings.ini is
read only; each run gets a fresh private snapshot. Captures preserve saved window size.
"""
import hashlib
import json
import os
import re
from pathlib import Path
import time

import psutil
import harness_process as subprocess
from measure_loading import GpuSampler, ROOT
from e195_assets import COCKPIT

EXE = ROOT / "build/bin/Release/Glider.exe"
SETTINGS = EXE.parent / "settings.ini"
PMDG = Path(r"F:\msfs-addons\pmdg-aircraft-77w\SimObjects\Airplanes\PMDG 777-300ER\model.300ER\PMDG773ER_VC.gltf")
OUT = ROOT / "build/review/loading/interactive"


def run(label, initial, reopen):
    snapshot = SETTINGS.read_bytes()
    profiles = re.search(r"(?ms)^\[QualityProfiles\]\s*\n(.*?)(?=^\[|\Z)", snapshot.decode("utf-8-sig"))
    selected = re.search(r"(?m)^selection\s*=\s*([0-4])\s*$", profiles.group(1)) if profiles else None
    preset = ("auto", "low", "med", "high", "ultra")[int(selected.group(1)) if selected else 0]
    private = OUT / label / "settings"
    private.mkdir(parents=True, exist_ok=True)
    (private / "settings.ini").write_bytes(snapshot)
    args = [str(EXE), *( [str(initial)] if initial else [] ),
        "--quality-preset", preset,  # Explicitly keep saved profiles active in capture/UI tooling.
        "--profile-report", str(OUT / f"{label}.json"),
        "--capture-viewport", str(OUT / f"{label}.png"), "--capture-frame", "80" if reopen else "12"]
    if reopen:
        script = OUT / f"{label}.txt"
        script.write_text("wait 8\nclick 18 12\nwait 4\nclickp file.open 0\nwait 110\n", encoding="utf-8")
        args += ["--ui-script", str(script), "--ui-report", str(OUT / f"{label}.ui.json")]
    else:
        args += ["--exit-after-profile"]
    start = time.perf_counter()
    gpu = GpuSampler()
    peak, gpu_peak = 0, 0
    with (OUT / f"{label}.log").open("wb") as log:
        child = subprocess.Popen(args, cwd=ROOT,
            env={**os.environ, "GLIDER_DATA_DIR": str(private), "GLIDER_TEST_OPEN_FILE": str(PMDG)}, stdout=log, stderr=log)
        while child.poll() is None:
            try:
                memory = psutil.Process(child.pid).memory_info()
                peak = max(peak, getattr(memory, "peak_wset", memory.rss))
            except psutil.Error:
                pass
            gpu_peak = max(gpu_peak, gpu.sample(child.pid) or 0)
            if time.perf_counter() - start > 240:
                child.kill()
                raise TimeoutError(label)
            time.sleep(.025)
    gpu.close()
    external = dict(command=args, exit=child.returncode, wallSeconds=time.perf_counter()-start,
        peakWorkingSet=peak, peakDedicatedGpuBytes=gpu_peak,
        settingsSha256=hashlib.sha256(snapshot).hexdigest())
    (OUT / f"{label}.external.json").write_text(json.dumps(external, indent=2))
    assert child.returncode == 0, (label, external)
    text = (OUT / f"{label}.log").read_text(encoding="utf-8", errors="replace")
    assert "GL automation errors: 0" in text and "using error image" not in text
    if reopen:
        assert not json.loads((OUT / f"{label}.ui.json").read_text())["failed"]
    data = json.loads((OUT / f"{label}.json").read_text())
    assert Path(data["model"]) == PMDG
    print(label, json.dumps({key: data["startup"].get(key) for key in
        ("lastLoadWallMs", "lastLoadToFirstModelFrameMs", "loadDecodeMs", "rendererUploadMs", "validationMs", "totalStartupMs", "loadingProgressUiMs", "loadStagesMs", "residentModelTextureBytes", "decodedImageBytes", "texturesIntentionallyOmitted")}), flush=True)
    assert SETTINGS.read_bytes() == snapshot, "Original settings changed during the check"
    assert data["startup"]["texturesIntentionallyOmitted"] is False
    assert data["startup"]["residentModelTextureBytes"] > 0
    return data["startup"]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    fresh = run("pmdg_saved_startup", PMDG, False)
    opened = run("pmdg_open_from_cube", None, True)
    replaced = run("pmdg_replace_e195", COCKPIT, True)
    assert opened["residentModelTextureBytes"] == replaced["residentModelTextureBytes"] == fresh["residentModelTextureBytes"], "Reopening must upload the same PMDG texture payload as a fresh process"


if __name__ == "__main__":
    main()
