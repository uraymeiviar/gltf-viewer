// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/StatisticsPanel.h"
#include "gltf/Accessor.h"
#include "gltf/Animation.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Camera.h"
#include "gltf/Image.h"
#include "gltf/Light.h"
#include "gltf/Material.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/Sampler.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"
#include "gltf/Texture.h"
#include "imgui/AppSettings.h"
#include "imgui/EmbeddedVFS.h"
#include <imgui.h>
#include "imgui/Theme.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace ux3d::imgui {

namespace {

std::string formatBytes(uint64_t bytes)
{
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / 1024.0) << " KB";
        return ss.str();
    } else if (bytes < 1024ULL * 1024 * 1024) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
        return ss.str();
    } else {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)) << " GB";
        return ss.str();
    }
}

struct Row {
    std::string label;
    std::string value;
};

struct StatisticsSnapshot {
    std::vector<Row> rows;
    std::string clipboard;
};

struct StatisticsCache {
    bool valid = false;
    std::string path;
    const ux3d::gltf::Gltf* model = nullptr;
    StatisticsSnapshot snapshot;
};

StatisticsCache s_cache;

StatisticsSnapshot buildSnapshot(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath)
{
    // Compute total metrics.
    uint64_t totalVertices = 0;
    uint64_t totalTriangles = 0;
    uint64_t primitiveCount = 0;

    const auto& meshes = gltfModel.getElements<ux3d::gltf::Mesh>();
    for (const auto& mesh : meshes) {
        for (const auto& prim : mesh.getPrimitives()) {
            ++primitiveCount;

            // Vertices from POSITION attribute.
            for (const auto& attr : prim.getAttributes()) {
                if (attr.getName() == "POSITION" && attr.getAccessor() != nullptr) {
                    totalVertices += attr.getAccessor()->getCount();
                    break;
                }
            }

            // Triangles from indices or vertices.
            if (prim.getIndicesAccessor() != nullptr) {
                totalTriangles += prim.getIndicesAccessor()->getCount() / 3;
            } else {
                for (const auto& attr : prim.getAttributes()) {
                    if (attr.getName() == "POSITION" && attr.getAccessor() != nullptr) {
                        totalTriangles += attr.getAccessor()->getCount() / 3;
                        break;
                    }
                }
            }
        }
    }

    uint64_t totalBufferBytes = 0;
    for (const auto& buf : gltfModel.getElements<ux3d::gltf::Buffer>()) {
        totalBufferBytes += buf.getByteLength();
    }

    uint64_t totalImageBytes = 0;
    for (const auto& img : gltfModel.getElements<ux3d::gltf::Image>()) {
        size_t s = img.getData().getPixels().size();
        if (s == 0 && img.getEncodedBytes() != nullptr) {
            s = img.getEncodedBytes()->size();
        }
        totalImageBytes += s;
    }

    // Resolve the file size once per snapshot. Embedded models must not be
    // reported as disk files, and an unavailable disk path must stay visible
    // rather than silently turning into 0 B.
    uint64_t fileSizeBytes = 0;
    bool fileSizeKnown = false;
    std::string fileSource = "Unsaved";
    if (!currentFilePath.empty()) {
        if (VfsFile vfsFile = EmbeddedVFS::instance().getFile(currentFilePath); vfsFile.isValid()) {
            fileSizeBytes = vfsFile.size;
            fileSizeKnown = true;
            fileSource = "Embedded";
        } else {
            std::error_code ec;
            const auto directSize = std::filesystem::file_size(std::filesystem::path(currentFilePath), ec);
            if (!ec) {
                fileSizeBytes = directSize;
                fileSizeKnown = true;
                fileSource = "Disk";
            } else {
                const std::string resolved = AppSettings::instance().resolveAssetPath(currentFilePath);
                if (!resolved.empty()) {
                    ec.clear();
                    const auto resolvedSize = std::filesystem::file_size(std::filesystem::path(resolved), ec);
                    if (!ec) {
                        fileSizeBytes = resolvedSize;
                        fileSizeKnown = true;
                        fileSource = "Disk";
                    }
                }
                if (!fileSizeKnown) {
                    fileSource = "Unavailable";
                }
            }
        }
    }

    const std::string fileSizeText = fileSizeKnown ? formatBytes(fileSizeBytes) : "N/A";
    const std::string bufferText = formatBytes(totalBufferBytes);
    const std::string imageText = formatBytes(totalImageBytes);

    const size_t sceneCount = gltfModel.getElements<ux3d::gltf::Scene>().size();
    const size_t nodeCount = gltfModel.getElements<ux3d::gltf::Node>().size();
    const size_t materialCount = gltfModel.getElements<ux3d::gltf::Material>().size();
    const size_t textureCount = gltfModel.getElements<ux3d::gltf::Texture>().size();
    const size_t imageCount = gltfModel.getElements<ux3d::gltf::Image>().size();
    const size_t samplerCount = gltfModel.getElements<ux3d::gltf::Sampler>().size();
    const size_t animationCount = gltfModel.getElements<ux3d::gltf::Animation>().size();
    const size_t skinCount = gltfModel.getElements<ux3d::gltf::Skin>().size();
    const size_t cameraCount = gltfModel.getElements<ux3d::gltf::Camera>().size();
    const size_t lightCount = gltfModel.getElements<ux3d::gltf::Light>().size();
    const size_t accessorCount = gltfModel.getElements<ux3d::gltf::Accessor>().size();
    const size_t bufferViewCount = gltfModel.getElements<ux3d::gltf::BufferView>().size();
    const size_t bufferCount = gltfModel.getElements<ux3d::gltf::Buffer>().size();

    StatisticsSnapshot snapshot;
    snapshot.rows = {
        { "File Size", fileSizeText },
        { "File Source", fileSource },
        { "Total Buffer Data", bufferText },
        { "Total Image Data", imageText },
        { "Scenes", std::to_string(sceneCount) },
        { "Nodes", std::to_string(nodeCount) },
        { "Meshes", std::to_string(meshes.size()) },
        { "Primitives", std::to_string(primitiveCount) },
        { "Total Vertices", std::to_string(totalVertices) },
        { "Total Triangles", std::to_string(totalTriangles) },
        { "Materials", std::to_string(materialCount) },
        { "Textures", std::to_string(textureCount) },
        { "Images", std::to_string(imageCount) },
        { "Samplers", std::to_string(samplerCount) },
        { "Animations", std::to_string(animationCount) },
        { "Skins", std::to_string(skinCount) },
        { "Cameras", std::to_string(cameraCount) },
        { "Lights", std::to_string(lightCount) },
        { "Accessors", std::to_string(accessorCount) },
        { "Buffer Views", std::to_string(bufferViewCount) },
        { "Buffers", std::to_string(bufferCount) },
    };

    std::ostringstream copy;
    copy << "File: " << currentFilePath << "\n";
    copy << "File Source: " << fileSource << "\n";
    copy << "File Size: " << fileSizeText << "\n";
    copy << "Scenes: " << sceneCount << "\n";
    copy << "Nodes: " << nodeCount << "\n";
    copy << "Meshes: " << meshes.size() << "\n";
    copy << "Primitives: " << primitiveCount << "\n";
    copy << "Vertices: " << totalVertices << "\n";
    copy << "Triangles: " << totalTriangles << "\n";
    copy << "Materials: " << materialCount << "\n";
    copy << "Textures: " << textureCount << "\n";
    copy << "Images: " << imageCount << "\n";
    copy << "Animations: " << animationCount << "\n";
    copy << "Skins: " << skinCount << "\n";
    copy << "Cameras: " << cameraCount << "\n";
    copy << "Lights: " << lightCount << "\n";
    copy << "Accessors: " << accessorCount << "\n";
    copy << "Buffer Views: " << bufferViewCount << "\n";
    copy << "Buffers: " << bufferCount << " (" << bufferText << ")\n";
    snapshot.clipboard = copy.str();
    return snapshot;
}

void ensureSnapshot(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath)
{
    if (s_cache.valid && s_cache.model == &gltfModel && s_cache.path == currentFilePath) {
        return;
    }
    s_cache.snapshot = buildSnapshot(gltfModel, currentFilePath);
    s_cache.path = currentFilePath;
    s_cache.model = &gltfModel;
    s_cache.valid = true;
}

} // namespace

void StatisticsPanel::invalidate()
{
    s_cache.valid = false;
}

void StatisticsPanel::render(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath)
{
    ensureSnapshot(gltfModel, currentFilePath);
    const StatisticsSnapshot& snapshot = s_cache.snapshot;

    // Action buttons. Copy and display are both backed by the same snapshot.
    if (ImGui::Button(ICON_MDI_REFRESH " Refresh")) {
        invalidate();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy to Clipboard")) {
        ImGui::SetClipboardText(snapshot.clipboard.c_str());
    }

    ImGui::Separator();

    if (ImGui::BeginTable("StatisticsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto addRow = [](const Row& row) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.label.c_str());
            ImGui::TableNextColumn();
            ImFont* mono = getMonoFont();
            if (mono != nullptr) ImGui::PushFont(mono);
            ImGui::Text("%s", row.value.c_str());
            if (mono != nullptr) ImGui::PopFont();
        };

        for (const Row& row : snapshot.rows) {
            addRow(row);
        }

        ImGui::EndTable();
    }
}

} // namespace ux3d::imgui
