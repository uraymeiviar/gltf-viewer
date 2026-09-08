#pragma once

#include "filesystem/FileSystem.h"
#include "filesystem/FileSystemResults.h"

namespace ux3d::core {
template <typename T>
class Slice;
}

namespace ux3d::filesystem {
class TestFileSystem final : public IFileSystem {
public:
    ~TestFileSystem() final = default;
    static TestFileSystem& instance();

    Path toPath(const core::string& path) const final { return Path(*this, path); }
    File toFile(const core::string& path) const final { return File(*this, path); }
    Path fromURI(const core::string& uri) const final { return Path(*this, uri); }

    filesystem::Path getTempPath() const final { return filesystem::Path(*this); }
    filesystem::Path getAppDataPath() const final { return filesystem::Path(*this); }
    filesystem::Path getExecutableDirPath() const final { return filesystem::Path(*this); }

protected:
    bool isFile(const Path& path) const final;
    bool isDirectory(const Path& path) const final;
    bool isAbsolute(const Path& path) const final;
    bool isRelative(const Path& path) const final;
    bool isEmpty(const Path& path) const final;
    bool exists(const Path& path) const final;

    core::Vector<Path> listContents(const Dir& dir) const final;
    Path getParent(const Path& path) const final;
    Path getChild(const Path& path, const core::string& name) const final;

    // listFiles returns all contained regular files, if path is a directory. If the path points to a file, the vector will be empty. listFiles will not recurse into subdirectories
    core::Vector<File> listFiles(const Dir& path) const final;
    PathResult create(const Dir& directory) const final;

    filesystem::PathResult remove(const filesystem::Path& path) const final;

    core::string getName(const Path& path) const final;
    core::string getStem(const File& file) const final;
    core::string getExtension(const File& file) const final;

    FileLoadResult load(const File& file, core::Vector<uint8_t>& data) const final;
    FileLoadResult load(const File& file, core::Slice<uint8_t>& data) const final;
    FileLoadResult load(const File& file, core::string& text) const final;

    FileSaveResult save(const File& file, const uint8_t* data, size_t byteLength) const final;
    FileSaveResult save(const File& file, const core::Vector<uint8_t>& data) const final;
    FileSaveResult save(const File& file, const core::Slice<uint8_t>& data) const final;
    FileSaveResult save(const File& file, const core::string& text) const final;

    FileSaveResult append(const File& _file, const core::string& _text) const final;

private:
    inline FileLoadResult loadBinaryFile(const core::string& _Path, core::Vector<uint8_t>& _OutBuffer) const { return loadBinaryFile(_Path.data(), _OutBuffer); }
    inline FileLoadResult loadBinaryFile(const core::string& _Path, core::Slice<uint8_t>& _OutBuffer) const { return loadBinaryFile(_Path.data(), _OutBuffer); }
    inline FileLoadResult loadTextFile(const core::string& _Path, core::string& _OutString) const { return loadTextFile(_Path.data(), _OutString); }

    FileLoadResult loadBinaryFile(const char* _pPath, core::Vector<uint8_t>& _OutBuffer) const;
    FileLoadResult loadBinaryFile(const char* _pPath, core::Slice<uint8_t>& _OutBuffer) const;
    FileLoadResult loadTextFile(const char* _pPath, core::string& _OutString) const;
    FileLoadResult loadFile(const char* _pPath, core::string& _OutBuffer, std::ios_base::openmode mode) const;

    FileSaveResult saveBinaryFile(const char* _pPath, const uint8_t* _pData, size_t _byteLength) const;
    FileSaveResult saveBinaryFile(const char* _pPath, const core::Vector<uint8_t>& _OutBuffer) const;
    FileSaveResult saveBinaryFile(const char* _pPath, const core::Slice<uint8_t>& _OutBuffer) const;
    FileSaveResult saveTextFile(const char* _pPath, const core::string& _OutString) const;

    template <class Container>
    FileLoadResult loadFile(const char* _pPath, Container& _OutBuffer, core::IAllocator* _Allocator, std::ios_base::openmode mode) const;

    template <class Container>
    FileSaveResult saveFile(const char* _pPath, const Container& _OutBuffer, std::ios_base::openmode mode = std::ios_base::out) const;
};
}
