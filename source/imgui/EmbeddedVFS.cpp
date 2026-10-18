// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/EmbeddedVFS.h"
#include "imgui/zstd.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" {
extern const uint8_t g_embeddedResourcesArchive[];
extern const size_t g_embeddedResourcesArchiveSize;
}

namespace ux3d::imgui {

#pragma pack(push, 1)
struct VfsHeader {
    char magic[4];       // "GVFS"
    uint32_t version;   // 1
    uint32_t fileCount;
    uint32_t tableSize;
    uint32_t payloadSize;
};
#pragma pack(pop)

EmbeddedVFS& EmbeddedVFS::instance()
{
    static EmbeddedVFS s_instance;
    return s_instance;
}

std::string EmbeddedVFS::normalizePath(const std::string& path)
{
    std::string s = path;
    for (char& c : s) {
        if (c == '\\') c = '/';
    }

    if (s.rfind("file:///", 0) == 0) {
        s = s.substr(8);
    } else if (s.rfind("file://", 0) == 0) {
        s = s.substr(7);
    }

    // Collapse multiple consecutive slashes
    std::string cleaned;
    cleaned.reserve(s.size());
    bool lastWasSlash = false;
    for (char c : s) {
        if (c == '/') {
            if (!lastWasSlash) {
                cleaned.push_back(c);
            }
            lastWasSlash = true;
        } else {
            cleaned.push_back(c);
            lastWasSlash = false;
        }
    }
    s = std::move(cleaned);

    // If it contains "/resources/", extract from "resources/..." onwards
    auto pos = s.find("/resources/");
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    } else {
        const char* knownDirs[] = { "/shaders/", "/fonts/", "/environments/", "/sampleGltf/" };
        for (const char* kd : knownDirs) {
            auto p = s.find(kd);
            if (p != std::string::npos) {
                s = s.substr(p + 1);
                break;
            }
        }
    }

    while (s.rfind("./", 0) == 0) s = s.substr(2);
    while (!s.empty() && s.front() == '/') s.erase(s.begin());
    while (!s.empty() && s.back() == '/') s.pop_back();

    return s;
}

bool EmbeddedVFS::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (g_embeddedResourcesArchiveSize == 0) {
        std::fprintf(stderr, "[EmbeddedVFS] Error: Embedded archive is empty\n");
        return false;
    }

    unsigned long long contentSize = ZSTD_getFrameContentSize(g_embeddedResourcesArchive, g_embeddedResourcesArchiveSize);
    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN || contentSize == ZSTD_CONTENTSIZE_ERROR) {
        std::fprintf(stderr, "[EmbeddedVFS] Error: Cannot determine decompressed size of embedded archive\n");
        return false;
    }

    m_decompressedBuffer.resize(static_cast<size_t>(contentSize));
    size_t decompressedBytes = ZSTD_decompress(
        m_decompressedBuffer.data(),
        m_decompressedBuffer.size(),
        g_embeddedResourcesArchive,
        g_embeddedResourcesArchiveSize
    );

    if (ZSTD_isError(decompressedBytes)) {
        std::fprintf(stderr, "[EmbeddedVFS] ZSTD_decompress failed: %s\n", ZSTD_getErrorName(decompressedBytes));
        m_decompressedBuffer.clear();
        return false;
    }

    if (decompressedBytes < sizeof(VfsHeader)) {
        std::fprintf(stderr, "[EmbeddedVFS] Decompressed buffer smaller than header (%zu bytes)\n", decompressedBytes);
        m_decompressedBuffer.clear();
        return false;
    }

    const auto* header = reinterpret_cast<const VfsHeader*>(m_decompressedBuffer.data());
    if (std::memcmp(header->magic, "GVFS", 4) != 0) {
        std::fprintf(stderr, "[EmbeddedVFS] Invalid archive magic header\n");
        m_decompressedBuffer.clear();
        return false;
    }

    if (header->version != 1) {
        std::fprintf(stderr, "[EmbeddedVFS] Unsupported archive version %u\n", header->version);
        m_decompressedBuffer.clear();
        return false;
    }

    if (sizeof(VfsHeader) + header->tableSize + header->payloadSize > decompressedBytes) {
        std::fprintf(stderr, "[EmbeddedVFS] Corrupt archive table/payload size\n");
        m_decompressedBuffer.clear();
        return false;
    }

    const uint8_t* tablePtr = m_decompressedBuffer.data() + sizeof(VfsHeader);
    const uint8_t* tableEnd = tablePtr + header->tableSize;
    const uint8_t* payloadPtr = tableEnd;

    for (uint32_t i = 0; i < header->fileCount && tablePtr < tableEnd; ++i) {
        if (tablePtr + 2 > tableEnd) break;
        uint16_t pathLen = *reinterpret_cast<const uint16_t*>(tablePtr);
        tablePtr += 2;

        if (tablePtr + pathLen + 8 > tableEnd) break;
        std::string relPath(reinterpret_cast<const char*>(tablePtr), pathLen);
        tablePtr += pathLen;

        uint32_t offset = *reinterpret_cast<const uint32_t*>(tablePtr);
        tablePtr += 4;
        uint32_t size = *reinterpret_cast<const uint32_t*>(tablePtr);
        tablePtr += 4;

        if (offset + size > header->payloadSize) {
            continue;
        }

        VfsFile vf;
        vf.data = payloadPtr + offset;
        vf.size = size;

        relPath = normalizePath(relPath);

        // Store relative and "resources/" prefixed variants
        m_files[relPath] = vf;
        std::string resPrefixed = "resources/" + relPath;
        m_files[resPrefixed] = vf;

        // Populate directory index
        auto registerDirHierarchy = [this](const std::string& fullPath) {
            auto lastSlash = fullPath.rfind('/');
            if (lastSlash != std::string::npos) {
                std::string dir = fullPath.substr(0, lastSlash);
                std::string fn = fullPath.substr(lastSlash + 1);

                m_dirFiles[dir].push_back(fn);

                auto& contents = m_dirContents[dir];
                if (std::find(contents.begin(), contents.end(), fn) == contents.end()) {
                    contents.push_back(fn);
                }

                while (!dir.empty()) {
                    m_directories.insert(dir);
                    auto prevSlash = dir.rfind('/');
                    if (prevSlash != std::string::npos) {
                        std::string parent = dir.substr(0, prevSlash);
                        std::string sub = dir.substr(prevSlash + 1);
                        auto& parentContents = m_dirContents[parent];
                        if (std::find(parentContents.begin(), parentContents.end(), sub) == parentContents.end()) {
                            parentContents.push_back(sub);
                        }
                        dir = parent;
                    } else {
                        break;
                    }
                }
            }
        };

        registerDirHierarchy(relPath);
        registerDirHierarchy(resPrefixed);
    }

    m_initialized = true;
    std::printf("[EmbeddedVFS] Initialized with %u embedded files (decompressed: %.2f MB)\n",
                header->fileCount,
                decompressedBytes / (1024.0f * 1024.0f));
    return true;
}

VfsFile EmbeddedVFS::getFile(const std::string& path) const
{
    if (!m_initialized) return VfsFile{};

    std::string norm = normalizePath(path);
    auto it = m_files.find(norm);
    if (it != m_files.end()) {
        return it->second;
    }

    if (norm.rfind("resources/", 0) == 0) {
        auto it2 = m_files.find(norm.substr(10));
        if (it2 != m_files.end()) {
            return it2->second;
        }
    } else {
        auto it3 = m_files.find("resources/" + norm);
        if (it3 != m_files.end()) {
            return it3->second;
        }
    }

    return VfsFile{};
}

bool EmbeddedVFS::hasFile(const std::string& path) const
{
    return getFile(path).isValid();
}

bool EmbeddedVFS::hasDirectory(const std::string& path) const
{
    if (!m_initialized) return false;

    std::string norm = normalizePath(path);
    if (m_directories.find(norm) != m_directories.end()) {
        return true;
    }

    if (norm.rfind("resources/", 0) == 0) {
        if (m_directories.find(norm.substr(10)) != m_directories.end()) {
            return true;
        }
    } else {
        if (m_directories.find("resources/" + norm) != m_directories.end()) {
            return true;
        }
    }

    return false;
}

bool EmbeddedVFS::exists(const std::string& path) const
{
    return hasFile(path) || hasDirectory(path);
}

std::vector<std::string> EmbeddedVFS::listFiles(const std::string& dirPath) const
{
    std::vector<std::string> result;
    if (!m_initialized) return result;

    std::string norm = normalizePath(dirPath);
    auto it = m_dirFiles.find(norm);
    if (it == m_dirFiles.end()) {
        if (norm.rfind("resources/", 0) == 0) {
            it = m_dirFiles.find(norm.substr(10));
        } else {
            it = m_dirFiles.find("resources/" + norm);
        }
    }

    if (it != m_dirFiles.end()) {
        std::string base = dirPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') {
            base += "/";
        }
        for (const auto& fn : it->second) {
            result.push_back(base + fn);
        }
    }

    return result;
}

std::vector<std::string> EmbeddedVFS::listContents(const std::string& dirPath) const
{
    std::vector<std::string> result;
    if (!m_initialized) return result;

    std::string norm = normalizePath(dirPath);
    auto it = m_dirContents.find(norm);
    if (it == m_dirContents.end()) {
        if (norm.rfind("resources/", 0) == 0) {
            it = m_dirContents.find(norm.substr(10));
        } else {
            it = m_dirContents.find("resources/" + norm);
        }
    }

    if (it != m_dirContents.end()) {
        std::string base = dirPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') {
            base += "/";
        }
        for (const auto& entry : it->second) {
            result.push_back(base + entry);
        }
    }

    return result;
}

} // namespace ux3d::imgui
