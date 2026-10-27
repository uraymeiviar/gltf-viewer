"""Record loading stages, captures, logs and external process peak memory. Sequential runs only.

Example: python scripts/measure_loading.py --label before --exe build/bin/Release/Glider.exe
No executable is copied or renamed. --auto on tests the adaptive path; default is deterministic.
"""
import argparse
import json
import os
from pathlib import Path
import time
import ctypes
from ctypes import wintypes

import psutil
import harness_process as subprocess
from e195_assets import COCKPIT, TEXTURE_FOLDERS

ROOT = Path(__file__).resolve().parents[1]


class GpuSampler:
    """Windows PDH per-process dedicated usage, usable with unmodified baselines."""
    def __init__(self):
        self.query, self.counter = ctypes.c_void_p(), ctypes.c_void_p()
        self.pdh = ctypes.WinDLL("pdh") if os.name == "nt" else None
        if self.pdh:
            self.pdh.PdhOpenQueryW(None, 0, ctypes.byref(self.query))
            status = self.pdh.PdhAddEnglishCounterW(self.query, r"\GPU Process Memory(*)\Dedicated Usage", 0, ctypes.byref(self.counter))
            if status:
                self.close()

    def sample(self, pid):
        if not self.query.value:
            return None
        class Value(ctypes.Union):
            _fields_ = [("largeValue", ctypes.c_longlong), ("doubleValue", ctypes.c_double)]
        class CounterValue(ctypes.Structure):
            _fields_ = [("status", wintypes.DWORD), ("value", Value)]
        class Item(ctypes.Structure):
            _fields_ = [("name", wintypes.LPWSTR), ("value", CounterValue)]
        self.pdh.PdhCollectQueryData(self.query)
        size, count = wintypes.DWORD(), wintypes.DWORD()
        self.pdh.PdhGetFormattedCounterArrayW(self.counter, 0x400, ctypes.byref(size), ctypes.byref(count), None)
        if not size.value:
            return None
        buf = ctypes.create_string_buffer(size.value)
        status = self.pdh.PdhGetFormattedCounterArrayW(self.counter, 0x400, ctypes.byref(size), ctypes.byref(count), buf)
        if status:
            return None
        items = ctypes.cast(buf, ctypes.POINTER(Item))
        values = [items[i].value.value.largeValue for i in range(count.value)
                  if items[i].name.startswith(f"pid_{pid}_") and items[i].value.status in (0, 1)]
        return sum(values) if values else None

    def close(self):
        if self.query.value:
            self.pdh.PdhCloseQuery(self.query)
            self.query = ctypes.c_void_p()


def run(exe, model, label, auto="off", vram=None, without_textures=False, search_paths=None, quality_preset=None):
    out = ROOT / "build/review/loading"
    out.mkdir(parents=True, exist_ok=True)
    model_name = "heavy" if model.stem == "heavy" else "e195" if model == COCKPIT else model.stem
    name = f"{label}_{model_name}"
    report, png, log = (out / f"{name}.{ext}" for ext in ("json", "png", "log"))
    args = [str(exe), str(model), "--isolated", "--vsync", "off", "--window-size", "1600x900",
            "--capture-viewport", str(png), *( ["--capture-time", "6"] if auto == "on" else ["--capture-frame", "12"] ),
            "--profile-report", str(report), "--exit-after-profile"]
    # The baseline predates --auto-quality; do not pass new switches to it.
    if "baselines" not in str(exe):
        args += ["--auto-quality", auto] if auto != "legacy" else []
        args += ["--capture-loading", str(out/f"{name}_progress.png")]
    if without_textures:
        if "baselines" in str(exe):
            raise ValueError("The old baseline does not support --without-textures")
        args += ["--without-textures"]
    if quality_preset is not None:
        args += ["--quality-preset", quality_preset]
    if search_paths is not None:
        for folder in search_paths:
            args += ["--search-path", folder]
    elif model != ROOT/"build/review/heavy/heavy.gltf":
        for folder in TEXTURE_FOLDERS:
            args += ["--search-path", "../"+folder]
    env = os.environ.copy()
    if vram is not None:
        env["GLIDER_TEST_VRAM_MB"] = str(vram)
    start = time.perf_counter()
    gpu = GpuSampler()
    with log.open("wb") as f:
        p = subprocess.Popen(args, cwd=ROOT, env=env, stdout=f, stderr=f)
        peak, peak_gpu = 0, None
        while p.poll() is None:
            try:
                mem = psutil.Process(p.pid).memory_info()
                peak = max(peak, getattr(mem, "peak_wset", mem.rss))
            except psutil.Error:
                pass
            used = gpu.sample(p.pid)
            if used is not None:
                peak_gpu = max(used, peak_gpu or 0)
            if time.perf_counter()-start > 600:
                p.kill()
                raise TimeoutError(name)
            time.sleep(.025)
    gpu.close()
    external = dict(command=args, exit=p.returncode, wallSeconds=time.perf_counter()-start, peakWorkingSet=peak,
                    peakDedicatedGpuBytes=peak_gpu, gpuMethod="Windows PDH GPU Process Memory/Dedicated Usage (sampled)")
    (out/f"{name}.external.json").write_text(json.dumps(external, indent=2), encoding="utf-8")
    print(name, json.dumps(external), flush=True)
    if p.returncode or not report.exists() or not png.exists():
        raise RuntimeError(f"{name}: failed; see {log}")
    data = json.loads(report.read_text(encoding="utf-8"))
    print(json.dumps(data.get("startup", {}), indent=2), flush=True)
    return data


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, default=ROOT/"build/bin/Release/Glider.exe")
    p.add_argument("--label", required=True)
    p.add_argument("--auto", choices=("on", "off", "legacy"), default="off")
    p.add_argument("--vram", type=int)
    p.add_argument("--without-textures", action="store_true")
    p.add_argument("--model-path", type=Path, help="Explicit model instead of --model choices")
    p.add_argument("--search-path", action="append", help="Texture search folders/patterns relative to the model")
    p.add_argument("--quality-preset", choices=("auto", "low", "med", "mid", "high", "ultra"))
    p.add_argument("--model", choices=("both", "heavy", "e195"), default="both")
    a = p.parse_args()
    for model in ((a.model_path,) if a.model_path else (COCKPIT, ROOT/"build/review/heavy/heavy.gltf") if a.model == "both" else
                  (COCKPIT,) if a.model == "e195" else (ROOT/"build/review/heavy/heavy.gltf",)):
        if not model.exists():
            raise FileNotFoundError(model)
        run(a.exe.resolve(), model, a.label, a.auto, a.vram, a.without_textures, a.search_path, a.quality_preset)
