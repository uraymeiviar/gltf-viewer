// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#define CATCH_CONFIG_RUNNER // This tells Catch to provide a main() - only do this in one cpp file
#include "catch2/catch.hpp"
#include "core/Logger.h"
#include "logging/ConsoleLog.h"

int main(int _argumentCount, char* _arguments[])
{
#ifdef UX3D_TEST_LOGGING
    ux3d::logging::ConsoleLog logger;
    ux3d::core::addLoggerCallback(&logger);
#endif

    return Catch::Session().run(_argumentCount, _arguments);
}
