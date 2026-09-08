#include "logging/FileLogger.h"
#include <utility>

using namespace ux3d;
using namespace ux3d::logging;

FileLogger::FileLogger(filesystem::File _logFile)
    : m_logFile(std::move(_logFile))
{
}

void FileLogger::log(core::LogLevel _level, const char* _message)
{
    m_logFile.append(_message);
}
