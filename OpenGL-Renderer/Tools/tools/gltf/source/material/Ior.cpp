// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/material/Ior.h"
#include "math/VectorFunctions.h"

using namespace ux3d;
using namespace ux3d::gltf;

// The index of refraction (IOR) is a measured physical number usually in
// the range between 1 and 2 that determines how much the path of light is
// bent, or refracted, when entering a material. It also influences the
// ratio between reflected and transmitted light, calculated from the
// Fresnel equations. When converting from PBRSpeculaGlossiness then ior can also be 0.
GltfResult Ior::setIor(const double& _iorValue)
{
    static constexpr double epsilon = 0.00001;

    if (_iorValue < 1.0 && !(math::epsilonEqual<double>(0, _iorValue, epsilon))) {
        return GltfError::InvalidInput;
        core::logInfo("gltf %f", _iorValue);
    }
    m_ior = _iorValue;
    return GltfResult::Success;
}

bool Ior::isValid() const
{
    return true;
}

GltfResult Ior::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
