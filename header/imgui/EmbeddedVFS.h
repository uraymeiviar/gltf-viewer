// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace ux3d::imgui {

struct VfsFile {
    const uint8_t* data = nullptr;
    size_t size = 0;

    bool isValid() const { return data != nullptr; }
    std::string_view asStringView() const {
        return std::string_view(reinterpret_cast<const char*>(data), size);
    }
};

class EmbeddedVFS {
public:
    static EmbeddedVFS& instance();

    bool initialize();
    bool isInitialized() const { return m_initialized; }

    bool hasFile(const std::string& path) const;
    bool hasDirectory(const std::string& path) const;
    bool exists(const std::string& path) const;

    VfsFile getFile(const std::string& path) const;
    std::vector<std::string> listFiles(const std::string& dirPath) const;
    std::vector<std::string> listContents(const std::string& dirPath) const;

    static std::string normalizePath(const std::string& path);

private:
    EmbeddedVFS() = default;
    ~EmbeddedVFS() = default;

    EmbeddedVFS(const EmbeddedVFS&) = delete;
    EmbeddedVFS& operator=(const EmbeddedVFS&) = delete;

    bool m_initialized = false;
    std::vector<uint8_t> m_decompressedBuffer;
    std::unordered_map<std::string, VfsFile> m_files;
    std::unordered_set<std::string> m_directories;
    std::unordered_map<std::string, std::vector<std::string>> m_dirFiles;
    std::unordered_map<std::string, std::vector<std::string>> m_dirContents;
};

} // namespace ux3d::imgui
