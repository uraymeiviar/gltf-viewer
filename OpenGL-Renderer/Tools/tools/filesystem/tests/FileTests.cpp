// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "catch2/catch.hpp"
#include "core/String.h"
#include "filesystem/File.h"
#include "testHelpers/TestFileSystem.h"
#include <fstream>
#include <sstream>

namespace ux3d::tests {
namespace {
    filesystem::File testFile(const core::string& _path)
    {
        return filesystem::TestFileSystem::instance().toFile(_path);
    }
}

void createTestFile(const core::string& path, const core::string& content)
{
    std::ofstream file(path);
    file << content;
    file.close();
}

void loadTestFile(const core::string& path, core::string& content)
{
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
}

TEST_CASE("File load text", "[tools.filesystem.File]")
{
    filesystem::File file = testFile("test.txt");

    const core::string expectedOutput = "Hello sailor";
    createTestFile(file.toString(), expectedOutput);
    core::string output;

    CHECK(file.load(output));
    CHECK(output == expectedOutput);
}

TEST_CASE("File save text", "[tools.filesystem.File]")
{
    const core::string expectedOutput = "Hello sailor";
    filesystem::File file = testFile("test_write.txt");

    CHECK(file.save(expectedOutput));

    core::string output;
    loadTestFile(file.toString(), output);
    CHECK(output == expectedOutput);
}

TEST_CASE("File remove", "[tools.filesystem.File]")
{
    const core::string expectedOutput = "Hello sailor";
    filesystem::File file = testFile("test_remove.txt");
    CHECK(file.save(expectedOutput));

    CHECK(file.remove());
    CHECK(!file.exists());
}
}
