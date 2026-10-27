# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Validation script for the single portable settings.ini and I/O robustness.

Checks:
  1. pristine: A session with an empty private settings directory writes no file at
     all, because nothing differs from the built-in defaults.
  2. changed_value: Changing one preference writes settings.ini with the schema marker,
     the changed value, and no default-valued sections.
  3. readonly_fallback: When <exeDir> is not writable (denied write via ACL), Glider
     writes settings.ini to %APPDATA%/Glider (or %TEMP%/Glider), logs the fallback, and
     exits 0 without creating a file in the exe directory.
  4. corrupt_unreadable: A corrupt file logs one warning and restarts from defaults; an
     unreadable file is never overwritten and changes stay in memory for the session.
  5. isolated_mode: --isolated neither reads nor writes the private file nor the
     portable settings.ini next to the executable.
  6. baseline_compare: With --baseline <exe>, renders --isolated and compares the
     capture byte-for-byte with that executable.

Private settings directories are selected with GLIDER_DATA_DIR (the test hook) and are
never the user's state. Only the fallback check touches the real exe directory, and it
moves the existing settings.ini aside and restores it afterwards.
"""

import argparse
import atexit
import hashlib
import os
import pathlib
import shutil
import harness_process as subprocess
import sys
import time

REPO = pathlib.Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
REVIEW_DIR = REPO / "build" / "review"
PORTABLE = EXE.parent / "settings.ini"
SCHEMA_MARKER = "; Glider settings file (schema 1)"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--only", choices=["pristine", "changed_value", "readonly_fallback",
                                      "corrupt_unreadable", "isolated_mode", "baseline_compare"])
parser.add_argument("--baseline", default=None,
                    help="Optional baseline Glider.exe for the isolated capture comparison.")
args = parser.parse_args()

REVIEW_DIR.mkdir(parents=True, exist_ok=True)
failures = []


def section(name):
    return args.only in (None, name)


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def clean_dir(path):
    path = pathlib.Path(path)
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)
    return path


def private_env(directory):
    env = dict(os.environ)
    env.pop("GLIDER_DATA_DIR", None)
    env["GLIDER_DATA_DIR"] = str(directory)
    return env


def idle_script():
    script = REVIEW_DIR / "val_settings_idle.txt"
    script.write_text("wait 8\n", encoding="utf-8")
    return script


def run_glider(extra_args, env=None, timeout=120):
    return subprocess.run([str(EXE)] + extra_args, cwd=REPO, env=env,
                          capture_output=True, text=True, timeout=timeout)


def file_hash(path):
    path = pathlib.Path(path)
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def read_ini(path):
    sections = {}
    current = None
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = sections.setdefault(line[1:-1], {})
        elif current is not None and "=" in line:
            key, value = line.split("=", 1)
            current[key.strip()] = value.strip()
    return sections


def run_session(directory, label, extra_args=()):
    """Run one interactive session with a private settings directory."""
    script = idle_script()
    report = REVIEW_DIR / f"val_settings_{label}.json"
    report.unlink(missing_ok=True)
    result = run_glider(["--ui-script", str(script), "--ui-report", str(report)] + list(extra_args),
                        env=private_env(directory))
    return result, pathlib.Path(directory) / "settings.ini"


if section("pristine"):
    print("[pristine]")
    directory = clean_dir(REVIEW_DIR / "val_settings_pristine")
    result, ini_path = run_session(directory, "pristine")
    check(result.returncode == 0, f"Glider exits 0 with an empty settings directory ({result.returncode})")
    check(not ini_path.exists(), "No settings.ini is written when every value equals its default")
    check(not (directory / "settings.ini.tmp").exists(), "No temporary settings file is left behind")
    check("Corrupt or invalid" not in result.stderr and "Failed to read" not in result.stderr,
          "A missing settings file is valid and logs no warning")


if section("changed_value"):
    print("[changed_value]")
    directory = clean_dir(REVIEW_DIR / "val_settings_changed")
    result, ini_path = run_session(directory, "changed", ("--quality-preset", "low"))
    check(result.returncode == 0, f"Glider exits 0 after changing a preference ({result.returncode})")
    check(ini_path.is_file(), f"settings.ini written at {ini_path}")
    if ini_path.is_file():
        text = ini_path.read_text(encoding="utf-8")
        check(text.startswith(SCHEMA_MARKER), "settings.ini starts with the schema marker")
        ini = read_ini(ini_path)
        check(ini.get("QualityProfiles", {}).get("selection") == "1",
              "Changed preference is stored ([QualityProfiles] selection=1)")
        check(ini.get("RenderQuality", {}).get("iblCubeSize") == "256",
              "Preset-specific render value is stored (iblCubeSize=256)")
        check(all(name not in ini for name in ("SearchPaths", "SceneHierarchy", "Resources")),
              "Default-valued sections are not written")
        check(all(name not in ini for name in ("Window", "Layout")),
              "Default window size and layout are not written")


if section("readonly_fallback"):
    print("[readonly_fallback]")
    preserved = REVIEW_DIR / f"val_settings_portable_{time.time_ns()}.ini"
    had_portable = PORTABLE.is_file()
    if had_portable:
        PORTABLE.replace(preserved)
    fake_appdata = clean_dir(REVIEW_DIR / "val_settings_fallback_appdata")
    user = os.environ.get("USERNAME", "Everyone")
    denied = False

    def restore_portable_file():
        if denied:
            try:
                subprocess.run(["icacls", str(EXE.parent), "/remove:d", user], check=True, capture_output=True)
            except Exception:  # noqa: BLE001 - restoration must never mask the result
                pass
        if had_portable and preserved.is_file():
            preserved.replace(PORTABLE)

    atexit.register(restore_portable_file)
    try:
        subprocess.run(["icacls", str(EXE.parent), "/deny", f"{user}:(W)"], check=True, capture_output=True)
        denied = True
        env = dict(os.environ)
        env.pop("GLIDER_DATA_DIR", None)
        env["APPDATA"] = str(fake_appdata)
        report = REVIEW_DIR / "val_settings_fallback.json"
        report.unlink(missing_ok=True)
        result = run_glider(["--quality-preset", "low", "--ui-script", str(idle_script()),
                             "--ui-report", str(report)], env=env)
        check(result.returncode == 0, f"Glider exits 0 with an unwritable exe directory ({result.returncode})")
        check("is not writable; using" in result.stderr, "Fallback settings location is logged")
        fallback = fake_appdata / "Glider" / "settings.ini"
        check(fallback.is_file(), f"settings.ini written to the fallback location ({fallback})")
        if fallback.is_file():
            check(fallback.read_text(encoding="utf-8").startswith(SCHEMA_MARKER),
                  "Fallback settings.ini is written in the new format")
        check(not PORTABLE.exists(), "No settings.ini is created in the unwritable exe directory")
    finally:
        restore_portable_file()
        atexit.unregister(restore_portable_file)


if section("corrupt_unreadable"):
    print("[corrupt_unreadable]")
    directory = clean_dir(REVIEW_DIR / "val_settings_corrupt")
    ini_path = directory / "settings.ini"
    ini_path.write_bytes(b"\xde\xad\xbe\xef\x00\x01\x02\x03corrupt binary data")
    result, _ = run_session(directory, "corrupt")
    check(result.returncode == 0, f"Glider exits 0 with a corrupt settings.ini ({result.returncode})")
    check("Corrupt or invalid settings.ini" in result.stderr, "Corrupt settings.ini warning logged")
    check(not ini_path.exists() or ini_path.read_text(encoding="utf-8", errors="replace").startswith(SCHEMA_MARKER),
          "Corrupt file is replaced by a valid file on the next save (or removed when empty)")

    # Unreadable: the file must be left untouched and changes stay in memory.
    ini_path.write_text(SCHEMA_MARKER + "\n\n[QualityProfiles]\nselection=3\n", encoding="utf-8")
    original = ini_path.read_bytes()
    user = os.environ.get("USERNAME", "Everyone")
    denied = False

    def restore_read_access():
        if denied:
            try:
                subprocess.run(["icacls", str(ini_path), "/remove:d", user], check=True, capture_output=True)
            except Exception:  # noqa: BLE001 - restoration must never mask the result
                pass

    atexit.register(restore_read_access)
    try:
        subprocess.run(["icacls", str(ini_path), "/deny", f"{user}:(R)"], check=True, capture_output=True)
        denied = True
        result, _ = run_session(directory, "unreadable", ("--quality-preset", "low"))
        check(result.returncode == 0, f"Glider exits 0 with an unreadable settings.ini ({result.returncode})")
        check("Failed to read settings.ini" in result.stderr, "Unreadable settings.ini warning logged")
    finally:
        restore_read_access()
        atexit.unregister(restore_read_access)
    check(file_hash(ini_path) == hashlib.sha256(original).hexdigest(),
          "Unreadable settings.ini is left byte-identical")


if section("isolated_mode"):
    print("[isolated_mode]")
    directory = clean_dir(REVIEW_DIR / "val_settings_isolated")
    ini_path = directory / "settings.ini"
    ini_path.write_text(SCHEMA_MARKER + "\n\n[QualityProfiles]\nselection=2\n", encoding="utf-8")
    before = file_hash(ini_path)
    portable_before = file_hash(PORTABLE)
    result, _ = run_session(directory, "isolated", ("--isolated", "--quality-preset", "low"))
    check(result.returncode == 0, f"Glider exits 0 under --isolated ({result.returncode})")
    check(file_hash(ini_path) == before, "Private settings.ini is unchanged under --isolated")
    check(file_hash(PORTABLE) == portable_before, "Portable settings.ini is unchanged under --isolated")

    empty = clean_dir(REVIEW_DIR / "val_settings_isolated_empty")
    run_session(empty, "isolated_empty", ("--isolated", "--quality-preset", "low"))
    check(not (empty / "settings.ini").exists(), "--isolated does not create a settings file")


if section("baseline_compare"):
    print("[baseline_compare]")
    if args.baseline is None:
        print("  SKIP pass --baseline <Glider.exe> to compare an isolated capture")
    else:
        baseline = pathlib.Path(args.baseline)
        check(baseline.is_file(), f"Baseline exists ({baseline})")
        base_dump = REVIEW_DIR / "val_baseline_dump.png"
        cand_dump = REVIEW_DIR / "val_candidate_dump.png"
        base_dump.unlink(missing_ok=True)
        cand_dump.unlink(missing_ok=True)
        env = dict(os.environ)
        env.pop("GLIDER_DATA_DIR", None)

        def capture(exe, target):
            return subprocess.run([str(exe), "resources/sampleGltf/HelloCube.glb", "--isolated",
                                   "--dump-frame", str(target), "--exit-after-dump"],
                                  cwd=REPO, env=env, capture_output=True, text=True)

        res_base = capture(baseline, base_dump)
        res_cand = capture(EXE, cand_dump)
        check(res_base.returncode == 0, f"Baseline exits 0 ({res_base.returncode})")
        check(res_cand.returncode == 0, f"Candidate exits 0 ({res_cand.returncode})")
        if base_dump.is_file() and cand_dump.is_file():
            base_bytes = base_dump.read_bytes()
            cand_bytes = cand_dump.read_bytes()
            check(base_bytes == cand_bytes,
                  f"Candidate capture byte-identical to baseline ({len(cand_bytes)} bytes, "
                  f"sha256={hashlib.sha256(cand_bytes).hexdigest()[:16]})")
        else:
            check(False, "Both captures exist")


print(f"\nSummary: {len(failures)} failures")
if failures:
    sys.exit(1)
sys.exit(0)
