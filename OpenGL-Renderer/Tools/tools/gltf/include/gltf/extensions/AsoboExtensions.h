// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ux3d::gltf {
class Gltf;
class GltfObject;
}

// Typed model of the MSFS ASOBO_* glTF extensions (docs/ASOBO_EXTENSIONS_PHASE_0.md, A0.1).
// The parser reads an object's extension JSON once into typed values with their defaults. The spec table
// (AsoboExtensionSpecs.cpp) is generated from resources/schemas/asobo by scripts/generate_asobo_specs.py.
// Rules: extensions the file does not declare in extensionsUsed are ignored; keys outside the schema are kept;
// out-of-range numbers are clamped and type mismatches fall back to the default, each with a diagnostic.
namespace ux3d::gltf::asobo {

// Where an extension may appear (bit set).
namespace Scope {
inline constexpr uint32_t Asset = 1u << 0;
inline constexpr uint32_t Material = 1u << 1;
inline constexpr uint32_t Node = 1u << 2;
inline constexpr uint32_t Mesh = 1u << 3;
inline constexpr uint32_t Animation = 1u << 4;
inline constexpr uint32_t Root = 1u << 5;
inline constexpr uint32_t Any = Asset | Material | Node | Mesh | Animation | Root;
}

enum class ValueType { Boolean, Number, String, NumberArray, Texture, Json };

// Where a property's default comes from: none (the property is unset unless the file has it), the schema, or
// the MSFS 2024 Blender exporter.
enum class DefaultSource { None, Schema, Exporter };

// A textureInfo / normalTextureInfo / occlusionTextureInfo value. The index refers to the glTF textures array.
struct TextureRef {
    int32_t index = -1;
    int32_t texCoord = 0;
    double scale = 1.0; // "scale" (normal textures) or "strength" (occlusion textures)
    nlohmann::json extensions; // e.g. KHR_texture_transform, kept as JSON

    bool operator==(const TextureRef&) const = default;
};

using Value = std::variant<std::monostate, bool, double, std::string, std::vector<double>, TextureRef, nlohmann::json>;

struct PropertySpec {
    std::string_view name;
    ValueType type;
    Value defaultValue;
    DefaultSource defaultSource = DefaultSource::None;
    std::optional<double> minimum;
    std::optional<double> maximum;
    uint32_t minItems = 0;
    uint32_t maxItems = 0;
    // The schema's spelling when the exporter writes the key differently (UVOffSetU / UVOffsetU); both are read.
    std::string_view alias;
};

struct ExtensionSpec {
    std::string_view name;
    uint32_t scopes;
    const char* schemaFile; // relative to resources/schemas/asobo; nullptr when the extension has no schema
    std::vector<PropertySpec> properties;

    const PropertySpec* findProperty(std::string_view key) const;
};

const std::vector<ExtensionSpec>& extensionSpecs();
const ExtensionSpec* findExtensionSpec(std::string_view name);
bool isAsoboExtension(std::string_view name);

struct Diagnostic {
    enum class Severity { Info, Warning };
    Severity severity;
    std::string code; // ASOBO_EXTENSION_UNDECLARED, _NO_SCHEMA, _SCOPE, _MALFORMED, ASOBO_VALUE_RANGE, _TYPE
    std::string pointer; // JSON pointer, e.g. /materials/3/extensions/ASOBO_material_draw_order/drawOrderOffset
    std::string message;
};

// One parsed extension on one object.
class ExtensionData {
public:
    std::string_view name() const { return m_name; }
    const ExtensionSpec* spec() const { return m_spec; } // nullptr for an ASOBO extension without a spec

    // True when the file sets the key (with a usable value); false when the value is the default.
    bool isExplicit(std::string_view key) const;
    // The file's value, else the default; nullptr when neither exists.
    const Value* value(std::string_view key) const;

    std::optional<bool> boolean(std::string_view key) const;
    std::optional<double> number(std::string_view key) const;
    const std::string* string(std::string_view key) const;
    const std::vector<double>* numbers(std::string_view key) const;
    const TextureRef* texture(std::string_view key) const;
    const nlohmann::json* json(std::string_view key) const;

    // Keys the spec does not list (for example geometry_decal's decalChan0-2 and mode), kept as they are.
    const nlohmann::json& unknownKeys() const { return m_unknownKeys; }

private:
    friend class Parser;
    std::string m_name;
    const ExtensionSpec* m_spec = nullptr;
    std::vector<std::pair<std::string, Value>> m_values; // every spec property with a value or default
    std::vector<std::string> m_explicitKeys;
    nlohmann::json m_unknownKeys = nlohmann::json::object();
};

// The declared ASOBO extensions of one object.
class ExtensionSet {
public:
    const ExtensionData* find(std::string_view name) const;
    const std::vector<ExtensionData>& all() const { return m_extensions; }
    bool empty() const { return m_extensions.empty(); }

private:
    friend class Parser;
    std::vector<ExtensionData> m_extensions;
};

// Parses the ASOBO extensions of one object (scope is one Scope bit). Diagnostics are appended when requested;
// pointer is the object's JSON pointer, e.g. /materials/3.
ExtensionSet parseExtensions(const Gltf& gltf, const GltfObject& object, uint32_t scope,
                             const std::string& pointer = {}, std::vector<Diagnostic>* diagnostics = nullptr);

// Parses every object of the document and returns all diagnostics, for the validator.
std::vector<Diagnostic> validateExtensions(const Gltf& gltf);

// A3.7: ASOBO_sky_portal as Glider reads it (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.7; semantics section 19).
// A disc area light at the node, facing into the cabin along the node's local +Z axis (the MSFS Blender exporter
// turns light nodes by 180 degrees about X, so its "direction" ends up on +Z; verified on the DA62 SDK cockpit).
struct SkyPortal {
    int shapeType = 1;              // 1 = disc, the only portal shape
    double sourceRadiusCm = 50.0;   // exporter unit: centimetres (Blender preview radius = value / 100 m)
    double innerConeDeg = 0.0;      // full cone angles in degrees, clamped to [0, 180], inner <= outer
    double outerConeDeg = 160.0;

    double radiusMetres() const { return sourceRadiusCm * 0.01; }
    // The exporter preview's inner circle (Blender spot blend: sin(inner/2) / sin(outer/2), outer clamped to 179).
    double innerCircleRadiusMetres() const;
};

// The portal of a parsed node extension set, or nothing when the node has no (applicable) ASOBO_sky_portal.
std::optional<SkyPortal> readSkyPortal(const ExtensionSet& nodeExtensions);

} // namespace ux3d::gltf::asobo
