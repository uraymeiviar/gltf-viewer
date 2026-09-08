#include "gltf/mesh/MaterialVariantsMapping.h"
#include "core/Logger.h"
#include "gltf/Material.h"
#include "gltf/MaterialVariant.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::gltf;

MaterialVariantsResult MaterialVariantsPrimitiveMapping::addVariant(const MaterialVariant* _variant, Material* _material)
{
    if (hasVariant(_variant)) {
        core::logWarning("Material %s and/or variant %s are already part of this primitive.", _material->getName().c_str(), _variant->getName().c_str());
        return MaterialVariantsError::UniquenessViolated;
    }
    auto materialVariantPrimitve = findMaterialVariant(_material);
    if (materialVariantPrimitve.has_value()) {
        if (std::find(materialVariantPrimitve.value()->pVariants.begin(), materialVariantPrimitve.value()->pVariants.end(),
                _variant)
            == materialVariantPrimitve.value()->pVariants.end()) {
            materialVariantPrimitve.value()->pVariants.emplace_back(_variant);
        }
    } else {
        core::Vector<const MaterialVariant*> materialVariants;
        materialVariants.emplace_back(_variant);
        gltf::MaterialVariantPrimitive materialVariantsMapping(_material, std::move(materialVariants));
        m_materialVariantMappings.emplace_back(std::move(materialVariantsMapping));
    }
    return MaterialVariantsResult::Success;
}

std::optional<MaterialVariantPrimitive*> MaterialVariantsPrimitiveMapping::findMaterialVariant(const Material* _material)
{
    auto result = std::find_if(m_materialVariantMappings.begin(),
        m_materialVariantMappings.end(),
        [&](const MaterialVariantPrimitive& materialVariantPrimitive) -> bool {
            return materialVariantPrimitive.material == _material;
        });

    if (result == m_materialVariantMappings.end()) {
        return std::nullopt;
    }
    return std::make_optional(&*result);
}

MaterialVariantsResult MaterialVariantsPrimitiveMapping::addVariant(core::Vector<MaterialVariant*>& _variants, Material* _material)
{
    MaterialVariantsResult result = MaterialVariantsResult::Success;
    if (_material == nullptr) {
        return result;
    }
    for (const MaterialVariant* variant : _variants) {
        if (addVariant(variant, _material) != MaterialVariantsResult::Success) {
            result = MaterialVariantsError::UniquenessViolated;
        }
    }
    return result;
}

void MaterialVariantsPrimitiveMapping::removeVariant(const MaterialVariant* _variant)
{
    for (auto it = m_materialVariantMappings.begin(); it != m_materialVariantMappings.end(); it++) {
        for (auto it2 = it->pVariants.begin(); it2 != it->pVariants.end(); it2++) {
            if (*it2 == _variant) {
                // remove current VariantMapping
                it->pVariants.erase(it2);
                if (it->pVariants.size() == 0) {
                    m_materialVariantMappings.erase(it);
                }
                return;
            }
        }
    }
}

void MaterialVariantsPrimitiveMapping::resetVariants()
{
    m_materialVariantMappings.clear();
}

bool MaterialVariantsPrimitiveMapping::hasVariant(uint32_t _variantID) const
{
    for (auto& mapping : m_materialVariantMappings) {
        for (const MaterialVariant* pVariant : mapping.pVariants) {
            if (pVariant->m_ID == _variantID) {
                return true;
            }
        }
    }

    return false;
}

std::optional<Material*> MaterialVariantsPrimitiveMapping::getMaterial(const MaterialVariant* _variant) const
{

    for (auto& mapping : m_materialVariantMappings) {
        for (const MaterialVariant* pVariant : mapping.pVariants) {
            if (pVariant == _variant) {
                return std::make_optional(mapping.material);
            }
        }
    }
    return std::nullopt;
}

Material* MaterialVariantsPrimitiveMapping::getMaterial(uint32_t _variantID) const
{

    for (auto& mapping : m_materialVariantMappings) {
        for (const MaterialVariant* pVariant : mapping.pVariants) {
            if (pVariant->m_ID == _variantID) {
                return mapping.material;
            }
        }
    }
    return nullptr;
}

std::optional<core::Vector<const gltf::MaterialVariant*>> MaterialVariantsPrimitiveMapping::getMappingsForMaterial(const Material* _material) const
{
    for (auto& mapping : m_materialVariantMappings) {
        if (mapping.material == _material) {
            return std::make_optional(mapping.pVariants);
        }
    }
    return std::nullopt;
}
