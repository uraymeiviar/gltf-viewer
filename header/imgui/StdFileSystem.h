// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "filesystem/FileSystem.h"

namespace ux3d::imgui {

class StdFileSystem final : public filesystem::IFileSystem {
public:
    ~StdFileSystem() final = default;
    static StdFileSystem& instance();

    filesystem::Path toPath(const core::string& path) const final { return filesystem::Path(*this, path); }
    filesystem::Path fromURI(const core::string& uri) const final;
    filesystem::File toFile(const core::string& path) const final { return filesystem::File(*this, path); }
    filesystem::Path getTempPath() const final;
    filesystem::Path getAppDataPath() const final;
    filesystem::Path getExecutableDirPath() const final;

    filesystem::PathResult remove(const filesystem::Path& file) const final;
    bool isFile(const filesystem::Path& path) const final;

protected:
    bool isDirectory(const filesystem::Path& path) const final;
    bool isAbsolute(const filesystem::Path& path) const final;
    bool isRelative(const filesystem::Path& path) const final;
    bool isEmpty(const filesystem::Path& path) const final;
    bool exists(const filesystem::Path& path) const final;

    core::Vector<filesystem::Path> listContents(const filesystem::Dir& dir) const final;
    filesystem::Path getParent(const filesystem::Path& path) const final;
    filesystem::Path getChild(const filesystem::Path& path, const core::string& name) const final;

    core::Vector<filesystem::File> listFiles(const filesystem::Dir& path) const final;
    filesystem::PathResult create(const filesystem::Dir& directory) const final;

    core::string getName(const filesystem::Path& path) const final;
    core::string getStem(const filesystem::File& file) const final;
    core::string getExtension(const filesystem::File& file) const final;

    filesystem::FileLoadResult load(const filesystem::File& file, core::Vector<uint8_t>& data) const final;
    filesystem::FileLoadResult load(const filesystem::File& file, core::Slice<uint8_t>& data) const final;
    filesystem::FileLoadResult load(const filesystem::File& file, core::string& text) const final;

    filesystem::FileSaveResult save(const filesystem::File& file, const uint8_t* data, size_t byteLength) const final;
    filesystem::FileSaveResult save(const filesystem::File& file, const core::Vector<uint8_t>& data) const final;
    filesystem::FileSaveResult save(const filesystem::File& file, const core::Slice<uint8_t>& data) const final;
    filesystem::FileSaveResult save(const filesystem::File& file, const core::string& text) const final;

    filesystem::FileSaveResult append(const filesystem::File& _file, const core::string& _text) const final;

private:
    filesystem::FileLoadResult loadBinary(const char* pPath, core::Vector<uint8_t>& outBuffer) const;
    filesystem::FileLoadResult loadBinary(const char* pPath, core::Slice<uint8_t>& outBuffer) const;
    filesystem::FileLoadResult loadText(const char* pPath, core::string& outString) const;

    filesystem::FileSaveResult saveBinary(const char* pPath, const uint8_t* data, size_t byteLength) const;
    filesystem::FileSaveResult saveText(const char* pPath, const core::string& text) const;
};

} // namespace ux3d::imgui
