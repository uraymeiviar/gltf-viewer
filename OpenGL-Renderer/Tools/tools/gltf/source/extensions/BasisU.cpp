#include "gltf/extensions/BasisU.h"

#include "gltf/Image.h"

using namespace ux3d;
using namespace ux3d::gltf;

Image* ux3d::gltf::BasisU::getSource() const
{
    return m_pSource;
}

void ux3d::gltf::BasisU::setSource(Image* _pSource)
{
    m_pSource = _pSource;
}
