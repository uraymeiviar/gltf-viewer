// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/StaticVector.h"
#include <limits>

namespace ux3d::gltf {
class ProgressCallback {
private:
    float m_LastReport = -std::numeric_limits<float>::max();

public:
    float m_StepSize = 0.f;

    void report(float _Progress)
    {
        float step = _Progress - m_LastReport;
        if (step < m_StepSize && _Progress != 1.f) {
            return;
        }

        m_LastReport = _Progress;
        reportInternal(_Progress);
    }

protected:
    virtual void reportInternal(float _Progress) = 0;
};

class ProgressReporter : public ProgressCallback {
private:
    core::StaticVector<ProgressCallback*, 64u> m_Callbacks;

public:
    void registerCallback(ProgressCallback* _pCallback)
    {
        if (false == core::assertion(_pCallback != nullptr, "ProgressReporter: Callback cannot be null")) {
            return;
        }

        m_Callbacks.emplace_back(_pCallback);
    }

protected:
    void reportInternal(float _Progress) override
    {
        for (ProgressCallback* pCallback : m_Callbacks) {
            pCallback->report(_Progress);
        }
    }
};
}
