#include "filesystem/Path.h"
#include "filesystem/FileSystem.h"

using namespace ux3d;
using namespace ux3d::filesystem;

bool Path::isFile() const
{
    return m_filesystem->isFile(*this);
}

bool Path::isDirectory() const
{
    return m_filesystem->isDirectory(*this);
}

bool Path::isAbsolute() const
{
    return m_filesystem->isAbsolute(*this);
}

bool Path::isRelative() const
{
    return m_filesystem->isRelative(*this);
}

bool Path::exists() const
{
    return m_filesystem->exists(*this);
}

Path Path::getParent() const
{
    return m_filesystem->getParent(*this);
}

Path Path::operator/(const core::string& _name) const
{
    return m_filesystem->getChild(*this, _name);
}

core::string Path::getName() const
{
    return m_filesystem->getName(*this);
}

core::string Path::toString() const
{
    if (m_path.empty()) {
        return ".";
    }

    return m_path;
}

PathResult Path::remove() const
{
    return m_filesystem->remove(*this);
}
