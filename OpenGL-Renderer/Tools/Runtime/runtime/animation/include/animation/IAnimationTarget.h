// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

namespace ux3d::animation {
template <class T>
class IAnimationTarget {
public:
    virtual ~IAnimationTarget() = default;
    virtual void update(const T& value) = 0;
};
}
