"""Focused default/search-path opt-out checks; never reads or writes user settings."""
import configparser
import json
import os
from pathlib import Path
import time

import harness_process as subprocess
import check_open_without_textures as assets

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/review/default_search_path" / str(time.time_ns())
EXE = ROOT / "build/bin/Release/Glider.exe"


def run(model, directory, label, commands="", extra=()):
    directory.mkdir(parents=True, exist_ok=True)
    script = OUT / f"{label}.txt"
    script.write_text("wait 8\n" + commands + "wait 8\nsnap paths\n" +
        f"shot {(OUT / (label + '.png')).as_posix()}\nwait 2\n", encoding="utf-8")
    with (OUT / f"{label}.log").open("wb") as log:
        result = subprocess.run([str(EXE), str(model), "--quality-preset", "low", "--vsync", "off",
            "--window-size", "1600x900", "--show-search-paths", "--ui-script", str(script),
            "--ui-report", str(OUT / f"{label}.json"), *extra], cwd=ROOT,
            env={**os.environ, "GLIDER_DATA_DIR": str(directory)}, stdout=log, stderr=log, timeout=90)
    ui = json.loads((OUT / f"{label}.json").read_text())
    assert result.returncode == 0 and not ui["failed"], (label, ui)
    snap = ui["snaps"][-1]
    assert snap["glErrors"] == 0
    rows = [p for p in snap["probes"] if p["tag"] == "settings.search.path"]
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(directory / "settings.ini", encoding="utf-8-sig")
    text = (OUT / f"{label}.log").read_text(encoding="utf-8", errors="replace")
    return rows, ini, text


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assets.OUT = OUT
    model, *_ = assets.fixtures()
    settings = OUT / "settings"
    rows, ini, text = run(model, settings, "fresh")
    assert len(rows) == 1 and "[SearchPaths] Resolved 'preset.png'" in text
    assert not ini.has_section("SearchPaths"), "Built-in default should not require serialized settings"
    rows, ini, _ = run(model, settings, "removed", "clickp settings.search.remove 0\nwait 8\n")
    assert not rows and ini.get("SearchPaths", "count") == "0"
    rows, ini, text = run(model, settings, "empty_restart")
    assert not rows and ini.get("SearchPaths", "count") == "0"
    assert "using error image" in text and "[SearchPaths] Resolved 'preset.png'" not in text
    rows, ini, text = run(model, settings, "restored",
        "clickp settings.search_path 0\ntype ../texture*\nwait 4\nclickp settings.search.add 0\nwait 8\n" +
        f"reload {model.as_posix()}\nwait 8\n")
    assert len(rows) == 1 and "[SearchPaths] Resolved 'preset.png'" in text
    assert not ini.has_section("SearchPaths")
    rows, ini, text = run(model, settings, "restored_restart")
    assert len(rows) == 1 and "[SearchPaths] Resolved 'preset.png'" in text

    other = OUT / "unrelated_settings"
    other.mkdir()
    (other / "settings.ini").write_text("; Glider settings file (schema 1)\n[Window]\nwidth=1200\n")
    rows, ini, text = run(model, other, "missing_section")
    assert len(rows) == 1 and "[SearchPaths] Resolved 'preset.png'" in text

    custom = OUT / "custom_settings"
    custom.mkdir()
    (custom / "settings.ini").write_text("; Glider settings file (schema 1)\n[SearchPaths]\npath0=../other\n")
    rows, ini, text = run(model, custom, "custom_list")
    assert len(rows) == 1 and ini.get("SearchPaths", "path0") == "../other"
    assert not ini.has_option("SearchPaths", "path1")
    assert "using error image" in text and "[SearchPaths] Resolved 'preset.png'" not in text
    print("PASS visible wildcard default; removal/count=0; restart opt-out; restore; missing section; custom-list preservation")
    print(OUT)


if __name__ == "__main__":
    main()
