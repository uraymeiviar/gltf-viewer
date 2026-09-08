// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/MaterialVariant.h"

using namespace ux3d;
using namespace ux3d::gltf;

MaterialVariant::MaterialVariant(uint32_t _id)
    : ChildOfRoot(_id)
{
}

MaterialVariant::MaterialVariant(uint32_t _id, const MaterialVariant& _source)
    : ChildOfRoot(_id, _source)
{
}

bool MaterialVariant::isValid() const
{
    return true;
}

GltfResult MaterialVariant::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    return GltfResult::Success;
}
