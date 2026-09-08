// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include "animation/IAnimationTarget.h"

template <class T, typename F>
class AnimationTarget : public ux3d::animation::IAnimationTarget<T> {
public:
    AnimationTarget(F& lambda)
        : m_lambda(lambda)
    {
    }
    virtual ~AnimationTarget()
    {
    }

    virtual void update(const T& value) final
    {
        m_lambda(value);
    }

    F& m_lambda;
};
