// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "testHelpers/TestFileSystem.h"
#include "core/Platform.h"
#include "core/Slice.h"
#include <fstream>

#ifdef UX3D_IS_WINDOWS
#include "windows/dirent.h"
#include <algorithm>
#else
#include <dirent.h>
#endif

using namespace ux3d;
using namespace ux3d::filesystem;

TestFileSystem& TestFileSystem::instance()
{
    static TestFileSystem m_filesystem;
    return m_filesystem;
}

bool TestFileSystem::isFile(const Path& path) const
{
    std::ifstream file(path.toString().c_str());
    return file.good() && false == isDirectory(path);
}

bool TestFileSystem::isDirectory(const Path& path) const
{
    if (DIR* dir = opendir(path.toString().c_str()); dir != nullptr) {
        closedir(dir);
        return true;
    }
    return false;
}

bool TestFileSystem::isAbsolute(const Path& path) const
{
#ifdef UX3D_IS_WINDOWS
    return core::contains(path.toString(), ":");
#else
    return core::startsWith(path.toString(), "/");
#endif
}

bool TestFileSystem::isRelative(const Path& path) const
{
    return false == isAbsolute(path);
}

bool TestFileSystem::isEmpty(const Path& path) const
{
    return path.toString().empty();
}

bool TestFileSystem::exists(const Path& path) const
{
    return (isFile(path) || isDirectory(path));
}

core::Vector<Path> TestFileSystem::listContents(const Dir& dir) const
{
    core::Vector<Path> contents;

    if (DIR* directory = opendir(dir.toString().c_str()); directory != nullptr) {
        dirent* entry = readdir(directory);
        for (; entry != nullptr; entry = readdir(directory)) {
            (void)contents.emplace_back(*this, entry->d_name);
        }
    }

    return contents;
}

Path TestFileSystem::getParent(const Path& path) const
{
    core::string pathString = path.toString();
    size_t i = pathString.find_last_of('/');

#ifdef UX3D_IS_WINDOWS
    if (isAbsolute(path) && i == 2) {
        return toPath(pathString.substr(0, i) + "/");
    }
#else
    if (i == 0) {
        return toPath("/");
    }
#endif

    if (i == core::string::npos) {
        return toPath("");
    }

    return toPath(pathString.substr(0, i));
}

core::Vector<File> TestFileSystem::listFiles(const Dir& dir) const
{
    core::Vector<File> files;
    for (auto& entry : dir.listContents()) {
        if (entry.isFile()) {
            (void)files.push_back(entry);
        }
    }
    return files;
}

PathResult TestFileSystem::create(const Dir& directory) const
{
    if (isDirectory(directory)) {
        return PathResult::Success;
    }

#ifdef UX3D_IS_WINDOWS
    core::string path = directory.toString();
    std::replace(path.begin(), path.end(), '/', '\\');
    core::string command = "mkdir " + path;
#else
    core::string command = "mkdir -p " + directory.toString();
#endif

    system(command.c_str());

    return PathResult::Success;
}

filesystem::PathResult ux3d::filesystem::TestFileSystem::remove(const filesystem::Path& _path) const
{
    core::string command = "";

    if (_path.isDirectory()) {
        command = "rmdir " + _path.toString();
    } else if (_path.isFile()) {
#ifdef UX3D_IS_WINDOWS
        command = "del " + _path.toString();
#else
        command = "rm " + _path.toString();
#endif
    } else {
        return PathErrorE::NotFound;
    }

    if (system(command.c_str()) == 0) {
        return PathResult::Success;
    }

    return PathErrorE::SystemError;
}

Path TestFileSystem::getChild(const Path& path, const core::string& _name) const
{
    return Path(*this, path.toString() + "/" + _name);
}

core::string TestFileSystem::getName(const Path& _path) const
{
    core::string pathString = _path.toString();
    size_t i = pathString.find_last_of('/');

    if (i == core::string::npos) {
        return pathString;
    }

    return pathString.substr(i + 1);
}

core::string TestFileSystem::getStem(const File& _file) const
{
    core::string name = getName(_file);
    size_t i = name.find_last_of('.');

    if (i == core::string::npos) {
        return name;
    }

    return name.substr(0, i);
}

core::string TestFileSystem::getExtension(const File& _file) const
{
    core::string name = getName(_file);
    size_t i = name.find_last_of('.');

    if (i == core::string::npos) {
        return name;
    }

    if (name.size() == 1) {
        return name;
    }

    return name.substr(i + 1);
}

FileLoadResult TestFileSystem::load(const File& _file, core::Vector<uint8_t>& _data) const
{
    if (false == _file.isFile()) {
        return FileLoadResultE::FileNotFound;
    }
    return loadBinaryFile(_file.toString(), _data);
}

FileLoadResult TestFileSystem::load(const File& _file, core::Slice<uint8_t>& _data) const
{
    if (false == _file.isFile()) {
        return FileLoadResultE::FileNotFound;
    }
    return loadBinaryFile(_file.toString(), _data);
}

FileLoadResult TestFileSystem::load(const File& _file, core::string& _text) const
{
    if (false == _file.isFile()) {
        return FileLoadResultE::FileNotFound;
    }
    return loadTextFile(_file.toString(), _text);
}

FileSaveResult TestFileSystem::save(const File& _file, const uint8_t* _data, const size_t _byteLength) const
{
    return saveBinaryFile(_file.toString().c_str(), _data, _byteLength);
}

FileSaveResult TestFileSystem::save(const File& _file, const core::Vector<uint8_t>& _data) const
{
    return saveBinaryFile(_file.toString().c_str(), _data);
}

FileSaveResult TestFileSystem::save(const File& _file, const core::Slice<uint8_t>& _data) const
{
    return saveBinaryFile(_file.toString().c_str(), _data);
}

FileSaveResult TestFileSystem::save(const File& _file, const core::string& _text) const
{
    return saveTextFile(_file.toString().c_str(), _text);
}

FileSaveResult TestFileSystem::append(const File& _file, const core::string& _text) const
{
    return saveFile(_file.toString().c_str(), _text, std::ios_base::app);
}

FileLoadResult TestFileSystem::loadBinaryFile(const char* _pPath, core::Vector<uint8_t>& _OutBuffer) const
{
    return loadFile(_pPath, _OutBuffer, _OutBuffer.getAllocator(), std::ios::binary | std::ios_base::in);
}

FileLoadResult TestFileSystem::loadBinaryFile(const char* _pPath, core::Slice<uint8_t>& _OutBuffer) const
{
    return loadFile(_pPath, _OutBuffer, _OutBuffer.getAllocator(), std::ios::binary | std::ios_base::in);
}

FileLoadResult TestFileSystem::loadTextFile(const char* _pPath, core::string& _OutString) const
{
    return loadFile(_pPath, _OutString, std::ios_base::in);
}

FileLoadResult TestFileSystem::loadFile(const char* _pPath, core::string& _OutBuffer, std::ios_base::openmode mode) const
{
    std::ifstream stream(_pPath, mode | std::ios_base::binary);

    if (!stream.is_open()) {
        core::logError("FileLoader: Can not open file at %s", _pPath);
        return FileLoadResultE::CannotOpenFile;
    }
    auto begin = stream.tellg();
    stream.seekg(0, std::ios::end);
    auto end = stream.tellg() - begin;
    stream.seekg(0, std::ios::beg);

    using vt = typename core::string::value_type;

    // compute length in number of elements from bytes
    auto len = end / sizeof(vt) + (end % sizeof(vt) == 0 ? 0 : 1);

    _OutBuffer.resize(len);

    stream.read(reinterpret_cast<char*>(&_OutBuffer[0]), end);

    stream.close();

    return FileLoadResult::Success;
}

template <class Container>
FileLoadResult TestFileSystem::loadFile(const char* _pPath, Container& _OutBuffer, core::IAllocator* _Allocator, std::ios_base::openmode mode) const
{
    std::ifstream stream(_pPath, mode);

    if (!stream.is_open()) {
        core::logError("FileLoader: Can not open file at %s", _pPath);
        return FileLoadResultE::CannotOpenFile;
    }

    stream.seekg(0, std::ios::end);
    auto end = stream.tellg();
    stream.seekg(0, std::ios::beg);

    using vt = typename Container::value_type;

    // compute length in number of elements from bytes
    auto len = end / sizeof(vt) + (end % sizeof(vt) == 0 ? 0 : 1);

    if (_OutBuffer.resize(len, _Allocator)) {
        stream.read(reinterpret_cast<char*>(&_OutBuffer[0]), end);
    } else {
        return FileLoadResultE::AllocationFailed;
    }

    stream.close();

    return FileLoadResult::Success;
}

FileSaveResult TestFileSystem::saveBinaryFile(const char* _pPath, const uint8_t* _pData, const size_t _byteLength) const
{
    std::ofstream stream;
    stream.open(_pPath, std::ios::binary | std::ios_base::out);

    if (!stream.is_open()) {
        return FileSaveResultE::CannotOpenFile;
    }

    stream.write(reinterpret_cast<const char*>(_pData), _byteLength);

    stream.close();

    return FileSaveResult::Success;
}

FileSaveResult TestFileSystem::saveBinaryFile(const char* _pPath, const core::Vector<uint8_t>& _OutBuffer) const
{
    return saveFile(_pPath, _OutBuffer, std::ios::binary | std::ios_base::out);
}

FileSaveResult TestFileSystem::saveBinaryFile(const char* _pPath, const core::Slice<uint8_t>& _OutBuffer) const
{
    return saveFile(_pPath, _OutBuffer, std::ios::binary | std::ios_base::out);
}

FileSaveResult TestFileSystem::saveTextFile(const char* _pPath, const core::string& _OutString) const
{
    return saveFile(_pPath, _OutString);
}

template <class Container>
FileSaveResult TestFileSystem::saveFile(const char* _pPath, const Container& _OutBuffer, std::ios_base::openmode mode) const
{
    std::ofstream stream(_pPath, mode);

    if (!stream.is_open()) {
        return FileSaveResultE::CannotOpenFile;
    }

    stream.write(reinterpret_cast<const char*>(&_OutBuffer[0]), _OutBuffer.size() * sizeof(_OutBuffer[0]));

    stream.close();

    return FileSaveResult::Success;
}
