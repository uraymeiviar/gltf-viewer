// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Result.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include <optional>
#include <unordered_map>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#primitive

namespace ux3d::gltf {
class Material;
class MaterialVariant;

enum class MaterialVariantsError : uint32_t {
    UniquenessViolated = core::DerivedResultStart
};
using MaterialVariantsResult = core::ResultT<MaterialVariantsError>;

class MaterialVariantPrimitive : public GltfObject {
public:
    MaterialVariantPrimitive(core::Vector<const MaterialVariant*> _pVariants, Material* _material)
        : pVariants(_pVariants)
        , material(_material) {};
    MaterialVariantPrimitive(Material* _material, core::Vector<const MaterialVariant*>&& _pVariants)
        : pVariants(_pVariants)
        , material(_material) {};
    core::Vector<const MaterialVariant*> pVariants;
    Material* material;
};

class MaterialVariantsPrimitiveMapping : public GltfObject {
public:
    MaterialVariantsPrimitiveMapping(MaterialVariantsPrimitiveMapping&&) = delete;
    explicit MaterialVariantsPrimitiveMapping() { }
    MaterialVariantsPrimitiveMapping(const MaterialVariantsPrimitiveMapping& _source)
        : m_materialVariantMappings(_source.m_materialVariantMappings)
    {
    }

    // returns false if uniqueness of variantIds is violated
    MaterialVariantsResult addVariant(const MaterialVariant* _variant, Material* _material);

    // returns false if uniqueness of variantIds is violated
    MaterialVariantsResult addVariant(core::Vector<MaterialVariant*>& _variants, Material* _material);

    // removes variant mapping
    void removeVariant(const MaterialVariant* _variant);
    void resetVariants();

    bool hasVariant(const MaterialVariant* _variant) const { return getMaterial(_variant).has_value(); }
    bool hasVariant(uint32_t _variantID) const;
    bool hasMappingForMaterial(const Material* _material) const { return getMappingsForMaterial(_material).has_value(); }
    std::optional<Material*> getMaterial(const MaterialVariant* _variant) const;
    Material* getMaterial(uint32_t _variantID) const;
    std::optional<core::Vector<const gltf::MaterialVariant*>> getMappingsForMaterial(const Material* _material) const;

    const core::Vector<MaterialVariantPrimitive>& getMappings() const { return m_materialVariantMappings; };
    core::Vector<MaterialVariantPrimitive>& getMappings() { return m_materialVariantMappings; };

private:
    core::Vector<MaterialVariantPrimitive> m_materialVariantMappings;
    std::optional<MaterialVariantPrimitive*> findMaterialVariant(const Material* _material);
};
}
