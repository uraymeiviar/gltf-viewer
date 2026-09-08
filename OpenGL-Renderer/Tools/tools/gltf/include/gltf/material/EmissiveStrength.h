// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include <optional>

namespace ux3d::gltf {
class Gltf;

class EmissiveStrength : public GltfObject {
public:
    bool hasEmissiveStrength() const { return m_emissiveStrength.has_value(); }

    GltfResult setEmissiveStrength(double value)
    {
        if (value < 0) {
            return GltfError::InvalidInput;
        }
        m_emissiveStrength = value;
        return GltfResult::Success;
    }

    void unsetEmissiveStrength() { m_emissiveStrength.reset(); }
    double getEmissiveStrengthOrDefault() const { return m_emissiveStrength.value_or(1); }

    bool isValid() const { return true; }
    GltfResult getInvalidProperties(core::Vector<core::string>&) const { return GltfResult::Success; }

private:
    std::optional<double> m_emissiveStrength;
};

}
