// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfType.h"
#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"

#include <optional>

namespace ux3d::gltf {
class Gltf;

class ChildOfRoot : public GltfObject {
public:
    const uint32_t m_ID = 0u;

protected:
    std::optional<core::string> m_Name;

public:
    virtual ~ChildOfRoot() = default;

    virtual GltfType getGltfType() const = 0;

    bool hasName() const { return m_Name.has_value(); }
    core::string getName() const { return m_Name.value_or(""); }
    GltfResult setName(const core::string& _Name)
    {
        m_Name = _Name;
        return GltfResult::Success;
    }
    void unsetName() { m_Name.reset(); }

    virtual bool isValid() const = 0;

    virtual GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const = 0;

    ChildOfRoot(const ChildOfRoot&) = delete;
    ChildOfRoot(ChildOfRoot&&) = delete;

    void operator=(const ChildOfRoot&) = delete;
    void operator=(ChildOfRoot&&) = delete;

protected:
    ChildOfRoot(uint32_t _ID)
        : m_ID(_ID)
    {
    }
    ChildOfRoot(uint32_t _ID, const ChildOfRoot& _Source)
        : GltfObject(_Source)
        , m_ID(_ID)
        , m_Name(_Source.m_Name)
    {
    }
};
}
