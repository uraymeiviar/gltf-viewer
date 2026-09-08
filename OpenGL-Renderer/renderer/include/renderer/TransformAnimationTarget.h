// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include <vector>

#include "animation/IAnimationTarget.h"

namespace ux3d::renderer {

// Implement the animation target interface so that animation channels can point to
// members of the transform hierarchy node
template <class T>
class TransformAnimationTarget final : public animation::IAnimationTarget<T> {
public:
    void update(const T& value)
    {
        if constexpr (std::is_same_v<T, std::vector<float>>) {
            m_dirty = true;
        } else {
            m_dirty |= value != m_value;
        }
        m_value = value;
    }

    bool isDirty() const
    {
        return m_dirty;
    }

    void clean()
    {
        m_dirty = false;
    }

    const T& getValue() const
    {
        return m_value;
    }

private:
    T m_value;
    bool m_dirty = true;
};

}
