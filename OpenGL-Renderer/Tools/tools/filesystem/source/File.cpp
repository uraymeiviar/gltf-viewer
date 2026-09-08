// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "filesystem/File.h"

#include "filesystem/FileSystem.h"

using namespace ux3d;
using namespace ux3d::filesystem;

core::string File::getStem() const
{
    return m_filesystem->getStem(*this);
}

core::string File::getExtension() const
{
    return m_filesystem->getExtension(*this);
}

FileLoadResult File::load(core::Vector<uint8_t>& _data) const
{
    return m_filesystem->load(*this, _data);
}

FileLoadResult File::load(core::Slice<uint8_t>& _data) const
{
    return m_filesystem->load(*this, _data);
}

FileLoadResult File::load(core::string& _text) const
{
    return m_filesystem->load(*this, _text);
}

FileSaveResult File::save(const uint8_t* _data, const size_t _byteLength) const
{
    return m_filesystem->save(*this, _data, _byteLength);
}

FileSaveResult File::save(const core::Vector<uint8_t>& _data) const
{
    return m_filesystem->save(*this, _data);
}

FileSaveResult File::save(const core::Slice<uint8_t>& _data) const
{
    return m_filesystem->save(*this, _data);
}

FileSaveResult File::save(const core::string& _text) const
{
    return m_filesystem->save(*this, _text);
}

FileSaveResult File::append(const core::string& _text) const
{
    return m_filesystem->append(*this, _text);
}
