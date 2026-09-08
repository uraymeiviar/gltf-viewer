// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/Logger.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <mutex>

#include "core/Algorithms.h"
#include "core/DebugBreak.h"
#include "core/StaticVector.h"

ux3d::core::LogCallback::~LogCallback()
{
    removeLoggerCallback(this);
}

namespace ux3d {
namespace core {
    class Logger {
        Logger();

    public:
        Logger(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger& operator=(Logger&&) = delete;

        ~Logger();

        static Logger& GetInstance()
        {
            static Logger logInstance;
            return logInstance;
        }

        void log(LogLevel _Level, const char* _pFormat, va_list _Args);

        void AddCallback(LogCallback* _pCallback);
        void RemoveCallback(LogCallback* _pCallback);

    private:
        StaticVector<LogCallback*, 64u> m_Callbacks;

        std::mutex m_Mutex;
    };
}

} // !ux3d

ux3d::core::Logger::Logger()
    : m_Callbacks()
    , m_Mutex()
{
}

ux3d::core::Logger::~Logger()
{
    std::lock_guard lock(m_Mutex);
    m_Callbacks.clear();
}

void ux3d::core::Logger::log(const LogLevel _Level, const char* _pFormat, va_list _Args)
{
#ifdef UX3D_NO_LOG
    return;
#endif

    const char* pLevel = "";

    switch (_Level) {
    case LogLevel::Warning:
        pLevel = " Warning";
        break;
    case LogLevel::Error:
        pLevel = " Error";
        break;
    case LogLevel::Fatal:
        pLevel = " Fatal";
        break;
    default:
        break;
    }

    constexpr int BUFFERSIZE = 1024;

    char buffer[BUFFERSIZE];

    time_t rawtime;
    time(&rawtime);
    struct tm timeinfo {
    };

    size_t preamble = 0;

#ifdef UX3D_IS_WINDOWS
    localtime_s(&timeinfo, &rawtime);
#else
    timeinfo = *localtime(&rawtime);
#endif

    preamble = strftime(buffer, BUFFERSIZE, "%H:%M:%S", &timeinfo);

    preamble += snprintf(&buffer[preamble], BUFFERSIZE - preamble, "%s | ", pLevel);

    size_t end = vsnprintf(&buffer[preamble], BUFFERSIZE - preamble - 2u, _pFormat, _Args);

    end = std::min(end, BUFFERSIZE - preamble - 2u);

    // inject newline at the end
    if (end != 0) {
        buffer[preamble + end] = '\n';
        buffer[preamble + end + 1] = '\0';
    }

    // synchronize with mutex before writing to a shared resource
    std::lock_guard lock(m_Mutex);

    for (LogCallback* pCallback : m_Callbacks) {
        pCallback->log(_Level, buffer);
    }
}

void ux3d::core::Logger::AddCallback(LogCallback* _pCallback)
{
    std::lock_guard lock(m_Mutex);

    if (_pCallback != nullptr && m_Callbacks.full() == false) {
        m_Callbacks.push_back(_pCallback);
    }
}

void ux3d::core::Logger::RemoveCallback(LogCallback* _pCallback)
{
    if (_pCallback != nullptr) {
        std::lock_guard lock(m_Mutex);

        auto it = find(m_Callbacks, _pCallback);
        if (it != m_Callbacks.end()) {
            m_Callbacks.erase(it);
        }
    }
}

void ux3d::core::log(const LogLevel _Level, const char* _pFormat, ...)
{
#ifdef UX3D_NO_LOG
    return;
#endif

    va_list args;
    va_start(args, _pFormat);

    Logger::GetInstance().log(_Level, _pFormat, args);

    va_end(args);
}

bool ux3d::core::assertion(const bool _Condition, const char* _pFormat, ...)
{
#ifdef UX3D_NO_ASSERT
    return _Condition;
#endif

    if (_Condition) {
        return _Condition;
    }

    va_list args;
    va_start(args, _pFormat);

    Logger::GetInstance().log(LogLevel::Fatal, _pFormat, args);

    va_end(args);

    debugBreak();

    return _Condition;
}

void ux3d::core::addLoggerCallback(LogCallback* _pCallback)
{
    Logger::GetInstance().AddCallback(_pCallback);
}

void ux3d::core::removeLoggerCallback(LogCallback* _pCallback)
{
    Logger::GetInstance().RemoveCallback(_pCallback);
}
