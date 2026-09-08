#pragma once

#include "ConversionResult.h"
#include "PointerLookup.h"
#include "core/LinkedList.h"
#include "core/String.h"
#include "gltf/ImportOptions.h"
#include "nlohmann/json.hpp"
#include <optional>

namespace ux3d::gltf {
class Gltf;
class MaterialVariant;
class ChildOfRootFromJson;

class GltfFromJson {
public:
    explicit GltfFromJson(Gltf& _outGltf)
        : m_gltf(_outGltf)
    {
    }

    ConversionResult convert(const core::string& _gltfString, std::optional<ImportOptions> importOptions = std::nullopt);

private:
    ConversionResult createObjects();
    ConversionResult resolveDependencies();
    void convertRootExtensions();
    void convertRequiredExtensions();
    void convertAsset();

    ConversionResult create(const nlohmann::json::array_t& _jsonObjects, ChildOfRootFromJson& _converter);

    template <typename GltfObject, typename Converter>
    ConversionResult resolve(const nlohmann::json& _jsonObjects, core::LinkedList<GltfObject>& _gltfObjects, Converter& _converter);
    ConversionResult resolveDefaultScene(const nlohmann::json& _jsonScene);
    ConversionResult resolveMaterialVariants(const nlohmann::json& _jsonMaterialVariants);
    ConversionResult resolveAsset(const nlohmann::json& _jsonMaterialVariants);

    nlohmann::json m_json;
    Gltf& m_gltf;
    PointerLookup m_pointerLookup;
};
}
