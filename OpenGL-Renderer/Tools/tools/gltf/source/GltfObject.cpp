#include "gltf/GltfObject.h"

using namespace ux3d;
using namespace ux3d::gltf;

void GltfObject::setExtras(const core::string& _extras)
{
    m_extras = _extras;
}

void GltfObject::addExtras(const core::string& _extras)
{
    if (!_extras.empty()) {
        if (m_extras.empty()) {
            m_extras = _extras;
        } else {
            m_extras += ",\n" + _extras;
        }
    }
}

bool GltfObject::addUnknownExtension(const core::string& _extensionName, const core::string& _extensionObject)
{
    return m_unknownExtensions.insert({ _extensionName, _extensionObject }).second;
}

bool GltfObject::removeUnknownExtensions()
{
    if (m_unknownExtensions.size() == 0) {
        return false;
    }
    if (auto it = m_unknownExtensions.find("ux3d_gestaltor"); it != m_unknownExtensions.end()) {
        if (m_unknownExtensions.size() == 1) {
            return false;
        }
        auto gestaltorExtension = *it;
        m_unknownExtensions.clear();
        m_unknownExtensions.insert(gestaltorExtension);
        return true;
    }
    m_unknownExtensions.clear();
    return true;
}
