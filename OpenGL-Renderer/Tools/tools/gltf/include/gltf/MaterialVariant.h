#pragma once

#include "ChildOfRoot.h"

//DEV NOTES:
// Material Variants are globally stored in gltf, Primitives can have Material Variants

// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_variants

namespace ux3d::gltf {
class MaterialVariant : public ChildOfRoot {

public:
    MaterialVariant(uint32_t _id); // regular constructor
    MaterialVariant(uint32_t _id, const MaterialVariant& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::MaterialVariant; }

    Extensions getUsedExtensions() const override { return Extension::MaterialsVariants; }
    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;
};
}
