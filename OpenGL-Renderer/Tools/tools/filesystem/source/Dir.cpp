// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "filesystem/Dir.h"
#include "filesystem/FileSystem.h"

using namespace ux3d;
using namespace ux3d::filesystem;

core::Vector<Path> Dir::listContents() const
{
    return m_filesystem->listContents(*this);
}

core::Vector<File> Dir::listFiles() const
{
    return m_filesystem->listFiles(*this);
}

PathResult Dir::create() const
{
    return m_filesystem->create(*this);
}
