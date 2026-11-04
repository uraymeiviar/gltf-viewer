# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""--test, --capture <png> and --set key=value (docs/CLI_REFERENCE.md).

Checks:
  * --test renders byte-identical to the flags it stands for, and a later flag overrides it;
  * --capture <png> writes the image and <png>.json and exits, byte-identical to the long form;
  * --set for a viewport, an environment and a renderer key renders byte-identical to the dedicated flag;
  * an unknown key, a bad value and a missing "=" exit with code 1 and name the problem.

Run: python scripts/validate_cli_presets.py
"""

import sys
from pathlib import Path

import harness_process as subprocess

from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, same

OUT = REPO / "build" / "cli_presets"
LONG = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]


def run(args, name):
    log = OUT / f"{name}.log"
    with log.open("w", encoding="utf-8") as handle:
        result = subprocess.run([str(EXE), str(PLAIN_FIXTURE)] + args, stdout=handle, stderr=subprocess.STDOUT, timeout=300)
    return result.returncode, log.read_text(encoding="utf-8", errors="replace")


def capture(name, flags):
    png = OUT / f"{name}.png"
    png.unlink(missing_ok=True)
    run(flags + ["--capture-viewport", str(png), "--capture-frame", "24", "--profile-report", str(OUT / f"{name}.json"),
                 "--exit-after-profile"], name)
    return png if png.exists() else None


def main():
    OUT.mkdir(parents=True, exist_ok=True)

    print("[presets]")
    long_form = capture("long", LONG)
    check(long_form is not None, "the long form captures")
    check(same(capture("test", ["--test"]), long_form), "--test = the flags it stands for")
    wide = capture("wide", ["--test", "--window-size", "800x600"])
    long_wide = capture("long_wide", LONG + ["--window-size", "800x600"])
    check(same(wide, long_wide) and not same(wide, long_form), "a flag after --test overrides it")
    png = OUT / "capture.png"
    png.unlink(missing_ok=True)
    code, _ = run(["--capture", str(png)], "capture")
    check(code == 0 and same(png, long_form), "--capture writes the same image and exits with code 0")
    check(Path(str(png) + ".json").exists(), "--capture writes the report next to the image")

    print("[--set]")
    pairs = [("viewport", ["--hide-grid"], ["--set", "grid=off"]),
             ("environment", ["--exposure", "1.5", "--auto-exposure", "off"],
              ["--set", "exposure=1.5", "--set", "auto_exposure=off"]),
             ("renderer", ["--bloom", "off"], ["--set", "bloom=off"]),
             ("colour", ["--wireframe-overlay", "--wireframe-color", "1,0,0"],
              ["--wireframe-overlay", "--set", "wireframe_color=1,0,0,1"])]
    base = capture("set_none", LONG)
    for name, dedicated, generic in pairs:
        a = capture(f"flag_{name}", LONG + dedicated)
        b = capture(f"set_{name}", LONG + generic)
        check(a is not None and same(a, b), f"{name}: --set = {' '.join(dedicated)}")
    check(not same(capture("flag_exposure", LONG + ["--exposure", "1.5", "--auto-exposure", "off"]), base),
          "--exposure changes the picture (the comparison is not trivial)")

    print("[errors]")
    for name, flags, text in (("unknown", ["--set", "no_such_key=1"], "unknown setting no_such_key"),
                              ("bad_value", ["--set", "render_mode=no_such_mode"], "Invalid --set"),
                              ("no_equals", ["--set", "grid"], "expects key=value")):
        code, log = run(LONG + flags, f"error_{name}")
        check(code == 1 and text in log, f"{name}: exit code 1 and \"{text}\" (got {code})")
    finish()


if __name__ == "__main__":
    sys.exit(main())
