// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Logger.h"

namespace ux3d {
namespace logging {
    class ConsoleLog : public core::LogCallback {
    public:
        ConsoleLog() = default;
        explicit ConsoleLog(const core::LogLevel _Level)
            : LogCallback(_Level)
        {
        }
        void log(const core::LogLevel _Level, const char* _pMessage) final;
    };
}
}
