#pragma once

#include "core/Logger.h"
#include "filesystem/File.h"

namespace ux3d::logging {
class FileLogger : public core::LogCallback {
public:
    explicit FileLogger(filesystem::File _logFile);
    void log(core::LogLevel _level, const char* _message) override;

private:
    filesystem::File m_logFile;
};
}
