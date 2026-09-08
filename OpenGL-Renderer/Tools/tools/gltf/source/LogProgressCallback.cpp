// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/LogProgressCallback.h"
#include "core/Logger.h"

void ux3d::gltf::LogProgressCallback::reportInternal(float _Progress)
{
    uint32_t percent = static_cast<uint32_t>(_Progress * 100.f);
    core::logInfo("%s: %d%%", m_Name.c_str(), percent);
}
