// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/RuntimeTelemetry.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/RenderQuality.h"
#include "imgui/ResourceMemory.h"
#include "imgui/UiProbe.h"
#include "imgui/Profiling.h"
#include "core/LoadProgress.h"
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

bool s_focusRequested = false;

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
    uint64_t gpu = 0, cpu = 0, encoded = 0;
    uint32_t width = 0, height = 0, levels = 0;
};

struct StatisticsSnapshot {
    std::vector<Row> rows;
    std::vector<Row> textures, meshMemory;
    uint64_t textureBytes = 0, accessorBytes = 0;
    std::string clipboard;
};

struct StatisticsCache {
    bool valid = false;
    std::string path;
    const ux3d::gltf::Gltf* model = nullptr;
    RenderBufferStatistics buffers;
    StatisticsSnapshot snapshot;
};

StatisticsCache s_cache;

StatisticsSnapshot buildSnapshot(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath,
                                 const RenderBufferStatistics& buffers, const ux3d::renderer::GltfRenderLayer* layer)
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

    const std::string decalResolution = buffers.decalBufferActive
        ? (std::to_string(buffers.decalBufferWidth) + " x " + std::to_string(buffers.decalBufferHeight))
        : std::string();
    const std::string decalMemory = buffers.decalBufferActive ? formatBytes(buffers.decalBufferBytes) : std::string();

    StatisticsSnapshot snapshot;
    for (const auto& image : gltfModel.getImages()) {
        const uint64_t bytes = imageGpuBytes(&image,layer);
        snapshot.textureBytes += bytes;
        const std::string name = image.getUri().empty() ? ("Image " + std::to_string(image.m_ID)) : std::string(image.getUri().c_str());
        std::string value = memoryText(bytes) + " GPU; " + std::to_string(image.getData().getWidth()) + " x " + std::to_string(image.getData().getHeight()) + " decoded";
        if (layer) for (const auto& [key,texture] : layer->m_textureMapping.map) if (std::get<0>(key) == &image && texture)
            value += "; " + std::to_string(texture->residentWidth) + " x " + std::to_string(texture->residentHeight) + (std::get<1>(key) ? " linear, " : " sRGB, ") + std::to_string(texture->residentLevels) + " mips";
        value += "; " + memoryText(image.getData().getPixels().size()) + " CPU; " + memoryText(image.getEncodedBytes()->size()) + " encoded";
        Row row{name,value,bytes,image.getData().getPixels().size(),image.getEncodedBytes()->size()};
        if (layer) for (const auto& [key,texture] : layer->m_textureMapping.map) if (std::get<0>(key) == &image && texture) {
            row.width = texture->residentWidth; row.height = texture->residentHeight; row.levels = texture->residentLevels;
        }
        snapshot.textures.push_back(std::move(row));
    }
    if (layer) for (const auto& [accessor,buffer] : layer->m_bufferMapping.map) if (buffer) snapshot.accessorBytes += buffer->residentBytes;
    for (const auto& mesh : meshes) snapshot.meshMemory.push_back({
        mesh.hasName() ? std::string(mesh.getName().c_str()) : ("Mesh " + std::to_string(mesh.m_ID)), memoryText(meshGpuBytes(mesh,layer)),meshGpuBytes(mesh,layer)});
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
    // A2.1: one shared decal buffer, so these rows are counted once for all viewports.
    // Files without a decal buffer keep the panel exactly as it was before A2.1.
    if (buffers.decalBufferActive) {
        snapshot.rows.push_back({ "Decal Buffer", "Active (shared)" });
        snapshot.rows.push_back({ "Decal Buffer Resolution", decalResolution });
        snapshot.rows.push_back({ "Decal Buffer Memory", decalMemory });
    }

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
    if (buffers.decalBufferActive) {
        copy << "Decal Buffer: Active (shared)\n";
        copy << "Decal Buffer Resolution: " << decalResolution << "\n";
        copy << "Decal Buffer Memory: " << decalMemory << "\n";
    }
    snapshot.clipboard = copy.str();
    for (const auto& row : snapshot.textures) snapshot.clipboard += "Texture: " + row.label + ": " + row.value + "\n";
    for (const auto& row : snapshot.meshMemory) snapshot.clipboard += "Mesh GPU accessor payload: " + row.label + ": " + row.value + "\n";
    return snapshot;
}

void ensureSnapshot(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath,
                    const RenderBufferStatistics& buffers, const ux3d::renderer::GltfRenderLayer* layer)
{
    if (s_cache.valid && s_cache.model == &gltfModel && s_cache.path == currentFilePath
        && s_cache.buffers == buffers) {
        return;
    }
    s_cache.snapshot = buildSnapshot(gltfModel, currentFilePath, buffers, layer);
    s_cache.path = currentFilePath;
    s_cache.model = &gltfModel;
    s_cache.buffers = buffers;
    s_cache.valid = true;
}

} // namespace

void StatisticsPanel::invalidate()
{
    s_cache.valid = false;
}

void StatisticsPanel::requestFocus()
{
    s_focusRequested = true;
}

bool StatisticsPanel::consumeFocusRequest()
{
    const bool requested = s_focusRequested;
    s_focusRequested = false;
    return requested;
}

void StatisticsPanel::render(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath,
                             const RenderBufferStatistics& buffers, const ux3d::renderer::GltfRenderLayer* layer)
{
    ensureSnapshot(gltfModel, currentFilePath, buffers, layer);
    const StatisticsSnapshot& snapshot = s_cache.snapshot;
    const auto& live = RuntimeTelemetry::get();
    const auto& memory = live.memory;
    char cpu[180];
    std::snprintf(cpu,sizeof(cpu),"%.2f ms frame / %.2f ms P95 / %.2f ms max",live.cpuMs,live.p95CpuMs,live.maxCpuMs);
    char gpu[140]; std::snprintf(gpu,sizeof(gpu),"%.2f ms GPU; %.1f UI FPS / %.1f redraws/s",live.gpuMs,live.fps,live.renderFps);
    char process[100]; std::snprintf(process,sizeof(process),"%.1f%% across %u logical processors; %.2f CPU seconds",live.cpuPercent,memory.logicalProcessors,memory.processCpu100ns/1e7);
    const auto& startup = profiling::startupProfile();
    std::vector<Row> runtime = {
        {"GPU memory source",memory.gpuSource},
        {"VRAM usage / OS budget", memory.gpuKnown && memory.gpuBudget ? memoryText(memory.gpuUsage) + " / " + memoryText(memory.gpuBudget) : "Unknown"},
        {"VRAM available headroom",memory.gpuKnown ? memoryText(memory.availableVram) : "Unknown"},
        {"Dedicated GPU capacity",memory.gpuCapacity ? memoryText(memory.gpuCapacity) : "Unknown"},
        {"Peak sampled GPU usage",memoryText(live.peakGpuUsage)},
        {"Shared GPU usage / budget",memory.sharedGpuKnown ? memoryText(memory.sharedGpuUsage) + " / " + memoryText(memory.sharedGpuBudget) : "Unknown"},
        {"Resident texture payload (estimate)",memoryText(snapshot.textureBytes)},
        {"Accessor buffer payload",memoryText(snapshot.accessorBytes)},
        {"Effective IBL payload (estimate)",RenderQuality::allowIbl() ? memoryText(RenderQuality::effective().estimatedIblBytes()) : "IBL disabled"},
        {"Viewport FBO + post-AA targets (estimate)",memoryText(buffers.viewportTargetBytes)},
        {"System physical RAM used / total",memoryText(memory.totalRam-memory.availableRam) + " / " + memoryText(memory.totalRam)},
        {"System available RAM",memoryText(memory.availableRam)},
        {"System commit used / limit",memoryText(memory.commitLimit-memory.commitAvailable) + " / " + memoryText(memory.commitLimit)},
        {"Process working set / peak",memoryText(memory.workingSet) + " / " + memoryText(memory.peakWorkingSet)},
        {"Process private commit / peak",memoryText(memory.privateCommit) + " / " + memoryText(memory.peakPrivateCommit)},
        {"CPU frame (rolling 240 frames)",cpu},{"GPU render / refresh",gpu},{"Process CPU",process},
        {"Scene rendering",live.lastFrameRendered ? "Redrawn" : "Cached / idle"},
        {"Quality preset",RenderQuality::profileName(RenderQuality::selectedProfile())},
        {"Adaptive state",RenderQuality::autoStatus()},
        {"Image loading",core::loading::state.imagesOmitted ? "Intentionally omitted (material values only)" : "Enabled"},
        {"Loading progress UI ms",std::to_string(core::loading::state.uiMilliseconds)},
        {"Last load wall ms",std::to_string(core::loading::state.wallMilliseconds)},
        {"Last open to rendered frame ms",std::to_string(core::loading::state.firstModelFrameMilliseconds)},
        {"Adaptive frame budget","33 ms target; step up <28 ms / step down >50 ms"},
        {"Texture load maximum",core::loading::state.textureLimit ? std::to_string(core::loading::state.textureLimit.load()) : "Unlimited"},
        {"Texture budget / planned payload",memoryText(core::loading::state.textureBudget) + " / " + memoryText(core::loading::state.estimatedTextureBytes)},
        {"Textures reduced / allocation fallbacks",std::to_string(core::loading::state.reductions.load()) + " / " + std::to_string(core::loading::state.allocationFailures.load())},
        {"Import / upload / validation ms",std::to_string(startup.loadDecodeMs) + " / " + std::to_string(startup.rendererUploadMs) + " / " + std::to_string(startup.validationMs)}
    };
    {
        std::lock_guard lock(core::loading::state.mutex);
        for (int i = 0; i < core::loading::StageCount; ++i) runtime.push_back({std::string("Last stage CPU ms: ") + core::loading::names[i],std::to_string(core::loading::state.milliseconds[i])});
    }

    // Action buttons. Copy and display are both backed by the same snapshot.
    if (ImGui::Button(ICON_MDI_REFRESH " Refresh")) {
        invalidate();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy to Clipboard")) {
        std::string text = snapshot.clipboard;
        for (const auto& row : runtime) text += row.label + ": " + row.value + "\n";
        ImGui::SetClipboardText(text.c_str());
    }

    ImGui::Separator();
    ImGui::Text("CPU %.2f ms   GPU %.2f ms   %.1f FPS   Process %.1f%%",live.cpuMs,live.gpuMs,live.fps,live.cpuPercent);

    if (ImGui::BeginTabBar("StatisticsSections")) {
    const bool overview = ImGui::BeginTabItem("Overview");
    if (overview) {
    ImGui::BeginChild("StatisticsOverview",ImVec2(0,0),false);
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

        for (const auto& row : runtime) addRow(row);
        for (const Row& row : snapshot.rows) {
            addRow(row);
        }

        ImGui::EndTable();
    }
    ImGui::TextWrapped("GPU payload figures exclude driver padding/caches. Mesh rows deduplicate base accessors within each mesh; shared accessors can appear in multiple rows. Generated tangents, morph/skinning buffers and additional renderer targets are not attributed per mesh.");
    ImGui::EndChild();
    ImGui::EndTabItem();
    }
    auto resources = [](const char* title, const std::vector<Row>& rows, bool textures) {
        ImGui::TextWrapped("Estimated uploaded payload; excludes driver padding. Shared resources are counted once per image / once within each mesh.");
        if (ImGui::BeginTable(title,textures ? 6 : 2,ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,ImVec2(0,ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("Resource",ImGuiTableColumnFlags_WidthFixed,180);
            ImGui::TableSetupColumn("GPU payload",ImGuiTableColumnFlags_WidthFixed,110);
            if (textures) {
                ImGui::TableSetupColumn("GPU dimensions",ImGuiTableColumnFlags_WidthFixed,120);
                ImGui::TableSetupColumn("Mips",ImGuiTableColumnFlags_WidthFixed,45);
                ImGui::TableSetupColumn("Decoded RAM",ImGuiTableColumnFlags_WidthFixed,110);
                ImGui::TableSetupColumn("Encoded RAM",ImGuiTableColumnFlags_WidthFixed,110);
            }
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper; clipper.Begin(static_cast<int>(rows.size()));
            while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(rows[i].label.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",rows[i].label.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(rows[i].gpu ? memoryText(rows[i].gpu).c_str() : "Not resident");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",rows[i].value.c_str());
                if (textures) {
                    ImGui::TableNextColumn(); ImGui::Text("%u x %u",rows[i].width,rows[i].height);
                    ImGui::TableNextColumn(); ImGui::Text("%u",rows[i].levels);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(memoryText(rows[i].cpu).c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(memoryText(rows[i].encoded).c_str());
                }
            }
            ImGui::EndTable();
        }
    };
    const bool textures = ImGui::BeginTabItem("Textures"); UiProbe::item("statistics.tab.textures",0);
    if (textures) { resources("Textures and images",snapshot.textures,true); ImGui::EndTabItem(); }
    const bool meshes = ImGui::BeginTabItem("Meshes"); UiProbe::item("statistics.tab.meshes",0);
    if (meshes) { resources("Mesh GPU accessor payload",snapshot.meshMemory,false); ImGui::EndTabItem(); }
    ImGui::EndTabBar();
    }
}

} // namespace ux3d::imgui
