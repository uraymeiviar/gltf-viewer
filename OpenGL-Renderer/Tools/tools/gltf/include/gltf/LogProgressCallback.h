// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ProgressReporter.h"
#include "core/String.h"

namespace ux3d::gltf {
class LogProgressCallback : public ProgressCallback {
private:
    core::string m_Name;

public:
    explicit LogProgressCallback(const core::string& _Name)
        : m_Name(_Name)
    {
    }
    void reportInternal(float _Progress) override;
};
}
