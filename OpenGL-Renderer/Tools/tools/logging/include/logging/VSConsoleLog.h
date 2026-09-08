// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Logger.h"

namespace ux3d {
namespace logging {
    class VSConsoleLog : public core::LogCallback {
    public:
        VSConsoleLog() = default;
        explicit VSConsoleLog(const core::LogLevel _Level)
            : LogCallback(_Level)
        {
        }

        void log(const core::LogLevel _Level, const char* _pMessage) final;
    };
}
}
