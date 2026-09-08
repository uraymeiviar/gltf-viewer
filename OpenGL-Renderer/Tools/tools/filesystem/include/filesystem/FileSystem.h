// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "core/String.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "filesystem/FileSystemResults.h"
#include "filesystem/Path.h"

namespace ux3d::core {
template <typename T>
class Slice;
}

namespace ux3d::filesystem {
class IFileSystem {
    friend class Path;
    friend class File;
    friend class Dir;

public:
    virtual ~IFileSystem() {};

    virtual Path toPath(const core::string& path) const = 0;
    virtual Path fromURI(const core::string& uri) const = 0;
    virtual File toFile(const core::string& path) const = 0;
    virtual Path getTempPath() const = 0;
    virtual Path getAppDataPath() const = 0;
    virtual Path getExecutableDirPath() const = 0;

    // macOS sandbox specific. Implementations must define these using
    // the macOS sandbox API
    static bool sandboxIsExempt(const filesystem::Path& file);
    static bool sandboxHasBookmark(const filesystem::File& file);
    static void sandboxSaveBookmark(const filesystem::File& file);
    static bool sandboxRequestAccess(const filesystem::Path& path);

protected:
    virtual bool isFile(const Path& path) const = 0;
    virtual bool isDirectory(const Path& path) const = 0;
    virtual bool isAbsolute(const Path& path) const = 0;
    virtual bool isRelative(const Path& path) const = 0;
    virtual bool isEmpty(const Path& path) const = 0;
    virtual bool exists(const Path& path) const = 0;

    virtual core::Vector<Path> listContents(const Dir& dir) const = 0;
    virtual Path getParent(const Path& path) const = 0;
    virtual Path getChild(const Path& path, const core::string& name) const = 0;

    // listFiles returns all contained regular files, if path is a directory. If the path points to a file, the vector will be empty. listFiles will not recurse into subdirectories
    virtual core::Vector<File> listFiles(const Dir& path) const = 0;

    // create the full path to the directory, returns true if the path already exists
    virtual PathResult create(const Dir& directory) const = 0;

    virtual PathResult remove(const Path& path) const = 0;

    virtual core::string getName(const Path& path) const = 0;
    virtual core::string getStem(const File& file) const = 0;
    virtual core::string getExtension(const File& file) const = 0;

    virtual FileLoadResult load(const File& file, core::Vector<uint8_t>& data) const = 0;
    virtual FileLoadResult load(const File& file, core::Slice<uint8_t>& data) const = 0;
    virtual FileLoadResult load(const File& file, core::string& text) const = 0;

    virtual FileSaveResult save(const File& file, const uint8_t* data, const size_t byteLength) const = 0;
    virtual FileSaveResult save(const File& file, const core::Vector<uint8_t>& data) const = 0;
    virtual FileSaveResult save(const File& file, const core::Slice<uint8_t>& data) const = 0;
    virtual FileSaveResult save(const File& file, const core::string& text) const = 0;

    virtual FileSaveResult append(const File& _file, const core::string& _text) const = 0;

    // macOS sandbox specific
    static bool sandboxGetAccessWithBookmark(const filesystem::File& file);
    static void sandboxReleaseAccessWithBookmark(const filesystem::File& file);
};
}
