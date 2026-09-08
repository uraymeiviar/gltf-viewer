#pragma once

#include <optional>
#include <utility>

#include "PathResult.h"
#include "core/String.h"
#include "core/Vector.h"

namespace ux3d::filesystem {
class IFileSystem;
class Path {
public:
    // ⚠️ don't use the default constructor ⚠️
    // it's only a workaround for a MSVC bug
    // https://developercommunity.visualstudio.com/content/problem/60897/c-shared-state-futuresstate-default-constructs-the.html
    Path()
        : m_filesystem(nullptr)
    {
    }
    Path(const IFileSystem& fileSystem)
        : m_filesystem(&fileSystem)
    {
    }
    Path(const IFileSystem& fileSystem, const char* path)
        : m_filesystem(&fileSystem)
        , m_path(path)
    {
    }
    Path(const IFileSystem& fileSystem, core::string path)
        : m_filesystem(&fileSystem)
        , m_path(std::move(path))
    {
    }

    const IFileSystem& getFilesystem() const { return *(this->m_filesystem); }

    core::string toString() const;
    core::string getName() const;

    bool isFile() const;
    bool isDirectory() const;
    bool isAbsolute() const;
    bool isRelative() const;
    bool exists() const;

    Path getParent() const;

    PathResult remove() const;

    // operator/ returns a new path that is the OS equivalent to POSIX `Path.m_path + "/" + name`
    Path operator/(const core::string& name) const;

    bool operator==(const Path& other) const { return this->m_path == other.m_path; }
    bool operator!=(const Path& other) const { return this->m_path != other.m_path; }

protected:
    const IFileSystem* m_filesystem = nullptr;
    core::string m_path = "";
};
}
