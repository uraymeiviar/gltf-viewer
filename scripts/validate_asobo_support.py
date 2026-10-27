# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A0.2 support matrix coverage and declared-extension validation messages."""

import json
import re
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "asobo_support"
SPECS = REPO / "OpenGL-Renderer/Tools/tools/gltf/source/extensions/AsoboExtensionSpecs.cpp"
COCKPIT = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf")
CODE = "ASOBO_EXTENSIONS_NOT_RENDERED"
METADATA = {"ASOBO_unique_id", "ASOBO_tags", "ASOBO_animation_retargeting", "ASOBO_softbody_mesh"}
EXCLUDED = METADATA | {"ASOBO_asset_optimized", "ASOBO_material_invisible", "ASOBO_material_environment_occluder",
                       "ASOBO_material_disable_motion_blur", "ASOBO_material_fake_terrain",
                       "ASOBO_normal_map_convention", "ASOBO_material_draw_order", "ASOBO_material_emissive",
                       "ASOBO_material_detail_map", "ASOBO_occlusion_strength", "MSFT_texture_dds"}
failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def report(model, validate=False):
    path = OUT / (model.stem + (".validation.json" if validate else ".asobo.json"))
    path.unlink(missing_ok=True)
    args = (["--validate-file", str(model), "--validation-report", str(path)] if validate
            else [str(model), "--asobo-report", str(path)])
    try:
        result = subprocess.run([str(EXE), "--isolated", *args], cwd=REPO,
                                capture_output=True, text=True, timeout=600)
        # Validation may report asset errors with a nonzero exit code.
        if path.exists() and (validate or result.returncode == 0):
            return json.loads(path.read_text(encoding="utf-8"))
        print(f"  Process exit: {result.returncode}")
    except (OSError, subprocess.TimeoutExpired, ValueError) as error:
        print(f"  Report error: {error}")
    return None


def fixture(name, declared):
    path = OUT / (name + ".gltf")
    path.write_text(json.dumps({"asset": {"version": "2.0"}, "scene": 0,
                                "scenes": [{"nodes": [0]}], "nodes": [{"name": "fixture"}],
                                "extensionsUsed": declared}), encoding="utf-8")
    return path


def check_validation(path, expected):
    rep = report(path, validate=True)
    check(rep is not None, f"{path.stem}: validation report written")
    if rep is None:
        return
    issues = [issue for issue in rep.get("issues", []) if issue.get("code") == CODE]
    check(len(issues) == (1 if expected else 0), f"{path.stem}: one issue only when unsupported extensions exist")
    if expected and len(issues) == 1:
        issue = issues[0]
        check(issue.get("severity") == "Info" and issue.get("pointer") == "/extensionsUsed",
              f"{path.stem}: info severity and extensionsUsed pointer")
        names = re.findall(r"(?:ASOBO_|MSFT_)[A-Za-z0-9_]+", issue.get("message", ""))
        check(names == sorted(expected), f"{path.stem}: exact sorted, comma-separated extension list")
        check(", ".join(sorted(expected)) in issue.get("message", ""), f"{path.stem}: comma separators")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # specCount is a count, not a name table; read the generated source independently.
    names = set(re.findall(r'^\s*\{"(ASOBO_[^"]+)", Scope::', SPECS.read_text(encoding="utf-8"), re.M))
    expected = names | {"MSFT_texture_dds"}
    print("[support table]")
    model = fixture("support", sorted(expected, reverse=True))
    rep = report(model)
    check(rep is not None, "support report written")
    if rep is not None:
        check(rep.get("specCount") == len(names) == 42, "specCount matches all 42 generated extension specs (including two MSFS 2020 glass schemas)")
        support = rep.get("support", {})
        check(set(support) == expected | {"ASOBO_material_code"},
              "every spec plus MSFT_texture_dds and the extras material-code switch has a status, with no extra entries")
        check(support.get("ASOBO_material_code") == "Partial", "extras material-code mapping: Partial")
        check({"ASOBO_material_glass", "ASOBO_material_kitty_glass"} <= names,
              "both legacy glass schemas have typed specs")
        for name in sorted(expected):
            status = "Metadata" if name in METADATA else "Not yet supported"
            if name in ("ASOBO_asset_optimized", "ASOBO_material_invisible", "ASOBO_material_environment_occluder",
                        "ASOBO_normal_map_convention", "ASOBO_material_draw_order", "ASOBO_material_emissive",
                        "ASOBO_material_detail_map", "ASOBO_occlusion_strength", "MSFT_texture_dds"):
                status = "Rendered"
            elif name in ("ASOBO_material_geometry_decal", "ASOBO_material_blend_gbuffer",
                          "ASOBO_material_windshield_v3", "ASOBO_material_glass_v2",
                          "ASOBO_material_windshield", "ASOBO_material_glass", "ASOBO_material_kitty_glass"):
                status = "Partial"
            elif name in ("ASOBO_material_disable_motion_blur", "ASOBO_material_fake_terrain"):
                status = "Not applicable"
            check(support.get(name) == status, f"{name}: {status}")

    print("[validation fixtures]")
    check_validation(model, expected - EXCLUDED)
    check_validation(fixture("supported", sorted(EXCLUDED)), set())
    check_validation(fixture("unknown", ["KHR_materials_unlit", "ASOBO_future_test", "MSFT_texture_dds"]),
                     {"ASOBO_future_test"})

    print("[real cockpit]")
    if COCKPIT.exists():
        doc = json.loads(COCKPIT.read_text(encoding="utf-8"))
        unsupported = {name for name in doc.get("extensionsUsed", [])
                       if (name.startswith("ASOBO_") or name == "MSFT_texture_dds") and name not in EXCLUDED}
        check("ASOBO_material_glass_v2" in unsupported and "ASOBO_material_draw_order" not in unsupported,
              "cockpit declares glass v2 (unsupported) and draw order (supported)")
        check_validation(COCKPIT, unsupported)
    else:
        print("  SKIP E195-Cockpit-LOD00.gltf (not found)")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        check(False, str(error))
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
