// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "File.h"
#include "Path.h"

namespace ux3d::filesystem {
class Dir : public Path {
public:
    using Path::Path;

    Dir(const Path& other)
        : Path(other)
    {
    }

    core::Vector<Path> listContents() const;

    // listFiles returns all contained regular files, if path is a directory. If the path points to a file, the vector will be empty. listFiles will not recurse into subdirectories
    core::Vector<File> listFiles() const;
    PathResult create() const;
};
}
