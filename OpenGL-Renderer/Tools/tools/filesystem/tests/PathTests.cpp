// Copyright © 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "testHelpers/TestFileSystem.h"

// UX3D_IS_WINDOWS comes from here, please leave it
#include "core/Platform.h"

namespace ux3d::filesystem::tests {
namespace {
    constexpr auto TestNamespace = "[filesystem.Path]";
    const auto& fs = TestFileSystem::instance();
}

#ifdef UX3D_IS_WINDOWS
const filesystem::Path AbsolutePath(fs, "C:/home/satan/deathlist.txt");
const filesystem::Path AbsolutePathParent(fs, "C:/home/satan");

const filesystem::Path AbsolutePathDirectlyUnderRoot(fs, "D:/Qt");
const filesystem::Path AbsolutePathDirectlyUnderRootParent(fs, "D:/");
#else
const filesystem::Path AbsolutePath(fs, "/home/satan/deathlist.txt");
const filesystem::Path AbsolutePathParent(fs, "/home/satan");

const filesystem::Path AbsolutePathDirectlyUnderRoot(fs, "/Qt");
const filesystem::Path AbsolutePathDirectlyUnderRootParent(fs, "/");
#endif
const core::string AbsolutePathFileNameWithExtension = "deathlist.txt";
const core::string AbsolutePathFileNameWithoutExtension = "deathlist";
const core::string AbsolutePathFileNameExtension = ".txt";

const filesystem::Path RelativePath(fs, "hello/sailor.png");
const filesystem::Path RelativePathParent(fs, "hello");
const core::string RelativePathFileNameWithExtension = "sailor.png";
const core::string RelativePathFileNameWithoutExtension = "sailor";
const core::string RelativePathFileNameExtension = ".png";

const filesystem::Path RelativePathWithoutParent(fs, "oopsie");
const filesystem::Path SelfPath(fs, ".");
const filesystem::Path ParentPath(fs, "..");

const filesystem::Path RelativePathToCurrent(fs, "./kreator/extreme aggression.mp3");
const filesystem::Path RelativePathToCurrentParent(fs, "./kreator");
const core::string RelativePathToCurrentFileNameWithExtension = "extreme aggression.mp3";
const core::string RelativePathToCurrentFileNameWithoutExtension = "extreme aggression";
const core::string RelativePathToCurrentFileNameExtension = ".mp3";

const filesystem::Path RelativePathThroughParent(fs, "../this-is-me");
const filesystem::Path RelativePathThroughParentParent(fs, "..");
const core::string RelativePathThroughParentFileNameWithExtension = "this-is-me";
const core::string RelativePathThroughParentFileNameWithoutExtension = "this-is-me";
const core::string RelativePathThroughParentFileNameExtension = "";

SCENARIO("Path: isAbsolutePath", TestNamespace)
{

    CHECK(AbsolutePath.isAbsolute());
    CHECK_FALSE(RelativePath.isAbsolute());
    CHECK_FALSE(RelativePathToCurrent.isAbsolute());
    CHECK_FALSE(RelativePathThroughParent.isAbsolute());
}
SCENARIO("Path: isRelativePath", TestNamespace)
{
    CHECK_FALSE(AbsolutePath.isRelative());
    CHECK((RelativePath).isRelative());
    CHECK((RelativePathToCurrent).isRelative());
    CHECK((RelativePathThroughParent).isRelative());
}
SCENARIO("Path: getParent", TestNamespace)
{
    CHECK((AbsolutePath).getParent() == AbsolutePathParent);
    CHECK((AbsolutePathDirectlyUnderRoot).getParent() == AbsolutePathDirectlyUnderRootParent);
    CHECK((RelativePath).getParent() == RelativePathParent);
    CHECK((RelativePathWithoutParent).getParent().toString() == ".");
    CHECK((RelativePathToCurrent).getParent() == RelativePathToCurrentParent);
    CHECK((RelativePathThroughParent).getParent() == RelativePathThroughParentParent);
}
SCENARIO("Path: getFileName (with extension)", TestNamespace)
{
    CHECK(filesystem::File(AbsolutePath).getName() == AbsolutePathFileNameWithExtension);
    CHECK(filesystem::File(RelativePath).getName() == RelativePathFileNameWithExtension);
    CHECK(filesystem::File(RelativePathToCurrent).getName() == RelativePathToCurrentFileNameWithExtension);
    CHECK(filesystem::File(RelativePathThroughParent).getName() == RelativePathThroughParentFileNameWithExtension);
}
}
