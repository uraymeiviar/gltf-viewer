// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>
#include <utility>

namespace ux3d::core {
enum class LogLevel : uint32_t {
    All,
    Info,
    Warning,
    Error,
    Fatal
};

class LogCallback {
public:
    LogCallback() = default;
    virtual ~LogCallback();
    explicit LogCallback(const LogLevel _Level)
        : m_LogLevel(_Level) {};
    virtual void log(LogLevel _Level, const char* _pMessage) = 0;

protected:
    const LogLevel m_LogLevel = LogLevel::All;
};

void log(LogLevel _Level, const char* _pFormat, ...);

template <class... Args>
inline void logInfo(const char* _pFormat, Args&&... _Args)
{
    log(LogLevel::Info, _pFormat, std::forward<Args>(_Args)...);
}

template <class... Args>
inline void logError(const char* _pFormat, Args&&... _Args)
{
    log(LogLevel::Error, _pFormat, std::forward<Args>(_Args)...);
}

template <class... Args>
inline void logWarning(const char* _pFormat, Args&&... _Args)
{
    log(LogLevel::Warning, _pFormat, std::forward<Args>(_Args)...);
}

//template <class ... Args>
//inline void logFatal(const char* _pFormat, Args&& ..._Args)
//{
//	log(LogLevel::Fatal, _pFormat, std::forward<Args>(_Args)...);
//}

template <class... Args>
inline void debugLog(const LogLevel _Level, const char* _pFormat, Args&&... _Args)
{
#ifndef NDEBUG
    log(_Level, _pFormat, std::forward<Args>(_Args)...);
#else
    (void)_Level;
    (void)_pFormat;
    (void)sizeof...(_Args);
#endif
}

bool assertion(bool _Condition, const char* _pFormat, ...);

template <class... Args>
inline bool debugAssert(const bool _Condition, const char* _pFormat, Args&&... _Args)
{
#ifndef NDEBUG
    return assertion(_Condition, _pFormat, std::forward<Args>(_Args)...);
#else
    (void)_pFormat;
    (void)sizeof...(_Args);
    return _Condition;
#endif
}

void addLoggerCallback(LogCallback* _pCallback);
void removeLoggerCallback(LogCallback* _pCallback);
}
