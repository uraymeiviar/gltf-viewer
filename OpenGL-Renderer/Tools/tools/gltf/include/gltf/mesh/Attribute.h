// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "gltf/GltfResult.h"

namespace ux3d::gltf {
class Accessor;

class Attribute {
private:
    core::string m_Name;
    Accessor* m_Accessor = nullptr;

public:
    Attribute(Attribute&&) = delete;

    explicit Attribute() { }
    Attribute(const Attribute&) = default;

    GltfResult setAccessor(Accessor* _Accessor);
    GltfResult setName(core::string _Name);
    core::string getName() const { return m_Name; }
    Accessor* getAccessor() const { return m_Accessor; }

    void unsetAccessor() { m_Accessor = nullptr; }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;
};
}
