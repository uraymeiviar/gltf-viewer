#include "logging/FileLogger.h"
#include "testHelpers/TestFileSystem.h"
#include <catch2/catch.hpp>

using namespace ux3d;
using namespace ux3d::logging;

TEST_CASE("FileLogger: call log directly", "[tools.logging.FileLogger]")
{
    filesystem::File logFile(filesystem::TestFileSystem::instance(), "FileLogger_direct-calls.log");
    logFile.remove();

    FileLogger logger(logFile);

    core::string info("some info");
    core::string warning("some warning");
    core::string error("some error");

    logger.log(core::LogLevel::Info, info.c_str());
    logger.log(core::LogLevel::Warning, warning.c_str());
    logger.log(core::LogLevel::Error, error.c_str());

    core::string text;
    logFile.load(text);

    // all formatting is done by the core logger so this should just be it
    CHECK(text == info + warning + error);
}

TEST_CASE("FileLogger: call core::log", "[tools.logging.FileLogger]")
{
    filesystem::File logFile(filesystem::TestFileSystem::instance(), "FileLogger_core-log.log");
    logFile.remove();

    FileLogger logger(logFile);
    core::addLoggerCallback(&logger);

    core::string info("core info!");
    core::string warning("core warning!");
    core::string error("core error!");

    core::logInfo(info.c_str());
    core::logWarning(warning.c_str());
    core::logError(error.c_str());

    core::string text;
    logFile.load(text);

    // can't check for equality because logger adds timestamp and stuff
    CHECK(core::contains(text, info.c_str()));
    CHECK(core::contains(text, warning.c_str()));
    CHECK(core::contains(text, error.c_str()));
}
