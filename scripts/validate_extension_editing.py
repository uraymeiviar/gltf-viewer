# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Material extension editing and viewport-only display policy.

Drives actual Inspector widgets by UiProbe, checks revision producers and the
absence of controls for absent extensions, and compares each disabled KHR feature
with a file lacking only that feature, using the same binary and exact PNG bytes.
All-on fixtures, HelloCube, E195 cockpit and the DA62 forward view are compared
with --baseline-exe (default f9cf00c). No legacy pre-HDR binaries are used.
Generated documents/captures stay under build/extension_editing; input files must
remain byte-identical. This is an explicit, standalone suite, not a benchmark.
"""

import argparse
import hashlib
import json
from pathlib import Path

import harness_process as subprocess
from validate_asobo_common import EXE, REPO, check, finish, render, same, view, render_view

OUT = REPO / "build/extension_editing"
CC = "KHR_materials_clearcoat"
TR = "KHR_materials_transmission"
VENDOR = "EXT_glider_test"
ABSENT = ("KHR_materials_unlit", "KHR_materials_volume", "KHR_materials_ior",
          "KHR_materials_specular", "KHR_materials_sheen", "KHR_materials_iridescence")
COMMON = ["--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05",
          "--vsync", "off", "--aa", "none", "--auto-exposure", "off", "--hide-grid",
          "--select-material", "0"]


def write_fixture():
    source = REPO / "resources/test_models/hierarchy_animation_fixture.gltf"
    data = json.loads(source.read_text(encoding="utf-8"))
    data["nodes"] = [{"name": "ExtensionCube", "mesh": 0}]
    data["scenes"] = [{"nodes": [0]}]
    data["meshes"] = [data["meshes"][0]]
    data["meshes"][0]["primitives"][0].pop("extensions", None)
    data["materials"] = [{
        "name": "Editable", "pbrMetallicRoughness": {
            "baseColorFactor": [0.65, 0.22, 0.08, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.35},
        "extensions": {CC: {"clearcoatFactor": 0.8, "clearcoatRoughnessFactor": 0.15},
                       TR: {"transmissionFactor": 0.45},
                       VENDOR: {"enabled": True, "gain": 0.5, "color": [0.2, 0.4, 0.6],
                                "label": "read only"}}}]
    data["extensionsUsed"] = [CC, TR, VENDOR]
    for key in ("extensions", "animations", "cameras"):
        data.pop(key, None)
    model = OUT / "extensions.gltf"
    model.write_text(json.dumps(data), encoding="utf-8")
    for extension in (CC, TR):
        stripped = json.loads(json.dumps(data))
        del stripped["materials"][0]["extensions"][extension]
        stripped["extensionsUsed"].remove(extension)
        (OUT / f"without_{extension}.gltf").write_text(json.dumps(stripped), encoding="utf-8")
    return model


def run_ui(model, name, lines):
    script, report, capture = (OUT / f"{name}{suffix}" for suffix in (".txt", ".json", ".png"))
    script.write_text("\n".join(["wait 8", "clickp material.section.general 0", "wait 3",
                                 "clickp material.section.pbr 0", "wait 3", "snap start"]
                                + lines + ["wait 10", "snap final", "move 900 20", "wait 220"]), encoding="utf-8")
    report.unlink(missing_ok=True)
    capture.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(model), *COMMON, "--ui-script", str(script),
                             "--ui-report", str(report), "--capture-viewport", str(capture),
                             "--capture-frame", "180"], cwd=REPO, capture_output=True, timeout=180)
    (OUT / f"{name}.log").write_bytes(result.stdout + result.stderr)
    if (result.returncode & 0xffffffff) == 0xc0000142:
        raise RuntimeError("Glider failed with 0xc0000142; stop launches, do not retry")
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    check(result.returncode == 0 and not data.get("failed", True), f"{name}: UI run completed {data.get('errors')}")
    return {snap["label"]: snap for snap in data.get("snaps", [])}, capture


def check_transmission_receiver():
    # Transmission excludes a material from decal receiving. Turning it off must
    # restore receiving as well as the opaque pass and the shader feature flag.
    import validate_asobo_draworder as draworder

    draworder.OUT = OUT / "receiver"
    base = draworder.make_base("receiver.gltf", [
        {"name": "receiver", "color": (0.0, 0.0, 0.0), "z": 0.0, "opaque": True},
        {"name": "decal", "color": draworder.RED, "z": 0.0003},
    ])
    data = json.loads(base.read_text(encoding="utf-8"))
    # A lit receiver exercises the forward property-injection path too.
    data["materials"][0]["extensions"] = {CC: {"clearcoatFactor": 0.8}, TR: {"transmissionFactor": 0.45}}
    data["materials"][1]["extensions"][draworder.DECAL] = {}
    data["extensionsUsed"] += [CC, TR, draworder.DECAL]
    model = OUT / "transmission_receiver.gltf"
    model.write_text(json.dumps(data), encoding="utf-8")
    del data["materials"][0]["extensions"][TR]
    data["extensionsUsed"].remove(TR)
    stripped = OUT / "transmission_receiver_without.gltf"
    stripped.write_text(json.dumps(data), encoding="utf-8")
    _, capture = run_ui(model, "receiver_transmission_off", [f"clickp khr.display.{TR} 0"])
    reference = render(EXE, stripped, "receiver_transmission_without", COMMON, OUT)
    check(same(capture, reference), "transmission off restores opaque decal receiving exactly")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-exe", type=Path, default=REPO / "build/baselines/f9cf00c.exe")
    parser.add_argument("--synthetic-only", action="store_true", help="development check; omit real-asset comparisons")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    model = write_fixture()
    inputs = {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in OUT.glob("*.gltf")}

    snaps, on = run_ui(model, "all_on", [])
    if "start" not in snaps:
        finish()
    tags = {p["tag"] for p in snaps["start"]["probes"]}
    check({f"khr.display.{ext}" for ext in (CC, TR)} <= tags, "declared KHR extensions have display switches")
    check(all(f"khr.header.{ext}" not in tags and f"khr.display.{ext}" not in tags for ext in ABSENT),
          "absent KHR extensions have neither headers nor switches")
    check(not any("create" in tag or "destroy" in tag or "remove" in tag for tag in tags if tag.startswith("khr.")),
          "no extension add/remove probe controls")
    source = (REPO / "source/imgui/MaterialInspector.cpp").read_text(encoding="utf-8")
    check(all(f"material->{verb}{ext}(" not in source for verb in ("create", "destroy")
              for ext in ("Unlit", "Clearcoat", "Transmission", "Volume", "Ior", "Specular", "Sheen",
                          "Iridescence", "EmissiveStrength", "PbrSpecularGlossiness")),
          "Inspector contains no extension membership mutations (including conversion)")
    check(f"ext.{VENDOR}.label" not in tags, "vendor string without schema enum has no edit widget")
    check({f"ext.{VENDOR}.{key}" for key in ("enabled", "gain", "color")} <= tags,
          "vendor bool, number and colour use editable probes")

    for name, lines, expected in (
        ("edit_khr", [f"dclickp khr.{CC}.clearcoatFactor 0", "wait 3", "key a ctrl", "type 0.25", "key enter"], ("Material",)),
        ("edit_vendor", [f"clickp ext.{VENDOR}.enabled 0"], ("Material", "Extension")),
    ):
        edited, _ = run_ui(model, name, lines)
        if "start" in edited and "final" in edited:
            a, b = (edited[label]["revisions"] for label in ("start", "final"))
            for kind in expected:
                check(b[kind] > a[kind], f"{name}: {kind} revision advances")
            check(b["syncs"] > a["syncs"], f"{name}: renderer resynchronizes")

    for ext in (CC, TR):
        toggled, off = run_ui(model, f"off_{ext}", [f"clickp khr.display.{ext} 0"])
        stripped = OUT / f"without_{ext}.gltf"
        reference = render(EXE, stripped, f"without_{ext}", COMMON, OUT)
        check(same(off, reference), f"{ext} off: byte-identical to file without extension")
        check(not same(off, on), f"{ext} off: fixture makes the display change visible")
        if "final" in toggled:
            a, b = (toggled[label]["revisions"] for label in ("start", "final"))
            check(b["ViewportSettings"] > a["ViewportSettings"] and b["Material"] == a["Material"]
                  and b["Extension"] == a["Extension"], f"{ext} switch changes display state only")
            check(any(p["tag"] == f"khr.header.{ext}" for p in toggled["final"]["probes"]),
                  f"{ext} remains in Inspector while disabled")
        restored, restored_capture = run_ui(model, f"restored_{ext}",
                                           [f"clickp khr.display.{ext} 0", "wait 5", f"clickp khr.display.{ext} 0"])
        check(same(restored_capture, on), f"{ext} on again restores exact rendering")

    independent, _ = run_ui(model, "viewport_independence", [
        "vpcreate", "aamode none 2", "vpsplit 2", "clickp Materials 0", "wait 5",
        "vpfocus 1", "wait 5", f"clickp khr.display.{TR} 0", "wait 5", "snap first_off",
        "vpfocus 2", "wait 5", "snap second_on", "vpfocus 1", "wait 5", "snap first_again",
    ])
    for label, applied in (("first_off", False), ("second_on", True), ("first_again", False)):
        probe = next((p for p in independent.get(label, {}).get("probes", [])
                      if p["tag"] == f"khr.display.{TR}"), None)
        check(probe is not None and probe["selected"] == applied,
              f"{label}: display switch belongs to the active viewport")

    baseline = render(args.baseline_exe, model, "fixture_baseline", COMMON, OUT)
    check(same(on, baseline), "all switches on: fixture byte-identical to f9cf00c baseline")
    check(all(hashlib.sha256(path.read_bytes()).hexdigest() == digest for path, digest in inputs.items()),
          "all input files remain byte-identical after edits and switches")
    check_transmission_receiver()
    if not args.synthetic_only:
        hello = REPO / "resources/sampleGltf/HelloCube.glb"
        a = render(EXE, hello, "hello_current", folder=OUT)
        b = render(args.baseline_exe, hello, "hello_baseline", folder=OUT)
        check(same(a, b), "HelloCube byte-identical to baseline")
        entry = view("cockpit_main_displays")
        a = render_view(EXE, entry, "e195_current", folder=OUT)
        b = render_view(args.baseline_exe, entry, "e195_baseline", folder=OUT)
        check(same(a, b), "E195 cockpit byte-identical to baseline")
        sdk = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
        da62 = sdk / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
        flags = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20",
                 "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid",
                 "--search-path", str(sdk / "Sources/Art/Max/textures")]
        a = render(EXE, da62, "da62_current", flags, OUT)
        b = render(args.baseline_exe, da62, "da62_baseline", flags, OUT)
        check(same(a, b), "DA62 forward view byte-identical to baseline")
    finish()


if __name__ == "__main__":
    main()
