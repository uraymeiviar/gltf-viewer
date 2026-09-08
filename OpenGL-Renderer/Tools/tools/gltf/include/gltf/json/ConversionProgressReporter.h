// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gltf/ProgressReporter.h"

namespace ux3d::gltf {
class ConversionProgressReporter : public ProgressReporter {
private:
    uint32_t m_ConvertedElements = 0u;

public:
    uint32_t m_TotalElements = 1u;

    void onConvertedAnElement()
    {
        m_ConvertedElements += 1;
        float progress = static_cast<float>(m_ConvertedElements) / m_TotalElements;
        report(progress);
    }
};
}
