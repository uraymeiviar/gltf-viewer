# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""ASOBO harness template (A0.5) and the A0.4 extension-switch checks.

Later phases import it (`from validate_asobo_common import ...`) for:
  * views():          the A0.3 reference views (docs/ASOBO_REFERENCE_VIEWS.md) with their camera flags;
  * render():         an isolated, textured viewport capture of a model with a view's flags;
  * fixture():        a small model with ASOBO extensions injected into its materials;
  * same() / check(): byte comparison and PASS/FAIL reporting in the regression format.

Run on its own, it checks A0.4 (docs/ASOBO_EXTENSIONS_PHASE_0.md):
  * everything off: with --asobo off, the E195 views are byte-identical to the build before ASOBO support
    (167a5e0) run with --show-invisible-materials, which is that build with every ASOBO effect off;
  * one extension off: --asobo-disable ASOBO_material_invisible gives the same result;
  * defaults: an extension written as {} renders like the same extension with its defaults written out;
  * files without ASOBO extensions render identically with ASOBO on and off.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "asobo_common"
# The last build before ASOBO support; its default rendering hides ASOBO_material_invisible only.
PRE_ASOBO_EXE = REPO / "build" / "baselines" / "167a5e0.exe"
E195 = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model")
REAL_ASSETS = {"Cockpit": E195 / "E195-Cockpit-LOD00.gltf", "Exterior": E195 / "E195-Exterior-LOD00.gltf"}
# Texture folders of the E195 project (relative to the model), as in the user's settings.ini.
E195_SEARCH_PATHS = ["../texture", "../texture.E195.cockpit", "../texture.FSS"]
PLAIN_FIXTURE = REPO / "resources" / "test_models" / "hierarchy_animation_fixture.gltf"
COMMON = ["--isolated", "--window-size", "1600x900", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def finish():
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)


def views():
    """The A0.3 reference views: [{"name", "model" ("Cockpit"/"Exterior"), "flags": [...]}]."""
    text = (REPO / "docs" / "ASOBO_REFERENCE_VIEWS.md").read_text(encoding="utf-8")
    result = []
    for row in re.finditer(r"^\| \*\*\d+\*\* \| `([^`]+)`[^|]*\| (\w+) \| `([^`]+)` \|", text, re.M):
        result.append({"name": row.group(1), "model": row.group(2), "flags": row.group(3).split()})
    return result


def view(name):
    return next(v for v in views() if v["name"] == name)


def render(exe, model, name, flags=(), folder=OUT, search_paths=()):
    """Captures the viewport (no UI) at frame 24; returns the PNG path or None."""
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    png = folder / f"{name}.png"
    png.unlink(missing_ok=True)
    paths = [arg for path in search_paths for arg in ("--search-path", path)]
    subprocess.run([str(exe), str(model)] + COMMON + paths + list(flags) +
                   ["--capture-viewport", str(png), "--capture-frame", "24", "--profile-report", str(folder / f"{name}.json"),
                    "--exit-after-profile"], cwd=REPO, capture_output=True, timeout=900)
    return png if png.exists() else None


def render_view(exe, view_entry, name, flags=(), folder=OUT, textured=True):
    """textured=False leaves out --search-path, for binaries older than the flag (they read it as a model path)."""
    return render(exe, REAL_ASSETS[view_entry["model"]], name, list(view_entry["flags"]) + list(flags), folder,
                  E195_SEARCH_PATHS if textured else ())


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() if path else None


def same(a, b):
    return a is not None and b is not None and digest(a) == digest(b)


def fixture(name, material_extensions, base=PLAIN_FIXTURE, folder=OUT):
    """Copies base and adds {material index: {extension: value}}, declaring each extension; returns the path."""
    doc = json.loads(Path(base).read_text(encoding="utf-8"))
    declared = set(doc.get("extensionsUsed", []))
    for index, extensions in material_extensions.items():
        doc["materials"][index].setdefault("extensions", {}).update(extensions)
        declared |= set(extensions)
    doc["extensionsUsed"] = sorted(declared)
    path = Path(folder) / f"{name}.gltf"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=1), encoding="utf-8")
    return path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pre-asobo-exe", default=str(PRE_ASOBO_EXE), help="build before ASOBO support")
    args = parser.parse_args()
    pre = Path(args.pre_asobo_exe)

    print("[reference views]")
    check(len(views()) >= 11, f"{len(views())} reference views parsed from docs/ASOBO_REFERENCE_VIEWS.md")

    print("[files without ASOBO extensions]")
    on, off = render(EXE, PLAIN_FIXTURE, "plain_on"), render(EXE, PLAIN_FIXTURE, "plain_off", ["--asobo", "off"])
    check(on is not None and same(on, off), "plain fixture: ASOBO on and off are byte-identical")

    print("[defaults]")
    implicit = fixture("invisible_implicit", {0: {"ASOBO_material_invisible": {}}})
    explicit = fixture("invisible_explicit", {0: {"ASOBO_material_invisible": {"enabled": True}}})
    a, b = render(EXE, implicit, "invisible_implicit"), render(EXE, explicit, "invisible_explicit")
    check(a is not None and same(a, b), "ASOBO_material_invisible {} renders like enabled: true")
    check(not same(a, on), "the invisible material is hidden (the image differs from the plain fixture)")
    shown = render(EXE, implicit, "invisible_disabled", ["--asobo-disable", "ASOBO_material_invisible"])
    check(same(shown, on), "--asobo-disable ASOBO_material_invisible draws it again")

    print("[--search-path]")
    if all(p.exists() for p in REAL_ASSETS.values()):
        from PIL import Image
        textured = render_view(EXE, view("cockpit_main_displays"), "cockpit_main_displays_textured")
        pixels = list(Image.open(textured).convert("RGB").getdata()) if textured else []
        # Glider draws a missing texture as magenta.
        magenta = sum(1 for r, g, b in pixels if r > 180 and b > 180 and g < 90) / max(1, len(pixels))
        check(textured is not None and magenta < 0.01, f"isolated render with --search-path is textured ({magenta:.1%} magenta)")

    print("[everything off versus the build before ASOBO support]")
    if not pre.exists():
        print(f"  SKIP {pre.name} not found")
    elif not all(p.exists() for p in REAL_ASSETS.values()):
        print("  SKIP E195 assets not found")
    else:
        for name in ("cockpit_main_displays", "exterior_fuselage_nose"):
            entry = view(name)
            # The old build has no --search-path, so both sides render with the default texture folder only.
            reference = render_view(pre, entry, f"{name}_pre", ["--show-invisible-materials"], textured=False)
            all_off = render_view(EXE, entry, f"{name}_off", ["--asobo", "off"], textured=False)
            one_off = render_view(EXE, entry, f"{name}_invisible_off", ["--asobo-disable", "ASOBO_material_invisible"],
                                  textured=False)
            check(reference is not None and same(all_off, reference), f"{name}: --asobo off is byte-identical to {pre.stem}")
            check(same(one_off, reference), f"{name}: --asobo-disable ASOBO_material_invisible is byte-identical to {pre.stem}")

    finish()
