// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "filesystem/File.h"
#include "filesystem/FileCopy.h"
#include "testHelpers/TestFileSystem.h"
#include <catch2/catch.hpp>

namespace ux3d::filesystem::tests {
namespace {
    File testFile(const core::string& _path)
    {
        return TestFileSystem::instance().toFile(_path);
    }
}

SCENARIO("FileCopy", "[tools.filesystem.FileCopy]")
{
    GIVEN("a text file")
    {
        const File file = testFile("FileCopyTest.txt");
        const core::string content = "hallo MAN, you mad?";
        REQUIRE(file.save(content).derived == FileSaveResult::Success);

        WHEN("copied to valid location")
        {
            const File outFile = testFile("FileCopyTest.copy");
            REQUIRE(copyTextFile(file, outFile).derived == FileCopyResult::Success);

            THEN("output file exists")
            {
                core::string outContent;
                REQUIRE(outFile.load(outContent).derived == FileLoadResult::Success);

                AND_THEN("has correct content")
                {
                    CHECK(outContent == content);
                }
            }
        }

        WHEN("copied to own location")
        {
            FileCopyResult result = copyTextFile(file, file);

            THEN("fails")
            {
                CHECK(result.derived == FileCopyError::InputEqualsOutput);
            }
        }

        WHEN("copied to invalid location")
        {
            const File outFile = testFile("this/folder/doesnt/exist/FileCopyTest.copy");
            FileCopyResult result = copyTextFile(file, outFile);

            THEN("reports failure")
            {
                CHECK(result.derived != FileCopyResult::Success);
            }
            THEN("file can't be loaded")
            {
                core::string outContent;
                CHECK(outFile.load(outContent).derived != FileLoadResult::Success);
            }
        }
    }

    GIVEN("a binary file")
    {
        const File file = testFile("FileCopyTest.bin");
        const core::Vector<uint8_t> data = { 1u, 2u, 4u, 8u };
        REQUIRE(file.save(data).derived == FileSaveResult::Success);

        WHEN("copied to valid location")
        {
            const File outFile = testFile("FileCopyTest.copy");
            REQUIRE(copyBinaryFile(file, outFile).derived == FileCopyResult::Success);

            THEN("output file exists")
            {
                core::Vector<uint8_t> outData;
                REQUIRE(outFile.load(outData).derived == FileLoadResult::Success);

                AND_THEN("has correct content")
                {
                    REQUIRE(data.size() == outData.size());
                    for (size_t i = 0; i < data.size(); ++i) {
                        CHECK(data[i] == outData[i]);
                    }
                }
            }
        }

        WHEN("copied to own location")
        {
            FileCopyResult result = copyBinaryFile(file, file);

            THEN("fails")
            {
                CHECK(result.derived == FileCopyError::InputEqualsOutput);
            }
        }

        WHEN("copied to invalid location")
        {
            const File outFile = testFile("this/folder/doesnt/exist/FileCopyTest.copy");
            FileCopyResult result = copyBinaryFile(file, outFile);

            THEN("reports failure")
            {
                CHECK(result.derived != FileCopyResult::Success);
            }
            THEN("file can't be loaded")
            {
                core::Vector<uint8_t> outData;
                CHECK(outFile.load(outData).derived != FileLoadResult::Success);
            }
        }
    }
}
}
