// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include <optional>

// KHR_materials_dispersion (https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_dispersion):
// dispersion = 20 / Abbe number (>= 0, default 0). It only changes volumetric refraction (KHR_materials_transmission
// with KHR_materials_volume): the refracted scene is sampled once per colour channel with an IOR spread around the
// material's IOR (the Khronos sample viewer: halfSpread = (ior - 1) * 0.025 * dispersion; red ior - halfSpread,
// green ior, blue ior + halfSpread). Animatable through KHR_animation_pointer (PointerPath).

namespace ux3d::gltf {
class Gltf;

class Dispersion : public GltfObject {
public:
    static constexpr const char* ExtensionName = "KHR_materials_dispersion";
    static constexpr const char* PointerPath = "extensions/KHR_materials_dispersion/dispersion";

    bool hasDispersion() const { return m_dispersion.has_value(); }

    GltfResult setDispersion(double value)
    {
        if (value < 0) {
            return GltfError::InvalidInput;
        }
        m_dispersion = value;
        return GltfResult::Success;
    }

    void unsetDispersion() { m_dispersion.reset(); }
    double getDispersionOrDefault() const { return m_dispersion.value_or(0.0); }

    bool isValid() const { return true; }
    GltfResult getInvalidProperties(core::Vector<core::string>&) const { return GltfResult::Success; }

private:
    std::optional<double> m_dispersion;
};

}
