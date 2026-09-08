// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "logging/ConsoleLog.h"

#include "core/Platform.h"

#include <stdio.h>

#ifdef UX3D_IS_ANDROID
#include <android/log.h>
#endif

using namespace ux3d::logging;

void ConsoleLog::log(const ux3d::core::LogLevel _Level, const char* _pMessage)
{
    if (_Level < m_LogLevel) {
        return;
    }

#ifdef UX3D_IS_ANDROID
    __android_log_print(ANDROID_LOG_INFO, "UX3D", "%s", _pMessage);
#else
    printf("%s", _pMessage);
#endif
}
