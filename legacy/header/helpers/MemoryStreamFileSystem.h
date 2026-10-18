// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "filesystem/FileSystem.h"

namespace ux3d::editor {
class MemoryStreamFileSystem final : public filesystem::IFileSystem {
public:
    MemoryStreamFileSystem(core::Vector<uint8_t>& _output)
        : m_outputData(_output) {};
    ~MemoryStreamFileSystem() final = default;

    filesystem::Path toPath(const core::string& path) const final { return filesystem::Path(*this, path); }
    // TODO: implement this by inheriting from QFileSystem
    filesystem::Path fromURI(const core::string& uri) const final { return filesystem::Path(*this, uri); }
    filesystem::File toFile(const core::string& path) const final { return filesystem::File(*this, path); }
    filesystem::Path getTempPath() const final { return filesystem::Path(*this); }
    filesystem::Path getAppDataPath() const final { return filesystem::Path(*this); }
    filesystem::Path getExecutableDirPath() const final { return filesystem::Path(*this); }

protected:
    bool isFile(const filesystem::Path& path) const final;
    bool isDirectory(const filesystem::Path& path) const final;
    bool isAbsolute(const filesystem::Path& path) const final;
    bool isRelative(const filesystem::Path& path) const final;
    bool isEmpty(const filesystem::Path& path) const final;
    bool exists(const filesystem::Path& path) const final;

    core::Vector<filesystem::Path> listContents(const filesystem::Dir& dir) const final;
    filesystem::Path getParent(const filesystem::Path& path) const final;
    filesystem::Path getChild(const filesystem::Path& path, const core::string& name) const final;

    // listFiles returns all contained regular files, if path is a directory. If the path points to a file, the vector will be empty. listFiles will not recurse into subdirectories
    core::Vector<filesystem::File> listFiles(const filesystem::Dir& path) const final;
    filesystem::PathResult create(const filesystem::Dir& directory) const final;

    filesystem::PathResult remove(const filesystem::Path& file) const final;

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
    core::Vector<uint8_t>& m_outputData;
};
}
