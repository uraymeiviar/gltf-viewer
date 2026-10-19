// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Extensions.h"
#include "core/String.h"
#include <unordered_map>

namespace ux3d::gltf {
class GltfObject {
public:
    virtual ~GltfObject() = default;

    virtual Extensions getUsedExtensions() const { return Extension::None; }

    bool addUnknownExtension(const core::string& _extensionName, const core::string& _extensionObject);
    void setUnknownExtension(const core::string& _extensionName, const core::string& _extensionObject) { m_unknownExtensions[_extensionName] = _extensionObject; }
    bool removeUnknownExtension(const core::string& _extensionName) { return m_unknownExtensions.erase(_extensionName) > 0; }
    const std::unordered_map<core::string, core::string>& getUnknownExtensions() const { return m_unknownExtensions; }
    bool removeUnknownExtensions();
    void clearExtras() { m_extras.clear(); }

    void setExtras(const core::string& _extras);
    void addExtras(const core::string& _extras);
    const core::string& getExtras() const { return m_extras; }

protected:
    std::unordered_map<core::string, core::string> m_unknownExtensions;
    core::string m_extras = "";
};
}
