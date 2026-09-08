// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "filesystem/FileSystemResults.h"
#include "filesystem/Path.h"

namespace ux3d::core {
template <typename T>
class Slice;
}

namespace ux3d::filesystem {
// Default implementation of a file, using std::filesystem
// When specifying a custom derived class, it must not add fields, as otherwise slicing
// will occur.

class File : public Path {
public:
    using Path::Path;

    File(const Path& other)
        : Path(other)
    {
    }

    core::string getStem() const;
    core::string getExtension() const;

    FileLoadResult load(core::Vector<uint8_t>& data) const;
    FileLoadResult load(core::Slice<uint8_t>& data) const;
    FileLoadResult load(core::string& text) const;

    FileSaveResult save(const uint8_t* data, size_t byteLength) const;
    FileSaveResult save(const core::Vector<uint8_t>& data) const;
    FileSaveResult save(const core::Slice<uint8_t>& data) const;
    FileSaveResult save(const core::string& text) const;

    FileSaveResult append(const core::string& _text) const;
};
}
