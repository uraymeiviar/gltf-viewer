// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/StdFileSystem.h"
#include "core/Slice.h"
#include "core/Vector.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace ux3d::filesystem;

namespace ux3d::imgui {

StdFileSystem& StdFileSystem::instance()
{
    static StdFileSystem s_instance;
    return s_instance;
}

static std::string toGeneric(const std::string& path)
{
    std::string result = path;
    for (char& c : result) {
        if (c == '\\') {
            c = '/';
        }
    }
    return result;
}

filesystem::Path StdFileSystem::fromURI(const core::string& uri) const
{
    std::string s = uri.c_str();
    const std::string prefix = "file:///";
    if (s.rfind(prefix, 0) == 0) {
        s = s.substr(prefix.length());
    } else {
        const std::string prefix2 = "file://";
        if (s.rfind(prefix2, 0) == 0) {
            s = s.substr(prefix2.length());
        }
    }
    return Path(*this, toGeneric(s));
}

filesystem::Path StdFileSystem::getTempPath() const
{
    std::error_code ec;
    fs::path p = fs::temp_directory_path(ec);
    return toPath(toGeneric(p.string()));
}

filesystem::Path StdFileSystem::getAppDataPath() const
{
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        return toPath(toGeneric(std::string(appData)));
    }
#endif
    return getTempPath();
}

filesystem::Path StdFileSystem::getExecutableDirPath() const
{
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len > 0) {
        fs::path p(buffer);
        return toPath(toGeneric(p.parent_path().string()));
    }
#endif
    std::error_code ec;
    return toPath(toGeneric(fs::current_path(ec).string()));
}

bool StdFileSystem::isFile(const filesystem::Path& path) const
{
    std::error_code ec;
    return fs::is_regular_file(fs::u8path(path.toString().c_str()), ec);
}

bool StdFileSystem::isDirectory(const filesystem::Path& path) const
{
    std::error_code ec;
    return fs::is_directory(fs::u8path(path.toString().c_str()), ec);
}

bool StdFileSystem::isAbsolute(const filesystem::Path& path) const
{
    std::error_code ec;
    return fs::u8path(path.toString().c_str()).is_absolute();
}

bool StdFileSystem::isRelative(const filesystem::Path& path) const
{
    std::error_code ec;
    return fs::u8path(path.toString().c_str()).is_relative();
}

bool StdFileSystem::isEmpty(const filesystem::Path& path) const
{
    return path.toString().empty();
}

bool StdFileSystem::exists(const filesystem::Path& path) const
{
    std::error_code ec;
    return fs::exists(fs::u8path(path.toString().c_str()), ec);
}

core::Vector<filesystem::Path> StdFileSystem::listContents(const filesystem::Dir& dir) const
{
    core::Vector<filesystem::Path> contents;
    std::error_code ec;
    fs::path dirPath = fs::u8path(dir.toString().c_str());

    if (!fs::is_directory(dirPath, ec)) {
        return contents;
    }

    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        contents.emplace_back(*this, toGeneric(entry.path().string()));
    }

    return contents;
}

filesystem::Path StdFileSystem::getParent(const filesystem::Path& path) const
{
    fs::path p = fs::u8path(path.toString().c_str());
    return toPath(toGeneric(p.parent_path().string()));
}

filesystem::Path StdFileSystem::getChild(const filesystem::Path& path, const core::string& name) const
{
    fs::path p = fs::u8path(path.toString().c_str());
    p /= fs::u8path(name.c_str());
    return toPath(toGeneric(p.string()));
}

core::Vector<filesystem::File> StdFileSystem::listFiles(const filesystem::Dir& path) const
{
    core::Vector<filesystem::File> files;
    std::error_code ec;
    fs::path dirPath = fs::u8path(path.toString().c_str());

    if (!fs::is_directory(dirPath, ec)) {
        return files;
    }

    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (entry.is_regular_file(ec)) {
            files.emplace_back(*this, toGeneric(entry.path().string()));
        }
    }

    return files;
}

filesystem::PathResult StdFileSystem::create(const filesystem::Dir& directory) const
{
    std::error_code ec;
    fs::create_directories(fs::u8path(directory.toString().c_str()), ec);
    if (ec) {
        return filesystem::PathErrorE::SystemError;
    }
    return filesystem::PathResult::Success;
}

filesystem::PathResult StdFileSystem::remove(const filesystem::Path& path) const
{
    std::error_code ec;
    fs::path p = fs::u8path(path.toString().c_str());
    if (!fs::exists(p, ec)) {
        return filesystem::PathErrorE::NotFound;
    }

    if (fs::is_directory(p, ec)) {
        fs::remove_all(p, ec);
    } else {
        fs::remove(p, ec);
    }

    if (ec) {
        return filesystem::PathErrorE::SystemError;
    }
    return filesystem::PathResult::Success;
}

core::string StdFileSystem::getName(const filesystem::Path& path) const
{
    fs::path p = fs::u8path(path.toString().c_str());
    return p.filename().string();
}

core::string StdFileSystem::getStem(const filesystem::File& file) const
{
    fs::path p = fs::u8path(file.toString().c_str());
    return p.stem().string();
}

core::string StdFileSystem::getExtension(const filesystem::File& file) const
{
    fs::path p = fs::u8path(file.toString().c_str());
    return p.extension().string();
}

filesystem::FileLoadResult StdFileSystem::load(const filesystem::File& file, core::Vector<uint8_t>& data) const
{
    return loadBinary(file.toString().c_str(), data);
}

filesystem::FileLoadResult StdFileSystem::load(const filesystem::File& file, core::Slice<uint8_t>& data) const
{
    return loadBinary(file.toString().c_str(), data);
}

filesystem::FileLoadResult StdFileSystem::load(const filesystem::File& file, core::string& text) const
{
    return loadText(file.toString().c_str(), text);
}

filesystem::FileSaveResult StdFileSystem::save(const filesystem::File& file, const uint8_t* data, size_t byteLength) const
{
    return saveBinary(file.toString().c_str(), data, byteLength);
}

filesystem::FileSaveResult StdFileSystem::save(const filesystem::File& file, const core::Vector<uint8_t>& data) const
{
    return saveBinary(file.toString().c_str(), data.data(), data.size());
}

filesystem::FileSaveResult StdFileSystem::save(const filesystem::File& file, const core::Slice<uint8_t>& data) const
{
    return saveBinary(file.toString().c_str(), data.data(), data.size());
}

filesystem::FileSaveResult StdFileSystem::save(const filesystem::File& file, const core::string& text) const
{
    return saveText(file.toString().c_str(), text);
}

filesystem::FileSaveResult StdFileSystem::append(const filesystem::File& file, const core::string& text) const
{
    std::ofstream stream(file.toString().c_str(), std::ios::app);
    if (!stream.is_open()) {
        return filesystem::FileSaveResultE::CannotOpenFile;
    }
    stream.write(text.data(), text.size());
    return filesystem::FileSaveResult::Success;
}

filesystem::FileLoadResult StdFileSystem::loadBinary(const char* pPath, core::Vector<uint8_t>& outBuffer) const
{
    std::ifstream stream(pPath, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return filesystem::FileLoadResultE::CannotOpenFile;
    }

    const auto fileSize = static_cast<size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    if (!outBuffer.resize(fileSize)) {
        return filesystem::FileLoadResultE::AllocationFailed;
    }

    if (fileSize > 0) {
        stream.read(reinterpret_cast<char*>(outBuffer.data()), fileSize);
        if (!stream) {
            return filesystem::FileLoadResultE::AllocationFailed;
        }
    }

    return filesystem::FileLoadResult::Success;
}

filesystem::FileLoadResult StdFileSystem::loadBinary(const char* pPath, core::Slice<uint8_t>& outBuffer) const
{
    std::ifstream stream(pPath, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return filesystem::FileLoadResultE::CannotOpenFile;
    }

    const auto fileSize = static_cast<size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    if (!outBuffer.resize(fileSize, outBuffer.getAllocator())) {
        return filesystem::FileLoadResultE::AllocationFailed;
    }

    if (fileSize > 0) {
        stream.read(reinterpret_cast<char*>(outBuffer.data()), fileSize);
        if (!stream) {
            return filesystem::FileLoadResultE::AllocationFailed;
        }
    }

    return filesystem::FileLoadResult::Success;
}

filesystem::FileLoadResult StdFileSystem::loadText(const char* pPath, core::string& outString) const
{
    std::ifstream stream(pPath, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return filesystem::FileLoadResultE::CannotOpenFile;
    }

    const auto fileSize = static_cast<size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    outString.resize(fileSize);
    if (fileSize > 0) {
        stream.read(reinterpret_cast<char*>(outString.data()), fileSize);
    }

    return filesystem::FileLoadResult::Success;
}

filesystem::FileSaveResult StdFileSystem::saveBinary(const char* pPath, const uint8_t* data, size_t byteLength) const
{
    std::ofstream stream(pPath, std::ios::binary | std::ios::out);
    if (!stream.is_open()) {
        return filesystem::FileSaveResultE::CannotOpenFile;
    }

    if (byteLength > 0 && data != nullptr) {
        stream.write(reinterpret_cast<const char*>(data), byteLength);
    }

    return filesystem::FileSaveResult::Success;
}

filesystem::FileSaveResult StdFileSystem::saveText(const char* pPath, const core::string& text) const
{
    std::ofstream stream(pPath, std::ios::out);
    if (!stream.is_open()) {
        return filesystem::FileSaveResultE::CannotOpenFile;
    }

    if (!text.empty()) {
        stream.write(text.data(), text.size());
    }

    return filesystem::FileSaveResult::Success;
}

} // namespace ux3d::imgui
