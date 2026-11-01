// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

// 3D Tiles / Cesium glTF extensions Glider recognises (the glb payloads of 3D Tiles 1.1 tilesets):
// EXT_structural_metadata (root schema / property tables / property textures / property attributes, and per primitive
// propertyTextures / propertyAttributes), EXT_mesh_features (per primitive feature ID sets), EXT_instance_features (per
// node, with EXT_mesh_gpu_instancing), CESIUM_primitive_outline (per primitive outline edge indices) and CESIUM_RTC
// (root: positions are relative to `center`). None changes what Glider draws: the geometry renders as plain glTF.
// The loader keeps them as unknown extensions (JSON text) on the root, primitives and nodes; summarize() reads that
// text for the Inspector, Statistics and get_state (read-only counts and names).
//
// CESIUM_RTC: the file's positions are relative to `center` (ECEF-like, ~6.4e6 m from the origin). Glider renders the
// scene in that relative-to-center frame, i.e. the center is subtracted (the scene root is not translated by it), so
// the geometry stays near the origin where single-precision positions, framing and clipping planes work; the center
// is shown (Statistics, get_state). A render therefore equals the file without CESIUM_RTC.

#include "math/VectorTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ux3d::gltf {
class Gltf;

namespace tiles {
inline constexpr const char* StructuralMetadata = "EXT_structural_metadata";
inline constexpr const char* MeshFeatures = "EXT_mesh_features";
inline constexpr const char* InstanceFeatures = "EXT_instance_features";
inline constexpr const char* PrimitiveOutline = "CESIUM_primitive_outline";
inline constexpr const char* Rtc = "CESIUM_RTC";

bool isTilesExtension(std::string_view name);

// One feature ID set of EXT_mesh_features / EXT_instance_features.
struct FeatureIdSet {
    uint64_t featureCount = 0;
    std::string source;     // "attribute _FEATURE_ID_<n>", "texture <index>" or "implicit (vertex / instance index)"
    int64_t propertyTable = -1;
    std::string label;
    std::optional<int64_t> nullFeatureId;
};
std::vector<FeatureIdSet> parseFeatureIds(const std::string& extensionJson, bool instances);

struct PropertyTable {
    std::string name;
    std::string className;
    uint64_t count = 0;
    std::vector<std::string> properties;
};

struct Summary {
    std::optional<math::dvec3> rtcCenter;  // CESIUM_RTC.center
    bool structuralMetadata = false;       // root EXT_structural_metadata present
    std::string schemaId;
    std::string schemaName;
    std::string schemaUri;                 // external schema (not read)
    std::vector<std::string> classNames;
    uint32_t enums = 0;
    std::vector<PropertyTable> propertyTables;
    uint32_t propertyTextures = 0;
    uint32_t propertyAttributes = 0;
    uint32_t meshFeaturePrimitives = 0;    // primitives with EXT_mesh_features
    uint32_t featureIdSets = 0;
    uint32_t featureIdAttributes = 0;
    uint32_t featureIdTextures = 0;
    uint32_t featureIdImplicit = 0;
    uint32_t metadataPrimitives = 0;       // primitives with EXT_structural_metadata (property textures / attributes)
    uint32_t instanceFeatureNodes = 0;     // nodes with EXT_instance_features
    uint32_t instanceFeatureIdSets = 0;
    uint32_t outlinePrimitives = 0;        // primitives with CESIUM_primitive_outline
    bool any() const
    {
        return rtcCenter.has_value() || structuralMetadata || meshFeaturePrimitives != 0 || metadataPrimitives != 0
            || instanceFeatureNodes != 0 || outlinePrimitives != 0;
    }
};
Summary summarize(const Gltf& gltf);
}
}
