// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "gltf/extensions/TextureDds.h"

#include "gltf/Image.h"

using namespace ux3d;
using namespace ux3d::gltf;

Image* ux3d::gltf::TextureDds::getSource() const
{
    return m_pSource;
}

void ux3d::gltf::TextureDds::setSource(Image* _pSource)
{
    m_pSource = _pSource;
}
