# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Generates the ASOBO extension spec table (A0.1) from resources/schemas/asobo.

Usage:
  python scripts/generate_asobo_specs.py          # rewrite OpenGL-Renderer/.../AsoboExtensionSpecs.cpp
  python scripts/generate_asobo_specs.py --check  # exit 1 if the committed table is out of date

Each gltf.<extension>.schema.json gives one extension: its property names, types, ranges and the few schema
defaults. Most properties have no schema default (112 of 133); their defaults come from the MSFS 2024 Blender
exporter (docs/ASOBO_EXTENSIONS_SEMANTICS.md) through EXPORTER_DEFAULTS below. The other exporter tables add
the keys it writes outside the schema, its spellings of schema keys, and the extensions it writes without a
schema. Ranges stay the schema's (the exporter's Blender UI limits are not enforced), except the array lengths
in ITEM_COUNTS. The extension name comes from the file name, since several schema titles are wrong
(geometry_decal's says blend_gbuffer).
"""

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SCHEMAS = REPO / "resources" / "schemas" / "asobo"
OUTPUT = REPO / "OpenGL-Renderer" / "Tools" / "tools" / "gltf" / "source" / "extensions" / "AsoboExtensionSpecs.cpp"

# Where each extension may appear. Unlisted ASOBO_material_* extensions are Material; anything else unlisted is Any.
SCOPES = {
    "ASOBO_normal_map_convention": "Asset",
    "ASOBO_asset_optimized": "Asset",
    "ASOBO_occlusion_strength": "Material",
    "ASOBO_extra_occlusion_texture": "Material",
    "ASOBO_extra_occlusion": "Material",
    "ASOBO_collision_object": "Node",
    "ASOBO_gizmo_object": "Node",
    "ASOBO_macro_light": "Node",
    "ASOBO_sky_portal": "Node",
    "ASOBO_unique_id": "Node",
    "ASOBO_tags": "Material | Scope::Node",
    "ASOBO_softbody_mesh": "Mesh",
}

# Everything below comes from the MSFS 2024 Blender exporter 8.4.1, as extracted in
# docs/ASOBO_EXTENSIONS_SEMANTICS.md (A0.6; the section for each extension cites the exporter lines).

# Defaults the exporter uses (its Blender property defaults; a key is usually omitted when it has its default).
# These replace a missing schema default; where the schema has one, the exporter's wins only when listed here.
EXPORTER_DEFAULTS = {
    "ASOBO_occlusion_strength": {"strength": 1.0},
    "ASOBO_material_emissive": {"emissiveDayMultiplier": 1.0, "emissiveNightMultiplier": 1.0},
    "ASOBO_material_draw_order": {"drawOrderOffset": 0.0},
    "ASOBO_material_geometry_decal": {k: 1.0 for k in ("baseColorBlendFactor", "metallicBlendFactor", "roughnessBlendFactor",
                                                         "normalBlendFactor", "emissiveBlendFactor", "occlusionBlendFactor")},
    "ASOBO_material_blend_gbuffer": {k: 1.0 for k in ("baseColorBlendFactor", "metallicBlendFactor", "roughnessBlendFactor",
                                                        "normalBlendFactor", "emissiveBlendFactor", "occlusionBlendFactor")},
    "ASOBO_material_glass_v2": {"glassWidth": 0.0},
    "ASOBO_material_windshield_v3": {
        "detail1Rough": 0.0, "detail2Rough": 0.0, "detail1Opacity": 0.0, "detail2Opacity": 0.0,
        "microScratchesTiling": 1.0, "microScratchesStrength": 1.0, "detailNormalRefractScale": 1.0,
        "wiperLines": False, "wiperLinesTiling": 1.0, "wiperLinesStrength": 1.0, "wiper1State": 0.0},
    "ASOBO_material_fresnel_fade": {"fresnelFactor": 1.0, "fresnelOpacityOffset": 1.0},
    "ASOBO_material_ghost_effect": {"bias": 0.0, "scale": 1.0, "power": 1.0},
    "ASOBO_material_detail_map": {"UVScale": 1.0, "blendThreshold": 0.001},
    # The schema says true, but Blender defaults to false and the exporter never writes false, so a missing key
    # means false (the E195 cockpit tiles a material 2x without the key; clamping would stretch it).
    "ASOBO_material_UV_options": {"clampUVX": False, "clampUVY": False, "UVOffsetU": 0.0, "UVOffsetV": 0.0,
                                  "UVTilingU": 1.0, "UVTilingV": 1.0, "UVRotation": 0.0},
    "ASOBO_material_parallax_window": {"parallaxScale": 0.5, "roomSizeXScale": 0.5, "roomSizeYScale": 0.5,
                                       "roomNumberXY": 5.0},
    # A3.4: both exporters (2024 Blender, MSFS 2020 3ds Max) default the SSS colour to white RGBA.
    "ASOBO_material_SSS": {"SSSColor": [1.0, 1.0, 1.0, 1.0]},
    # A3.7: the exporter's Blender light properties (blender/msfs_lights.py: Source Radius (cm) 50, Inner Angle 0,
    # Outer Angle 160); a sky portal's shape is always "disc" = 1 (io/com/msfs_light_utils.py). The exporter writes
    # all four keys, so these defaults only matter for hand-written files.
    "ASOBO_sky_portal": {"shape_type": 1, "source_radius": 50.0, "inner_cone_angle": 0.0, "outer_cone_angle": 160.0},
}

# Array lengths the exporters write where the schema disagrees: {extension: {key: (minItems, maxItems)}}.
# A3.4: the SSS schema says 2 items, but every exporter writes an RGBA list and every file found has 4;
# RGB (3) is accepted too, and a 2-item list keeps the warning and the default.
ITEM_COUNTS = {
    "ASOBO_material_SSS": {"SSSColor": (3, 4)},
}

# Keys the exporter writes that the schema does not list: {extension: {key: (type, default or None)}}.
EXPORTER_PROPERTIES = {
    "ASOBO_material_geometry_decal": {
        "mode": ("String", ""), "normalOverrideFactor": ("Number", 1.0), "blendSharpnessFactor": ("Number", 0.0),
        "underClearcoat": ("Boolean", True), "decalChan0": ("Boolean", True), "decalChan1": ("Boolean", False),
        "decalChan2": ("Boolean", True), "blendMaskTexture": ("Texture", None)},
    "ASOBO_material_windshield_v3": {
        "UVScale": ("Number", 1.0), "ssrAttenuation": ("Number", 1.0), "cubemapReflectionMasking": ("Boolean", False)},
    "ASOBO_material_parallax_window": {"corridor": ("Boolean", False)},
}

# Keys the exporter spells differently from the schema: {extension: {schema key: exporter key}}. The property is
# named after the exporter (what files contain); the schema spelling is still accepted.
KEY_ALIASES = {
    "ASOBO_material_UV_options": {"UVOffSetU": "UVOffsetU", "UVOffSetV": "UVOffsetV"},
}

# Extensions the exporter writes that have no schema in resources/schemas/asobo:
# {name: {property: (type, default or None)}}.
EXTRA_EXTENSIONS = {
    "ASOBO_unique_id": {"id": ("String", None)},
    "ASOBO_asset_optimized": {},
    # The exporter's name and key for what the schema calls ASOBO_extra_occlusion_texture / extra_occlusion_texture;
    # it always writes texCoord 1.
    "ASOBO_extra_occlusion": {"extraOcclusionTexture": ("Texture", None)},
}


def property_type(spec):
    text = json.dumps(spec)
    if "textureInfo.schema.json" in text or "TextureInfo" in text:
        return "Texture"
    kind = spec.get("type")
    if kind == "boolean":
        return "Boolean"
    if kind in ("number", "integer"):
        return "Number"
    if kind == "string" or "enum" in text:
        return "String"
    if kind == "array" and spec.get("items", {}).get("type") == "number":
        return "NumberArray"
    return "Json"


def cpp_value(value):
    if value is None:
        return "Value{}"
    if isinstance(value, bool):
        return f"Value{{{'true' if value else 'false'}}}"
    if isinstance(value, (int, float)):
        return f"Value{{{float(value)!r}}}"
    if isinstance(value, str):
        return f"Value{{std::string({json.dumps(value)})}}"
    if isinstance(value, list):
        return "Value{std::vector<double>{" + ", ".join(repr(float(v)) for v in value) + "}}"
    raise ValueError(f"unsupported default {value!r}")


def optional(value):
    return "std::nullopt" if value is None else repr(float(value))


def load_extensions():
    extensions = {}
    for path in sorted(SCHEMAS.glob("*/gltf.*.schema.json")):
        name = path.name[len("gltf."):-len(".schema.json")]
        schema = json.loads(path.read_text(encoding="utf-8"))
        properties = []
        aliases = KEY_ALIASES.get(name, {})
        defaults = EXPORTER_DEFAULTS.get(name, {})
        for key, spec in schema.get("properties", {}).items():
            if key in ("extensions", "extras"):
                continue
            exporter_key = aliases.get(key, key)
            default, source = spec.get("default"), "Schema" if "default" in spec else "None"
            if exporter_key in defaults:
                default, source = defaults[exporter_key], "Exporter"
            min_items, max_items = ITEM_COUNTS.get(name, {}).get(exporter_key,
                                                                (spec.get("minItems", 0), spec.get("maxItems", 0)))
            properties.append({
                "name": exporter_key, "alias": key if exporter_key != key else "", "type": property_type(spec),
                "default": default, "source": source, "minimum": spec.get("minimum"), "maximum": spec.get("maximum"),
                "minItems": min_items, "maxItems": max_items,
            })
        for key, (kind, default) in EXPORTER_PROPERTIES.get(name, {}).items():
            properties.append(extra_property(key, kind, default))
        known = {p["name"] for p in properties}
        stale = (set(defaults) | set(aliases.values()) | set(ITEM_COUNTS.get(name, {}))) - known
        if stale:
            sys.exit(f"{name}: exporter tables name unknown properties {sorted(stale)}")
        extensions[name] = {"properties": properties, "schema": f"{path.parent.name}/{path.name}"}
    for name, props in EXTRA_EXTENSIONS.items():
        if name in extensions:
            sys.exit(f"{name} now has a schema; remove it from EXTRA_EXTENSIONS")
        extensions[name] = {"schema": None,
                            "properties": [extra_property(key, kind, default) for key, (kind, default) in props.items()]}
    return dict(sorted(extensions.items()))


def extra_property(key, kind, default):
    return {"name": key, "alias": "", "type": kind, "default": default,
            "source": "Exporter" if default is not None else "None",
            "minimum": None, "maximum": None, "minItems": 0, "maxItems": 0}


def scope_of(name):
    if name in SCOPES:
        return SCOPES[name]
    return "Material" if name.startswith("ASOBO_material_") else "Any"


def render(extensions):
    lines = [
        "// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.",
        "// Generated by scripts/generate_asobo_specs.py from resources/schemas/asobo. Do not edit by hand:",
        "// change the schemas or the generator's tables, then rerun it.",
        "",
        '#include "gltf/extensions/AsoboExtensions.h"',
        "",
        "namespace ux3d::gltf::asobo {",
        "",
        "const std::vector<ExtensionSpec>& extensionSpecs()",
        "{",
        "    static const std::vector<ExtensionSpec> specs = {",
    ]
    for name, ext in extensions.items():
        schema = json.dumps(ext["schema"]) if ext["schema"] else "nullptr"
        lines.append(f"        {{\"{name}\", Scope::{scope_of(name)}, {schema}, {{")
        for p in ext["properties"]:
            lines.append(
                f"            {{\"{p['name']}\", ValueType::{p['type']}, {cpp_value(p['default'])}, "
                f"DefaultSource::{p['source']}, {optional(p['minimum'])}, {optional(p['maximum'])}, "
                f"{p['minItems']}, {p['maxItems']}, \"{p['alias']}\"}},")
        lines.append("        }},")
    lines += ["    };", "    return specs;", "}", "", "} // namespace ux3d::gltf::asobo", ""]
    return "\n".join(lines)


parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--check", action="store_true", help="fail if the committed table differs from the schemas")
args = parser.parse_args()

text = render(load_extensions())
if args.check:
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
    if current.replace("\r\n", "\n") != text:
        sys.exit(f"{OUTPUT.relative_to(REPO)} is out of date; run scripts/generate_asobo_specs.py")
    print("ASOBO spec table is up to date")
else:
    OUTPUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUTPUT.relative_to(REPO)} ({len(load_extensions())} extensions)")
