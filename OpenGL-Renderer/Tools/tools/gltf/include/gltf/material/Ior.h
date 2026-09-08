// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include <optional>

//See https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_ior/extensions/2.0/Khronos/KHR_materials_ior

namespace ux3d::gltf {
class Gltf;

class Ior : public GltfObject {
private:
    std::optional<double> m_ior;

public:
    GltfResult setIor(const double& _iorValue);
    bool hasIor() const { return m_ior.has_value(); }
    void unsetIor() { m_ior.reset(); }
    double getIorOrDefault() const { return m_ior.value_or(1.5); }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
