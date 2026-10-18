// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "helpers/MemoryStreamFileSystem.h"
#include "core/Slice.h"
#include <QDir>

using namespace ux3d;
using namespace ux3d::filesystem;
using namespace ux3d::editor;

bool MemoryStreamFileSystem::isFile(const Path& path) const
{
    return false;
}

bool MemoryStreamFileSystem::isDirectory(const Path& path) const
{
    return false;
}

bool MemoryStreamFileSystem::isAbsolute(const Path& path) const
{
    return false;
}

bool MemoryStreamFileSystem::isRelative(const Path& path) const
{
    return true;
}

bool MemoryStreamFileSystem::isEmpty(const Path& path) const
{
    return m_outputData.empty();
}

bool MemoryStreamFileSystem::exists(const Path& path) const
{
    return false;
}

core::Vector<Path> MemoryStreamFileSystem::listContents(const Dir& _dir) const
{
    core::Vector<Path> contents;
    return contents;
}

Path MemoryStreamFileSystem::getParent(const Path& _path) const
{
    return toPath("");
}

core::Vector<File> MemoryStreamFileSystem::listFiles(const Dir& dir) const
{
    core::Vector<File> files;
    return files;
}

filesystem::PathResult MemoryStreamFileSystem::create(const filesystem::Dir& directory) const
{
    return PathResult::Success;
}

filesystem::PathResult ux3d::editor::MemoryStreamFileSystem::remove(const filesystem::Path& _path) const
{
    return filesystem::PathErrorE::PathIsNotADirectory;
}

Path MemoryStreamFileSystem::getChild(const Path& path, const core::string& _name) const
{
    return toPath("");
}

core::string MemoryStreamFileSystem::getName(const Path& _path) const
{
    return "memoryStream";
}

core::string MemoryStreamFileSystem::getStem(const filesystem::File& _file) const
{
    return "memoryStream";
}

core::string MemoryStreamFileSystem::getExtension(const filesystem::File& _file) const
{
    return "";
}

FileLoadResult MemoryStreamFileSystem::load(const File& _file, core::Vector<uint8_t>& _data) const
{
    return FileLoadResultE::AllocationFailed;
}

FileLoadResult MemoryStreamFileSystem::load(const File& _file, core::Slice<uint8_t>& _data) const
{
    return FileLoadResultE::AllocationFailed;
}

FileLoadResult MemoryStreamFileSystem::load(const File& _file, core::string& _text) const
{
    return FileLoadResultE::AllocationFailed;
}

FileSaveResult MemoryStreamFileSystem::save(const File& _file, const uint8_t* _data, const size_t _byteLength) const
{
    const size_t currentSize = m_outputData.size();
    if (m_outputData.resize(currentSize + static_cast<size_t>(_byteLength), nullptr)) {
        std::memcpy(m_outputData.data() + currentSize, _data, _byteLength);
    }
    return FileSaveResult::Success;
}

FileSaveResult MemoryStreamFileSystem::save(const File& _file, const core::Vector<uint8_t>& _data) const
{
    return this->save(_file, _data.data(), _data.size());
}

FileSaveResult MemoryStreamFileSystem::save(const File& _file, const core::Slice<uint8_t>& _data) const
{
    return this->save(_file, _data.data(), _data.size());
}

FileSaveResult MemoryStreamFileSystem::save(const File& _file, const core::string& _text) const
{
    return this->save(_file, reinterpret_cast<const uint8_t*>(_text.data()), _text.size());
}

filesystem::FileSaveResult MemoryStreamFileSystem::append(const File& _file, const core::string& _text) const
{
    return this->save(_file, reinterpret_cast<const uint8_t*>(_text.data()), _text.size());
}
