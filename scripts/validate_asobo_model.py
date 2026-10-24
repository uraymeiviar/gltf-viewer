# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A0.1 typed ASOBO extension model (docs/ASOBO_EXTENSIONS_PHASE_0.md).

Loads generated fixtures with --asobo-report and checks the parsed model:
  * typed values, schema and exporter defaults; explicit keys versus defaults;
  * an extension used but not declared in extensionsUsed is ignored (info diagnostic);
  * exporter defaults and the exporter's keys and spellings (docs/ASOBO_EXTENSIONS_SEMANTICS.md); other
    keys outside the schema are kept;
  * out-of-range numbers are clamped and wrong types fall back to the default (warnings);
  * a malformed (non-object) extension is dropped by the glTF loader without crashing;
  * an extension on the wrong kind of object is reported; one without a schema is kept;
  * --reload-model rebuilds the values from the new file;
  * the real E195 assets: every declared ASOBO extension on every object is in the model, without warnings;
  * the generated spec table matches resources/schemas/asobo.
"""

import json
import harness_process as subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "asobo_model"
E195 = Path(r"F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model")
REAL_ASSETS = [E195 / "E195-Cockpit-LOD00.gltf", E195 / "E195-Exterior-LOD00.gltf"]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def document(materials, nodes, declared, asset_extensions=None):
    doc = {
        "asset": {"version": "2.0", "generator": "validate_asobo_model.py"},
        "extensionsUsed": declared,
        "scene": 0,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "materials": materials,
    }
    if asset_extensions:
        doc["asset"]["extensions"] = asset_extensions
    return doc


def material(name, extensions):
    return {"name": name, "extensions": extensions}


MODEL = document(
    materials=[
        material("draw_order", {"ASOBO_material_draw_order": {"drawOrderOffset": 25}}),
        material("decal", {"ASOBO_material_geometry_decal": {"enabled": True, "baseColorBlendFactor": 0.5,
                                                             "mode": "default", "decalChan0": True, "futureKey": 1}}),
        material("sail_out_of_range", {"ASOBO_material_sail": {"sailLightAbsorption": 2.0}}),
        material("invisible_default", {"ASOBO_material_invisible": {}}),
        material("invisible_wrong_type", {"ASOBO_material_invisible": {"enabled": "yes"}}),
        material("extra_occlusion", {"ASOBO_extra_occlusion_texture": {"extra_occlusion_texture": {"index": 0, "texCoord": 1}}}),
        material("uv_options", {"ASOBO_material_UV_options": {"UVTilingU": 2}}),
        material("malformed", {"ASOBO_material_emissive": 5}),
        material("no_schema", {"ASOBO_material_future_test": {"answer": 42}}),
        material("undeclared", {"ASOBO_material_alphamode_dither": {"enabled": True}}),
        material("uv_schema_spelling", {"ASOBO_material_UV_options": {"UVOffSetU": 0.25}}),
        material("extra_occlusion_exporter", {"ASOBO_extra_occlusion": {"extraOcclusionTexture": {"index": 0, "texCoord": 1}}}),
        material("draw_order_default", {"ASOBO_material_draw_order": {}}),
    ],
    nodes=[
        {"name": "with_id", "extensions": {"ASOBO_unique_id": {"id": "NODE_0"}}},
        {"name": "wrong_scope", "extensions": {"ASOBO_material_draw_order": {"drawOrderOffset": 1}}},
    ],
    declared=["ASOBO_material_draw_order", "ASOBO_material_geometry_decal", "ASOBO_material_sail",
              "ASOBO_material_invisible", "ASOBO_extra_occlusion_texture", "ASOBO_material_UV_options",
              "ASOBO_material_emissive", "ASOBO_material_future_test", "ASOBO_unique_id",
              "ASOBO_normal_map_convention", "ASOBO_extra_occlusion"],
    asset_extensions={"ASOBO_normal_map_convention": {"tangent_space_convention": "DirectX"}},
)

RELOADED = document(
    materials=[material("draw_order", {"ASOBO_material_draw_order": {"drawOrderOffset": 7}})],
    nodes=[{"name": "plain"}],
    declared=["ASOBO_material_draw_order"],
)


def report(model_path, extra=()):
    path = OUT / (Path(model_path).stem + ("_reload" if extra else "") + ".asobo.json")
    path.unlink(missing_ok=True)
    result = subprocess.run([str(EXE), str(model_path), "--isolated", *extra, "--asobo-report", str(path)],
                            cwd=REPO, capture_output=True, text=True, timeout=600)
    if result.returncode != 0 or not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def extension(rep, pointer, name):
    for obj in rep["objects"]:
        if obj["pointer"] == pointer:
            for ext in obj["extensions"]:
                if ext["name"] == name:
                    return ext
    return None


def diagnostics(rep, code, pointer_prefix):
    return [d for d in rep["diagnostics"] if d["code"] == code and d["pointer"].startswith(pointer_prefix)]


OUT.mkdir(parents=True, exist_ok=True)
model_path, reload_path = OUT / "asobo_model.gltf", OUT / "asobo_reloaded.gltf"
model_path.write_text(json.dumps(MODEL, indent=2), encoding="utf-8")
reload_path.write_text(json.dumps(RELOADED, indent=2), encoding="utf-8")

print("[spec table]")
gen = subprocess.run([sys.executable, str(REPO / "scripts" / "generate_asobo_specs.py"), "--check"], capture_output=True, text=True)
check(gen.returncode == 0, "generated spec table matches resources/schemas/asobo")

print("[fixture]")
rep = report(model_path)
check(rep is not None, "report written")
if rep:
    check(rep["specCount"] >= 38, f"spec table has {rep['specCount']} extensions")
    e = extension(rep, "/materials/0", "ASOBO_material_draw_order")
    check(e and e["values"].get("drawOrderOffset") == 25 and e["explicit"] == ["drawOrderOffset"], "draw order: typed number, explicit")
    e = extension(rep, "/materials/1", "ASOBO_material_geometry_decal")
    check(e and e["values"].get("baseColorBlendFactor") == 0.5 and e["values"].get("enabled") is True, "decal: values")
    check(e and e["values"].get("mode") == "default" and e["values"].get("decalChan0") is True
          and e["values"].get("decalChan1") is False, "decal: exporter keys outside the schema typed, with exporter defaults")
    check(e and e["unknown"] == {"futureKey": 1}, "decal: other unknown keys kept")
    check(e and e["values"].get("roughnessBlendFactor") == 1.0, "decal: exporter default blend factor 1.0")
    e = extension(rep, "/materials/2", "ASOBO_material_sail")
    check(e and e["values"].get("sailLightAbsorption") == 1.0, "sail: 2.0 clamped to the schema maximum 1.0")
    check(len(diagnostics(rep, "ASOBO_VALUE_RANGE", "/materials/2")) == 1, "sail: range warning")
    e = extension(rep, "/materials/3", "ASOBO_material_invisible")
    check(e and e["values"].get("enabled") is True and e["explicit"] == [], "invisible: schema default true, not explicit")
    e = extension(rep, "/materials/4", "ASOBO_material_invisible")
    check(e and e["values"].get("enabled") is True and e["explicit"] == [], "invisible: wrong type falls back to the default")
    check(len(diagnostics(rep, "ASOBO_VALUE_TYPE", "/materials/4")) == 1, "invisible: type warning")
    e = extension(rep, "/materials/5", "ASOBO_extra_occlusion_texture")
    tex = e and e["values"].get("extra_occlusion_texture")
    check(bool(tex) and tex["index"] == 0 and tex["texCoord"] == 1, "extra occlusion: texture reference with texCoord")
    e = extension(rep, "/materials/6", "ASOBO_material_UV_options")
    check(e and e["values"].get("UVTilingU") == 2 and e["values"].get("UVTilingV") == 1.0
          and e["values"].get("clampUVX") is False, "UV options: explicit value; exporter defaults (clamp false overrides the schema)")
    e = extension(rep, "/materials/10", "ASOBO_material_UV_options")
    check(e and e["values"].get("UVOffsetU") == 0.25 and e["unknown"] == {}, "UV options: schema spelling UVOffSetU read as UVOffsetU")
    e = extension(rep, "/materials/11", "ASOBO_extra_occlusion")
    tex = e and e["values"].get("extraOcclusionTexture")
    check(bool(tex) and tex["texCoord"] == 1, "extra occlusion: the exporter's extension name and key")
    e = extension(rep, "/materials/12", "ASOBO_material_draw_order")
    check(e and e["values"].get("drawOrderOffset") == 0 and e["explicit"] == [], "draw order: exporter default 0")
    # The glTF loader drops a non-object extension value (with a log warning, and the schema validation reports
    # it), so it never reaches the ASOBO parser; its own malformed-JSON fallback is a safety net only.
    check(extension(rep, "/materials/7", "ASOBO_material_emissive") is None, "malformed: dropped by the loader, no crash")
    e = extension(rep, "/materials/8", "ASOBO_material_future_test")
    check(e and not e["hasSpec"] and e["unknown"] == {"answer": 42}, "no schema: kept as metadata")
    check(len(diagnostics(rep, "ASOBO_EXTENSION_NO_SCHEMA", "/materials/8")) == 1, "no schema: info")
    check(extension(rep, "/materials/9", "ASOBO_material_alphamode_dither") is None, "undeclared: ignored")
    check(len(diagnostics(rep, "ASOBO_EXTENSION_UNDECLARED", "/materials/9")) == 1, "undeclared: info")
    e = extension(rep, "/nodes/0", "ASOBO_unique_id")
    check(e and e["values"].get("id") == "NODE_0", "node: unique id")
    check(len(diagnostics(rep, "ASOBO_EXTENSION_SCOPE", "/nodes/1")) == 1, "material extension on a node: scope warning")
    e = extension(rep, "/asset", "ASOBO_normal_map_convention")
    check(e and e["values"].get("tangent_space_convention") == "DirectX", "asset: normal map convention")

print("[reload]")
rep = report(model_path, ["--reload-model", str(reload_path)])
check(rep is not None, "report written after --reload-model")
if rep:
    e = extension(rep, "/materials/0", "ASOBO_material_draw_order")
    check(e and e["values"].get("drawOrderOffset") == 7, "values come from the reloaded file")
    check(rep["declared"] == ["ASOBO_material_draw_order"], "declared list replaced on reload")
    check(len(rep["objects"]) == 1, "no objects left over from the first file")

print("[real assets]")
for asset in REAL_ASSETS:
    if not asset.exists():
        print(f"  SKIP {asset.name} (not found)")
        continue
    rep = report(asset)
    check(rep is not None, f"{asset.name}: report written")
    if not rep:
        continue
    doc = json.loads(asset.read_text(encoding="utf-8"))
    declared = set(doc.get("extensionsUsed", []))
    expected = 0
    for collection in ("materials", "nodes", "meshes", "animations"):
        for obj in doc.get(collection, []):
            expected += sum(1 for k in obj.get("extensions", {}) if k.startswith("ASOBO_") and k in declared)
    expected += sum(1 for k in doc["asset"].get("extensions", {}) if k.startswith("ASOBO_") and k in declared)
    parsed = sum(len(o["extensions"]) for o in rep["objects"])
    check(parsed == expected, f"{asset.name}: {parsed} of {expected} declared ASOBO extension instances parsed")
    warnings = [d for d in rep["diagnostics"] if d["severity"] == "warning"]
    check(not warnings, f"{asset.name}: no warnings ({len(warnings)}{': ' + warnings[0]['message'] if warnings else ''})")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
