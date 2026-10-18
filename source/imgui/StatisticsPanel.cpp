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
#include <imgui.h>
#include "imgui/Theme.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace ux3d::imgui {

static std::string formatBytes(uint64_t bytes)
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

void StatisticsPanel::render(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath)
{
    // Compute total metrics
    uint64_t totalVertices = 0;
    uint64_t totalTriangles = 0;
    uint64_t primitiveCount = 0;

    auto& meshes = gltfModel.getElements<ux3d::gltf::Mesh>();
    for (const auto& mesh : meshes) {
        for (const auto& prim : mesh.getPrimitives()) {
            ++primitiveCount;

            // Vertices from POSITION attribute
            for (const auto& attr : prim.getAttributes()) {
                if (attr.getName() == "POSITION" && attr.getAccessor() != nullptr) {
                    totalVertices += attr.getAccessor()->getCount();
                    break;
                }
            }

            // Triangles from indices or vertices
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

    // Total buffer memory
    uint64_t totalBufferBytes = 0;
    for (const auto& buf : gltfModel.getElements<ux3d::gltf::Buffer>()) {
        totalBufferBytes += buf.getByteLength();
    }

    // Total image pixel memory estimate
    uint64_t totalImageBytes = 0;
    for (const auto& img : gltfModel.getElements<ux3d::gltf::Image>()) {
        size_t s = img.getData().getPixels().size();
        if (s == 0 && img.getEncodedBytes() != nullptr) {
            s = img.getEncodedBytes()->size();
        }
        totalImageBytes += s;
    }

    // File size on disk
    uint64_t fileSizeBytes = 0;
    if (!currentFilePath.empty()) {
        std::error_code ec;
        fileSizeBytes = std::filesystem::file_size(currentFilePath, ec);
    }

    // Action Buttons
    if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy to Clipboard")) {
        std::ostringstream ss;
        ss << "File: " << currentFilePath << "\n";
        ss << "File Size: " << formatBytes(fileSizeBytes) << "\n";
        ss << "Scenes: " << gltfModel.getElements<ux3d::gltf::Scene>().size() << "\n";
        ss << "Nodes: " << gltfModel.getElements<ux3d::gltf::Node>().size() << "\n";
        ss << "Meshes: " << meshes.size() << "\n";
        ss << "Primitives: " << primitiveCount << "\n";
        ss << "Vertices: " << totalVertices << "\n";
        ss << "Triangles: " << totalTriangles << "\n";
        ss << "Materials: " << gltfModel.getElements<ux3d::gltf::Material>().size() << "\n";
        ss << "Textures: " << gltfModel.getElements<ux3d::gltf::Texture>().size() << "\n";
        ss << "Images: " << gltfModel.getElements<ux3d::gltf::Image>().size() << "\n";
        ss << "Animations: " << gltfModel.getElements<ux3d::gltf::Animation>().size() << "\n";
        ss << "Skins: " << gltfModel.getElements<ux3d::gltf::Skin>().size() << "\n";
        ss << "Cameras: " << gltfModel.getElements<ux3d::gltf::Camera>().size() << "\n";
        ss << "Lights: " << gltfModel.getElements<ux3d::gltf::Light>().size() << "\n";
        ss << "Accessors: " << gltfModel.getElements<ux3d::gltf::Accessor>().size() << "\n";
        ss << "Buffer Views: " << gltfModel.getElements<ux3d::gltf::BufferView>().size() << "\n";
        ss << "Buffers: " << gltfModel.getElements<ux3d::gltf::Buffer>().size() << " (" << formatBytes(totalBufferBytes) << ")\n";
        ImGui::SetClipboardText(ss.str().c_str());
    }

    ImGui::Separator();

    // Table display
    if (ImGui::BeginTable("StatisticsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto addRow = [](const char* name, const std::string& val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", name);
            ImGui::TableNextColumn();
            ImFont* mono = getMonoFont();
            if (mono != nullptr) ImGui::PushFont(mono);
            ImGui::Text("%s", val.c_str());
            if (mono != nullptr) ImGui::PopFont();
        };

        addRow("File Size on Disk", formatBytes(fileSizeBytes));
        addRow("Total Buffer Data", formatBytes(totalBufferBytes));
        addRow("Total Image Data", formatBytes(totalImageBytes));

        addRow("Scenes", std::to_string(gltfModel.getElements<ux3d::gltf::Scene>().size()));
        addRow("Nodes", std::to_string(gltfModel.getElements<ux3d::gltf::Node>().size()));
        addRow("Meshes", std::to_string(meshes.size()));
        addRow("Primitives", std::to_string(primitiveCount));
        addRow("Total Vertices", std::to_string(totalVertices));
        addRow("Total Triangles", std::to_string(totalTriangles));

        addRow("Materials", std::to_string(gltfModel.getElements<ux3d::gltf::Material>().size()));
        addRow("Textures", std::to_string(gltfModel.getElements<ux3d::gltf::Texture>().size()));
        addRow("Images", std::to_string(gltfModel.getElements<ux3d::gltf::Image>().size()));
        addRow("Samplers", std::to_string(gltfModel.getElements<ux3d::gltf::Sampler>().size()));

        addRow("Animations", std::to_string(gltfModel.getElements<ux3d::gltf::Animation>().size()));
        addRow("Skins", std::to_string(gltfModel.getElements<ux3d::gltf::Skin>().size()));
        addRow("Cameras", std::to_string(gltfModel.getElements<ux3d::gltf::Camera>().size()));
        addRow("Lights", std::to_string(gltfModel.getElements<ux3d::gltf::Light>().size()));

        addRow("Accessors", std::to_string(gltfModel.getElements<ux3d::gltf::Accessor>().size()));
        addRow("Buffer Views", std::to_string(gltfModel.getElements<ux3d::gltf::BufferView>().size()));
        addRow("Buffers", std::to_string(gltfModel.getElements<ux3d::gltf::Buffer>().size()));

        ImGui::EndTable();
    }
}

} // namespace ux3d::imgui
