// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "core/String.h"
#include "testHelpers/TestFileSystem.h"
#include <fstream>

namespace ux3d::filesystem::tests {
void createTestFile(core::string& path, core::string& content)
{
    std::ofstream file(path);
    file << content;
    file.close();
}

TEST_CASE("load text file", "[tools.filesystem.FileLoader]")
{
    auto fs = TestFileSystem::instance();

    SECTION("valid file")
    {
        core::string path = "test.txt";
        File file(fs, path);
        core::string expectedOutput = "Hello sailor";
        createTestFile(path, expectedOutput);
        core::string output;
        CHECK(file.load(output).derived == FileLoadResult::Success);
        CHECK(output == expectedOutput);
    }

    SECTION("path does not exist")
    {
        core::string path = "not.a.file";
        File file(fs, path);
        core::string output;
        CHECK(file.load(output).derived != FileLoadResult::Success);
        CHECK(output.empty());
    }

    SECTION("path points to a directory")
    {
        Path directory(fs, UX3D_TEST_RESOURCES "/subdir");
        REQUIRE(directory.exists());
        REQUIRE_FALSE(directory.isFile());
        REQUIRE(directory.isDirectory());

        File file(directory);
        core::string output;
        CHECK(file.load(output).derived == FileLoadResultE::FileNotFound);
    }
}
}
