// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "logging/VSConsoleLog.h"
#include "core/Platform.h"

#ifdef UX3D_IS_MSVC
#include <debugapi.h> // for OutputDebugStringA
#endif

using namespace ux3d::logging;

void VSConsoleLog::log(const ux3d::core::LogLevel _Level, const char* _pMessage)
{
    if (_Level < m_LogLevel) {
        return;
    }

#ifdef UX3D_IS_MSVC
    OutputDebugStringA(_pMessage);
#else
    (void)_pMessage;
#endif
}
