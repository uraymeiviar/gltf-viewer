// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include <imgui_internal.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <ImGuizmo.h>
#include <portable-file-dialogs.h>

#include "imgui/GladGL.h"
#include "imgui/Framebuffer.h"
#include "imgui/OrbitCamera.h"
#include "imgui/GridRenderer.h"
#include "imgui/StdFileSystem.h"

#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/Utils.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Mesh.h"
#include "gltf/Material.h"
#include "gltf/ExportOptions.h"
#include "gltf/ImportOptions.h"

#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/editor/EditorRenderLayer.h"

#include "core/Logger.h"

#include "imgui/MaterialInspector.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/ElementInspector.h"
#include "imgui/AnimationPlayer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/BoundingBoxRenderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <utility>

static void saveBmp(const char* filename, int width, int height, const uint8_t* rgba) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) return;
    uint32_t rowSize = ((width * 3 + 3) / 4) * 4;
    uint32_t imageSize = rowSize * height;
    uint32_t fileSize = 54 + imageSize;
    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(width), (uint8_t)(width >> 8), (uint8_t)(width >> 16), (uint8_t)(width >> 24),
        (uint8_t)(height), (uint8_t)(height >> 8), (uint8_t)(height >> 16), (uint8_t)(height >> 24),
        1, 0, 24, 0,
        0, 0, 0, 0,
        (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    out.write(reinterpret_cast<const char*>(header), 54);
    std::vector<uint8_t> row(rowSize, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            row[x * 3 + 0] = rgba[(y * width + x) * 4 + 2]; // B
            row[x * 3 + 1] = rgba[(y * width + x) * 4 + 1]; // G
            row[x * 3 + 2] = rgba[(y * width + x) * 4 + 0]; // R
        }
        out.write(reinterpret_cast<const char*>(row.data()), rowSize);
    }
}

// Log console message history
struct LogEntry {
    std::string text;
    ImVec4 color;
};

static std::vector<LogEntry> s_consoleLogs;

static void logMessage(const std::string& msg, ImVec4 color = ImVec4(0.85f, 0.85f, 0.88f, 1.0f))
{
    s_consoleLogs.push_back({ msg, color });
    std::fprintf(stderr, "%s\n", msg.c_str());
    std::fflush(stderr);
}

class ConsoleLogCallback : public ux3d::core::LogCallback {
public:
    ConsoleLogCallback() : LogCallback(ux3d::core::LogLevel::All) {}
    void log(ux3d::core::LogLevel level, const char* message) override {
        if (!message) return;
        ImVec4 color(0.85f, 0.85f, 0.88f, 1.0f);
        if (level == ux3d::core::LogLevel::Warning) {
            color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        } else if (level == ux3d::core::LogLevel::Error || level == ux3d::core::LogLevel::Fatal) {
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        } else if (level == ux3d::core::LogLevel::Info) {
            color = ImVec4(0.5f, 0.85f, 1.0f, 1.0f);
        }
        logMessage(message, color);
    }
};

static void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static bool s_scrollToSelectedNode = false;

static bool isAncestorOf(const ux3d::gltf::Node* parent, const ux3d::gltf::Node* target)
{
    if (parent == nullptr || target == nullptr) return false;
    for (const auto* child : parent->getChildren()) {
        if (child == target || isAncestorOf(child, target)) return true;
    }
    return false;
}

static ux3d::gltf::Extends getNodeExtends(ux3d::gltf::Node* node,
                                          const ux3d::gltf::Gltf* gltfModel,
                                          const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (node == nullptr || gltfModel == nullptr) {
        return ux3d::gltf::Extends();
    }
    ux3d::math::dmat4 parentTransform = (gltfLayer != nullptr)
        ? ux3d::math::dmat4(gltfLayer->getParentWorldTransform(node))
        : ux3d::math::identity<ux3d::math::dmat4>();
    return ux3d::gltf::utils::getExtends({ node }, gltfModel, parentTransform);
}

// Focus and look at node with bounding box filling ~50% of the screen area
static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::imgui::OrbitCamera& camera,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              bool& sceneDirty)
{
    if (node == nullptr) return;
    ux3d::gltf::Extends ext = getNodeExtends(node, &gltfModel, gltfLayer);
    glm::vec3 minB(ext.min.x, ext.min.y, ext.min.z);
    glm::vec3 maxB(ext.max.x, ext.max.y, ext.max.z);
    glm::vec3 center = (minB + maxB) * 0.5f;
    glm::vec3 size = maxB - minB;
    float maxDim = std::max({ size.x, size.y, size.z });

    if (maxDim <= 0.0001f || std::isinf(maxDim) || std::isnan(maxDim)) {
        glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
        center = glm::vec3(nodeWorld[3]);
        maxDim = 0.5f;
    }

    float fovRad = glm::radians(camera.getFoVY());
    float dimFactor = std::tan(fovRad * 0.5f) * std::min(1.0f, camera.getAspectRatio());
    if (dimFactor < 0.01f) dimFactor = 0.4142f;

    // To fill ~50% of screen: maxDim / (2 * distance * tan(fov/2)) = 0.5 => distance = maxDim / dimFactor
    float distance = maxDim / dimFactor;
    distance = std::clamp(distance, 0.01f, 50000.0f);

    camera.setTarget(center);
    camera.setDistance(distance);
    antiAliasing.resetHistory();
    sceneDirty = true;
}

// Recursive scene hierarchy node renderer with single-click focus/orbit
static void renderNodeHierarchy(ux3d::gltf::Node* node,
                                ux3d::imgui::SelectionState& selection,
                                ux3d::gltf::Gltf& gltfModel,
                                ux3d::renderer::GltfRenderLayer* gltfLayer,
                                ux3d::imgui::OrbitCamera& camera,
                                ux3d::imgui::AntiAliasing& antiAliasing,
                                bool& sceneDirty)
{
    if (!node) return;

    // Auto-expand parents if selected node is a descendant
    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
        if (isAncestorOf(node, selection.selectedNode)) {
            ImGui::SetNextItemOpen(true);
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode == node) {
        flags |= ImGuiTreeNodeFlags_Selected;
        if (s_scrollToSelectedNode) {
            ImGui::SetScrollHereY(0.5f);
            s_scrollToSelectedNode = false;
        }
    }
    if (node->getChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    std::string name = node->hasName() ? node->getName().c_str() : ("Node " + std::to_string(node->m_ID));
    std::string tag = "";
    if (node->getMesh() != nullptr) tag += " [Mesh]";
    if (node->getCamera() != nullptr) tag += " [Camera]";
    if (node->getLight() != nullptr) tag += " [Light]";
    std::string label = name + tag;

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, camera, antiAliasing, sceneDirty);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, camera, antiAliasing, sceneDirty);
    }

    if (open && !node->getChildren().empty()) {
        for (auto* child : node->getChildren()) {
            renderNodeHierarchy(child, selection, gltfModel, gltfLayer, camera, antiAliasing, sceneDirty);
        }
        ImGui::TreePop();
    }
}

// Recent Files Persistence
static std::vector<std::string> s_recentFiles;

static std::string getRecentFilesConfigPath() {
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Gestaltor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "recent_files.txt").string();
    }
    return "recent_files.txt";
}

static std::vector<std::string> loadRecentFiles() {
    std::vector<std::string> files;
    std::ifstream in(getRecentFilesConfigPath());
    if (!in.is_open()) return files;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        while (!line.empty() && (line.front() == ' ')) line.erase(line.begin());
        if (!line.empty()) {
            files.push_back(line);
        }
    }
    return files;
}

static void saveRecentFiles(const std::vector<std::string>& files) {
    std::ofstream out(getRecentFilesConfigPath());
    if (!out.is_open()) return;
    for (const auto& path : files) {
        out << path << "\n";
    }
}

static void addRecentFile(std::vector<std::string>& files, const std::string& path) {
    if (path.empty()) return;
    std::string normPath = path;
    for (char& c : normPath) {
        if (c == '\\') c = '/';
    }
    files.erase(std::remove_if(files.begin(), files.end(), [&](const std::string& f) {
        return f == normPath;
    }), files.end());

    files.insert(files.begin(), normPath);
    if (files.size() > 10) {
        files.resize(10);
    }
    saveRecentFiles(files);
}

static void updateWindowTitle(GLFWwindow* window, const std::string& currentFilePath) {
    if (window == nullptr) return;
    std::string title = "Gestaltor 2.2.7 - ";
    if (currentFilePath.empty()) {
        title += "untitled.glb";
    } else {
        std::filesystem::path p(currentFilePath);
        title += p.filename().string();
    }
    glfwSetWindowTitle(window, title.c_str());
}

// glTF loader helper
static bool loadGltfModel(const std::string& filePath,
                          ux3d::gltf::Gltf& gltfModel,
                          ux3d::gltf::Scene*& activeScene,
                          ux3d::imgui::SelectionState& selection,
                          std::string& currentFilePath,
                          ux3d::imgui::OrbitCamera& camera,
                          ux3d::renderer::GltfRenderLayer* gltfLayer,
                          ux3d::imgui::AnimationPlayer& animPlayer,
                          bool& sceneDirty)
{
    logMessage("Opening file: " + filePath);
    ux3d::gltf::ImportOptions importOptions;
    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());

    gltfModel.reset();
    auto result = ux3d::gltf::GltfParsing::fromFile(gltfModel, file, importOptions);
    if (result.derived == ux3d::gltf::GltfFromFileResult::Success) {
        currentFilePath = filePath;
        activeScene = gltfModel.getDefaultScene();
        if (!activeScene && !gltfModel.getElements<ux3d::gltf::Scene>().empty()) {
            activeScene = &gltfModel.getElements<ux3d::gltf::Scene>().front();
        }
        selection = ux3d::imgui::SelectionState();

        if (gltfLayer != nullptr) {
            gltfLayer->updateGltf(gltfModel);
            if (activeScene != nullptr) {
                gltfLayer->updateScene(gltfModel, *activeScene);
                gltfLayer->applyAnimationTime(0.0f);
            }
        }

        animPlayer.setModel(gltfModel, gltfLayer);

        size_t nodeCount = gltfModel.getElements<ux3d::gltf::Node>().size();
        size_t meshCount = gltfModel.getElements<ux3d::gltf::Mesh>().size();
        size_t matCount = gltfModel.getElements<ux3d::gltf::Material>().size();
        char infoBuf[256];
        std::snprintf(infoBuf, sizeof(infoBuf), "Model Stats: %zu nodes, %zu meshes, %zu materials", nodeCount, meshCount, matCount);
        logMessage(infoBuf, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));

        if (activeScene != nullptr && !activeScene->getNodes().empty()) {
            ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
            camera.fitToBounds(glm::vec3(extends.min.x, extends.min.y, extends.min.z),
                               glm::vec3(extends.max.x, extends.max.y, extends.max.z));
            char boundsBuf[256];
            std::snprintf(boundsBuf, sizeof(boundsBuf), "Scene Bounds: [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]",
                          extends.min.x, extends.min.y, extends.min.z,
                          extends.max.x, extends.max.y, extends.max.z);
            logMessage(boundsBuf, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
        } else {
            camera.reset();
        }

        sceneDirty = true;
        logMessage("Loaded successfully: " + filePath, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        if (filePath != "resources/sampleGltf/HelloCube.glb" && !filePath.empty()) {
            addRecentFile(s_recentFiles, filePath);
        }
        return true;
    } else {
        logMessage("Failed to load: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }
}

// glTF creator helper
[[maybe_unused]] static void newEmptyGltf(ux3d::gltf::Gltf& gltfModel,
                         ux3d::gltf::Scene*& activeScene,
                         ux3d::imgui::SelectionState& selection,
                         std::string& currentFilePath,
                         ux3d::imgui::OrbitCamera& camera,
                         ux3d::renderer::GltfRenderLayer* gltfLayer,
                         ux3d::imgui::AnimationPlayer& animPlayer,
                         bool& sceneDirty)
{
    gltfModel.reset();
    auto* scene = gltfModel.create<ux3d::gltf::Scene>();
    if (scene != nullptr) {
        scene->setName("Scene");
        gltfModel.setDefaultScene(*scene);
    }
    activeScene = scene;
    selection = ux3d::imgui::SelectionState();
    currentFilePath = "untitled.glb";
    if (gltfLayer != nullptr) {
        gltfLayer->updateGltf(gltfModel);
        if (activeScene != nullptr) {
            gltfLayer->updateScene(gltfModel, *activeScene);
        }
    }
    animPlayer.reset();
    animPlayer.setModel(gltfModel, gltfLayer);
    camera.reset();
    sceneDirty = true;
    logMessage("Created new empty glTF", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
}

// glTF saver helper
[[maybe_unused]] static bool saveGltfModel(const std::string& filePath,
                          const ux3d::gltf::Gltf& gltfModel,
                          std::string& currentFilePath)
{
    logMessage("Saving file: " + filePath);
    ux3d::gltf::ExportOptions exportOptions;
    bool isGlb = (filePath.size() >= 4 && filePath.substr(filePath.size() - 4) == ".glb");
    exportOptions.fileContainer = isGlb ? ux3d::gltf::FileContainer::Glb : ux3d::gltf::FileContainer::Gltf;
    exportOptions.conversionUsage = ux3d::gltf::ConversionUsage::ValidGltf;

    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());
    auto result = ux3d::gltf::GltfParsing::toFile(gltfModel, file, exportOptions);
    if (result.derived == ux3d::gltf::GltfToFileResult::Success) {
        currentFilePath = filePath;
        logMessage("Saved successfully: " + filePath, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        addRecentFile(s_recentFiles, filePath);
        return true;
    } else {
        logMessage("Failed to save: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }
}

struct PickCandidate {
    ux3d::gltf::Node* node = nullptr;
    float distance = 0.0f;
    float volume = 0.0f;
};

static bool rayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                              const glm::vec3& boxMin, const glm::vec3& boxMax, float& tHit)
{
    float tmin = -1e30f;
    float tmax = 1e30f;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDir[i]) < 1e-7f) {
            if (rayOrigin[i] < boxMin[i] || rayOrigin[i] > boxMax[i]) return false;
        } else {
            float ood = 1.0f / rayDir[i];
            float t1 = (boxMin[i] - rayOrigin[i]) * ood;
            float t2 = (boxMax[i] - rayOrigin[i]) * ood;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    if (tmax < 0.0f) return false;
    tHit = (tmin >= 0.0f) ? tmin : tmax;
    return true;
}

static ux3d::gltf::Node* pickNodeFromRay(const glm::vec3& rayOrigin,
                                         const glm::vec3& rayDir,
                                         const ux3d::gltf::Scene* activeScene,
                                         ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (activeScene == nullptr) return nullptr;

    std::unordered_map<ux3d::gltf::Node*, PickCandidate> bestHits;

    std::function<void(ux3d::gltf::Node*)> checkNodeRecursive;
    checkNodeRecursive = [&](ux3d::gltf::Node* node) {
        if (node == nullptr || !node->getVisible()) return;

        const auto* mesh = node->getMesh();
        if (mesh != nullptr && mesh->getVisible()) {
            glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
            glm::mat4 invWorld = glm::inverse(nodeWorld);

            glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
            glm::vec3 localDir = glm::normalize(glm::vec3(invWorld * glm::vec4(rayDir, 0.0f)));

            for (const auto& prim : mesh->getPrimitives()) {
                if (!prim.getVisible()) continue;
                for (const auto& attr : prim.getAttributes()) {
                    if (attr.getName() == "POSITION") {
                        const auto* acc = attr.getAccessor();
                        if (acc != nullptr && acc->getMin().size() >= 3 && acc->getMax().size() >= 3) {
                            glm::vec3 bMin(acc->getMin()[0], acc->getMin()[1], acc->getMin()[2]);
                            glm::vec3 bMax(acc->getMax()[0], acc->getMax()[1], acc->getMax()[2]);

                            // If ray origin is inside this box (e.g. outer cockpit closure box), ignore it
                            if (localOrigin.x >= bMin.x && localOrigin.x <= bMax.x &&
                                localOrigin.y >= bMin.y && localOrigin.y <= bMax.y &&
                                localOrigin.z >= bMin.z && localOrigin.z <= bMax.z) {
                                continue;
                            }

                            float tHit = 0.0f;
                            if (rayIntersectsAABB(localOrigin, localDir, bMin, bMax, tHit)) {
                                if (tHit >= 0.0f) {
                                    glm::vec3 localHitPt = localOrigin + localDir * tHit;
                                    glm::vec3 worldHitPt = glm::vec3(nodeWorld * glm::vec4(localHitPt, 1.0f));
                                    float worldDist = glm::length(worldHitPt - rayOrigin);
                                    glm::vec3 sz = bMax - bMin;
                                    float vol = std::abs(sz.x * sz.y * sz.z);
                                    auto it = bestHits.find(node);
                                    if (it == bestHits.end()) {
                                        bestHits[node] = { node, worldDist, vol };
                                    } else {
                                        if (worldDist < it->second.distance) {
                                            it->second.distance = worldDist;
                                        }
                                        it->second.volume += vol;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (auto* child : node->getChildren()) {
            checkNodeRecursive(child);
        }
    };

    for (auto* root : activeScene->getNodes()) {
        checkNodeRecursive(root);
    }

    if (bestHits.empty()) return nullptr;

    std::vector<PickCandidate> candidates;
    candidates.reserve(bestHits.size());
    for (auto& [n, cand] : bestHits) {
        candidates.push_back(cand);
    }

    std::sort(candidates.begin(), candidates.end(), [](const PickCandidate& a, const PickCandidate& b) {
        float diff = std::abs(a.distance - b.distance);
        float threshold = std::max(0.015f, 0.03f * std::min(a.distance, b.distance));
        if (diff < threshold) {
            return a.volume < b.volume;
        }
        return a.distance < b.distance;
    });

    return candidates.front().node;
}

static void drawNodeCoordinateGizmo(ImDrawList* drawList,
                                    const glm::mat4& nodeWorld,
                                    const glm::mat4& viewProj,
                                    const ImVec2& vpPos,
                                    const ImVec2& vpSize,
                                    const glm::vec3& camEye)
{
    glm::vec3 origin = glm::vec3(nodeWorld[3]);
    float distToCam = glm::length(origin - camEye);
    float axisWorldLen = std::max(0.002f, distToCam * 0.085f);

    glm::vec3 xDir = glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
    glm::vec3 yDir = glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
    glm::vec3 zDir = glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[2])) : glm::vec3(0, 0, 1);

    glm::vec3 xEnd = origin + xDir * axisWorldLen;
    glm::vec3 yEnd = origin + yDir * axisWorldLen;
    glm::vec3 zEnd = origin + zDir * axisWorldLen;

    auto project = [&](const glm::vec3& p, ImVec2& outPt, float& outZ) -> bool {
        glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        outPt.x = vpPos.x + (ndc.x * 0.5f + 0.5f) * vpSize.x;
        outPt.y = vpPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpSize.y;
        outZ = ndc.z;
        return true;
    };

    ImVec2 ptOrigin;
    float zOrigin;
    if (!project(origin, ptOrigin, zOrigin)) return;

    struct AxisItem {
        ImVec2 endPt;
        ImU32 color;
        float z;
    };
    std::vector<AxisItem> axes;

    ImVec2 ptX, ptY, ptZ;
    float zX, zY, zZ;
    if (project(xEnd, ptX, zX)) axes.push_back({ ptX, IM_COL32(235, 45, 45, 255), zX });
    if (project(yEnd, ptY, zY)) axes.push_back({ ptY, IM_COL32(45, 215, 45, 255), zY });
    if (project(zEnd, ptZ, zZ)) axes.push_back({ ptZ, IM_COL32(45, 120, 245, 255), zZ });

    std::sort(axes.begin(), axes.end(), [](const AxisItem& a, const AxisItem& b) {
        return a.z > b.z;
    });

    for (const auto& axis : axes) {
        drawList->AddLine(ptOrigin, axis.endPt, axis.color, 3.0f);
        glm::vec2 dir(axis.endPt.x - ptOrigin.x, axis.endPt.y - ptOrigin.y);
        float len = glm::length(dir);
        if (len > 5.0f) {
            dir /= len;
            glm::vec2 perp(-dir.y, dir.x);
            float arrowSize = 11.0f;
            ImVec2 tip = axis.endPt;
            ImVec2 p1(axis.endPt.x - dir.x * arrowSize + perp.x * (arrowSize * 0.45f),
                      axis.endPt.y - dir.y * arrowSize + perp.y * (arrowSize * 0.45f));
            ImVec2 p2(axis.endPt.x - dir.x * arrowSize - perp.x * (arrowSize * 0.45f),
                      axis.endPt.y - dir.y * arrowSize - perp.y * (arrowSize * 0.45f));
            drawList->AddTriangleFilled(tip, p1, p2, axis.color);
        }
    }

    drawList->AddCircleFilled(ptOrigin, 4.5f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircle(ptOrigin, 4.5f, IM_COL32(30, 30, 30, 255), 0, 1.5f);
}

static ux3d::filesystem::Path resolveRendererResourcesDir()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto exeDir = fs.getExecutableDirPath();
    if (fs.isFile(exeDir / "resources" / "shaders" / "primitive.vert")) {
        return exeDir / "resources";
    }
    if (fs.isFile(exeDir / "OpenGL-Renderer" / "resources" / "shaders" / "primitive.vert")) {
        return exeDir / "OpenGL-Renderer" / "resources";
    }

    auto compileTimePath = fs.toPath(UX3D_RENDERER_RESOURCES_DIR);
    if (fs.isFile(compileTimePath / "shaders" / "primitive.vert")) {
        return compileTimePath;
    }

    auto cwdPath = fs.toPath(".");
    if (fs.isFile(cwdPath / "resources" / "shaders" / "primitive.vert")) {
        return cwdPath / "resources";
    }
    if (fs.isFile(cwdPath / "OpenGL-Renderer" / "resources" / "shaders" / "primitive.vert")) {
        return cwdPath / "OpenGL-Renderer" / "resources";
    }

    return compileTimePath;
}

static ux3d::filesystem::Path resolveEnvironmentPath(const ux3d::filesystem::Path& rendererRes)
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto env1 = rendererRes / "environments" / "footprint_court.hdr";
    if (fs.isFile(env1)) return env1;

    auto exeDir = fs.getExecutableDirPath();
    auto env2 = exeDir / "resources" / "environments" / "footprint_court.hdr";
    if (fs.isFile(env2)) return env2;

    auto env3 = fs.toPath("resources/environments/footprint_court.hdr");
    if (fs.isFile(env3)) return env3;

    return env1;
}

static std::string resolveSampleModelPath()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto exeDir = fs.getExecutableDirPath();
    auto p1 = exeDir / "resources" / "sampleGltf" / "HelloCube.glb";
    if (fs.isFile(p1)) return p1.toString().c_str();

    auto p2 = fs.toPath("resources/sampleGltf/HelloCube.glb");
    if (fs.isFile(p2)) return p2.toString().c_str();

    return "resources/sampleGltf/HelloCube.glb";
}

static ux3d::renderer::RenderMode getRenderModeFromIndex(int idx)
{
    switch (idx) {
        case 1: return ux3d::renderer::RenderMode::Wireframe;
        case 2: return ux3d::renderer::RenderMode::NormalTexture;
        case 3: return ux3d::renderer::RenderMode::ShadingNormal;
        case 4: return ux3d::renderer::RenderMode::NormalGeometry;
        case 5: return ux3d::renderer::RenderMode::Tangent;
        case 6: return ux3d::renderer::RenderMode::Bitangent;
        case 7: return ux3d::renderer::RenderMode::BaseColor;
        case 8: return ux3d::renderer::RenderMode::Metallic;
        case 9: return ux3d::renderer::RenderMode::Roughness;
        case 10: return ux3d::renderer::RenderMode::MetallicRoughness;
        case 11: return ux3d::renderer::RenderMode::Occlusion;
        case 12: return ux3d::renderer::RenderMode::Emissive;
        case 13: return ux3d::renderer::RenderMode::Alpha;
        case 14: return ux3d::renderer::RenderMode::TexCoord_0;
        case 15: return ux3d::renderer::RenderMode::TexCoord_1;
        case 16: return ux3d::renderer::RenderMode::Clearcoat;
        case 17: return ux3d::renderer::RenderMode::Sheen;
        case 18: return ux3d::renderer::RenderMode::Specular;
        case 19: return ux3d::renderer::RenderMode::TransmissionVolume;
        case 20: return ux3d::renderer::RenderMode::Iridescence;
        default: return ux3d::renderer::RenderMode::None;
    }
}

static void setupDefaultDockLayout(ImGuiID dockspaceId, ImVec2 size, bool force = false)
{
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
    if (node == nullptr || !node->IsSplitNode() || force) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, size);

        ImGuiID dock_main_id = dockspaceId;
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
        ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.45f, nullptr, &dock_left);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.26f, nullptr, &dock_main_id);

        ImGui::DockBuilderDockWindow("3D Viewport", dock_main_id);
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Resources", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);

        ImGui::DockBuilderDockWindow("Viewport Settings", dock_bottom);
        ImGui::DockBuilderDockWindow("Logger", dock_bottom);
        ImGui::DockBuilderDockWindow("Statistics", dock_bottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // Target OpenGL 4.1 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create GLFW window (starts maximized)
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Gestaltor - glTF 2.0 Visual Editor", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync

    // Initialize core logger callback
    ConsoleLogCallback consoleLogger;
    ux3d::core::addLoggerCallback(&consoleLogger);

    // Initialize GLAD
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        ux3d::core::removeLoggerCallback(&consoleLogger);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    logMessage(std::string("OpenGL Version: ") + reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    logMessage(std::string("Renderer: ") + reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    // Initialize GladGL abstraction
    auto gladGL = std::make_unique<ux3d::renderer::GladGL>();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup ImGui Dark Theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.32f, 0.44f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.40f, 0.58f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.34f, 0.48f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.44f, 0.65f, 1.0f);

    // Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Initialize 3D Viewport components
    ux3d::imgui::Framebuffer sceneFbo;
    ux3d::imgui::OrbitCamera orbitCamera;
    ux3d::imgui::GridRenderer gridRenderer;
    gridRenderer.init();
    ux3d::imgui::AntiAliasing antiAliasing;
    antiAliasing.init();
    ux3d::imgui::BoundingBoxRenderer bboxRenderer;
    bboxRenderer.init();

    ux3d::gltf::Gltf gltfModel;
    ux3d::gltf::Scene* activeScene = nullptr;
    ux3d::imgui::SelectionState selection;
    std::string currentFilePath = "";
    bool isSceneDirty = false;

    ux3d::imgui::ViewportSettingsState viewportSettings;
    ux3d::imgui::AnimationPlayer animationPlayer;

    auto rendererResourcesDir = resolveRendererResourcesDir();
    auto shaderSource = rendererResourcesDir / "shaders";

    auto gltfLayer = std::make_unique<ux3d::renderer::GltfRenderLayer>(gladGL.get(), shaderSource / "ibl_sampler");
    auto commandGenerator = std::make_unique<ux3d::renderer::CommandGenerator>(gladGL.get(), shaderSource);
    auto glRenderer = std::make_unique<ux3d::renderer::Renderer>(gladGL.get());
    glRenderer->init(ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));

    ux3d::renderer::Camera rendererCamera(gladGL.get());

    auto envPath = resolveEnvironmentPath(rendererResourcesDir);
    if (ux3d::imgui::StdFileSystem::instance().isFile(envPath)) {
        gltfLayer->setEnvironmentMap(envPath);
        if (gltfLayer->filterEnvironmentMap()) {
            commandGenerator->setIBLEnabled(true);
            logMessage("Loaded default environment map: footprint_court.hdr");
        }
    }

    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
    commandGenerator->setExposure(1.0f);
    commandGenerator->setPunctualLightsEnabled(true);
    commandGenerator->setEnvironmentMapRotation(0.0f);
    commandGenerator->setEnvironmentMapBlurNormalized(0.0f);
    commandGenerator->setEnvironmentMapIntensity(1.0f);

    gltfLayer->renderSettings.enableSpecularGlossiness = true;
    gltfLayer->renderSettings.enableUnlit = true;
    gltfLayer->renderSettings.enableClearCoat = true;
    gltfLayer->renderSettings.enableSheen = true;
    gltfLayer->renderSettings.enableTransmission = true;
    gltfLayer->renderSettings.enableVolume = true;
    gltfLayer->renderSettings.enableIOR = true;
    gltfLayer->renderSettings.enableSpecular = true;

    std::string modelToLoad = resolveSampleModelPath();
    std::string dumpFramePath = "";
    int dumpFrameCountdown = -1;
    bool exitAfterDump = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-frame" && i + 1 < argc) {
            dumpFramePath = argv[++i];
            dumpFrameCountdown = 8;
        } else if (arg == "--exit-after-dump") {
            exitAfterDump = true;
        } else if (!arg.empty() && arg[0] != '-') {
            modelToLoad = arg;
        }
    }

    s_recentFiles = loadRecentFiles();

    if (ux3d::imgui::StdFileSystem::instance().isFile(ux3d::imgui::StdFileSystem::instance().toFile(modelToLoad.c_str()))) {
        loadGltfModel(modelToLoad, gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
        updateWindowTitle(window, currentFilePath);
    } else {
        updateWindowTitle(window, "untitled.glb");
    }

    // UI state
    bool showDemoWindow = false;
    static bool showInspector = true;
    static bool showHierarchy = true;
    static bool showViewport = true;
    static bool showResources = true;
    static bool showLogger = true;
    static bool showViewportSettings = true;
    static bool showStatistics = true;

    ImGuizmo::OPERATION currentGizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE currentGizmoMode = ImGuizmo::LOCAL;

    double lastFrameTime = glfwGetTime();

    // Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastFrameTime);
        lastFrameTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Update animation player
        animationPlayer.update(deltaTime, gltfLayer.get());

        // Turntable Auto-Rotate
        if (viewportSettings.turntable && !ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            orbitCamera.rotate(viewportSettings.turntableSpeed * deltaTime, 0.0f);
        }

        // Keyboard shortcuts
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOp = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOp = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOp = ImGuizmo::SCALE;
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
                currentGizmoMode = (currentGizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                orbitCamera.reset();
                antiAliasing.resetHistory();
            }

            // Numpad Camera Presets (Blender style)
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1)) {
                orbitCamera.setPresetView(io.KeyCtrl ? ux3d::imgui::PresetView::Back : ux3d::imgui::PresetView::Front);
                antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad3)) {
                orbitCamera.setPresetView(io.KeyCtrl ? ux3d::imgui::PresetView::Left : ux3d::imgui::PresetView::Right);
                antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad7)) {
                orbitCamera.setPresetView(io.KeyCtrl ? ux3d::imgui::PresetView::Bottom : ux3d::imgui::PresetView::Top);
                antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad5)) {
                orbitCamera.toggleProjection();
                antiAliasing.resetHistory();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal)) {
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends({ selection.selectedNode }, &gltfModel);
                    orbitCamera.fitToBounds(glm::vec3(extends.min.x, extends.min.y, extends.min.z),
                                            glm::vec3(extends.max.x, extends.max.y, extends.max.z));
                } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                    ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
                    orbitCamera.fitToBounds(glm::vec3(extends.min.x, extends.min.y, extends.min.z),
                                            glm::vec3(extends.max.x, extends.max.y, extends.max.z));
                }
                antiAliasing.resetHistory();
            }
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // Main DockSpace covering the whole window
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("GestaltorDockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("GestaltorMainDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

        static bool s_firstLayout = true;
        if (s_firstLayout) {
            s_firstLayout = false;
            setupDefaultDockLayout(dockspaceId, mainViewport->WorkSize, false);
        }

        // Global Keyboard Shortcuts (File & View)
        if (!io.WantTextInput && io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_O)) {
                auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                if (!openResult.empty()) {
                    loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                    updateWindowTitle(window, currentFilePath);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_1)) showInspector = !showInspector;
            if (ImGui::IsKeyPressed(ImGuiKey_2)) showHierarchy = !showHierarchy;
            if (ImGui::IsKeyPressed(ImGuiKey_3)) showViewport = !showViewport;
            if (ImGui::IsKeyPressed(ImGuiKey_4)) showResources = !showResources;
            if (ImGui::IsKeyPressed(ImGuiKey_5)) showLogger = !showLogger;
            if (ImGui::IsKeyPressed(ImGuiKey_6)) showViewportSettings = !showViewportSettings;
            if (ImGui::IsKeyPressed(ImGuiKey_7)) showStatistics = !showStatistics;
            if (ImGui::IsKeyPressed(ImGuiKey_L)) {
                showInspector = showHierarchy = showViewport = showResources = showLogger = showViewportSettings = showStatistics = true;
                setupDefaultDockLayout(dockspaceId, mainViewport->WorkSize, true);
            }
        }

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open ...", "Ctrl+O")) {
                    auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                    if (!openResult.empty()) {
                        loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                        updateWindowTitle(window, currentFilePath);
                    }
                }
                if (ImGui::BeginMenu("Open Recent")) {
                    if (s_recentFiles.empty()) {
                        ImGui::MenuItem("(No Recent Files)", nullptr, false, false);
                    } else {
                        std::string fileToOpen = "";
                        for (const auto& recentPath : s_recentFiles) {
                            if (ImGui::MenuItem(recentPath.c_str())) {
                                fileToOpen = recentPath;
                            }
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Clear Menu")) {
                            s_recentFiles.clear();
                            saveRecentFiles(s_recentFiles);
                        }
                        if (!fileToOpen.empty()) {
                            loadGltfModel(fileToOpen, gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                            updateWindowTitle(window, currentFilePath);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Reset Camera", "Home")) {
                    orbitCamera.reset();
                }
                std::string fitLabel = (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) ? "Fit to Selection" : "Fit to Scene";
                if (ImGui::MenuItem(fitLabel.c_str(), "F", false, activeScene != nullptr)) {
                    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                        ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends({ selection.selectedNode }, &gltfModel);
                        orbitCamera.fitToBounds(glm::vec3(extends.min.x, extends.min.y, extends.min.z),
                                                glm::vec3(extends.max.x, extends.max.y, extends.max.z));
                    } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                        ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
                        orbitCamera.fitToBounds(glm::vec3(extends.min.x, extends.min.y, extends.min.z),
                                                glm::vec3(extends.max.x, extends.max.y, extends.max.z));
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Gizmo")) {
                if (ImGui::MenuItem("Translate", "W", currentGizmoOp == ImGuizmo::TRANSLATE)) currentGizmoOp = ImGuizmo::TRANSLATE;
                if (ImGui::MenuItem("Rotate", "E", currentGizmoOp == ImGuizmo::ROTATE)) currentGizmoOp = ImGuizmo::ROTATE;
                if (ImGui::MenuItem("Scale", "R", currentGizmoOp == ImGuizmo::SCALE)) currentGizmoOp = ImGuizmo::SCALE;
                ImGui::Separator();
                if (ImGui::MenuItem("Local Space", nullptr, currentGizmoMode == ImGuizmo::LOCAL)) currentGizmoMode = ImGuizmo::LOCAL;
                if (ImGui::MenuItem("World Space", nullptr, currentGizmoMode == ImGuizmo::WORLD)) currentGizmoMode = ImGuizmo::WORLD;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Inspector", "Ctrl+1", &showInspector);
                ImGui::MenuItem("Show Hierarchy", "Ctrl+2", &showHierarchy);
                ImGui::MenuItem("Show Viewport", "Ctrl+3", &showViewport);
                ImGui::MenuItem("Show Resources", "Ctrl+4", &showResources);
                ImGui::MenuItem("Show Logger", "Ctrl+5", &showLogger);
                ImGui::MenuItem("Show Viewport Settings", "Ctrl+6", &showViewportSettings);
                ImGui::MenuItem("Show Statistics", "Ctrl+7", &showStatistics);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Layout", "Ctrl+L")) {
                    showInspector = showHierarchy = showViewport = showResources = showLogger = showViewportSettings = showStatistics = true;
                    setupDefaultDockLayout(dockspaceId, mainViewport->WorkSize, true);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End(); // End DockSpace

        // -------------------------------------------------------------
        // 3D Viewport Panel
        // -------------------------------------------------------------
        if (showViewport) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("3D Viewport", &showViewport);
            ImGui::PopStyleVar();

        // Animation Player Toolbar at top of viewport
        animationPlayer.renderToolbar(gltfModel, gltfLayer.get(), isSceneDirty);

        // Quick Navigation & Inspection Toolbar Strip
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
        ImGui::BeginGroup();
        if (ImGui::SmallButton("Front")) { orbitCamera.setPresetView(ux3d::imgui::PresetView::Front); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Back")) { orbitCamera.setPresetView(ux3d::imgui::PresetView::Back); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Right")) { orbitCamera.setPresetView(ux3d::imgui::PresetView::Right); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Left")) { orbitCamera.setPresetView(ux3d::imgui::PresetView::Left); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Top")) { orbitCamera.setPresetView(ux3d::imgui::PresetView::Top); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Iso")) { orbitCamera.setPresetView(ux3d::imgui::PresetView::Isometric); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const char* projLabel = orbitCamera.isOrthographic() ? "Ortho" : "Persp";
        if (ImGui::SmallButton(projLabel)) { orbitCamera.toggleProjection(); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton(viewportSettings.turntable ? "[Turntable ON]" : "Turntable")) {
            viewportSettings.turntable = !viewportSettings.turntable;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(viewportSettings.wireframeOverlay ? "[Wire ON]" : "Wire")) {
            viewportSettings.wireframeOverlay = !viewportSettings.wireframeOverlay;
            isSceneDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus (F)")) {
            if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                auto ext = ux3d::gltf::utils::getExtends({ selection.selectedNode }, &gltfModel);
                orbitCamera.fitToBounds(glm::vec3(ext.min.x, ext.min.y, ext.min.z), glm::vec3(ext.max.x, ext.max.y, ext.max.z));
            } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                auto ext = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
                orbitCamera.fitToBounds(glm::vec3(ext.min.x, ext.min.y, ext.min.z), glm::vec3(ext.max.x, ext.max.y, ext.max.z));
            }
            antiAliasing.resetHistory();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(viewportSettings.showSceneBoundingBox ? "[BBox ON]" : "BBox")) {
            viewportSettings.showSceneBoundingBox = !viewportSettings.showSceneBoundingBox;
        }

        if (gltfModel.hasMaterialVariants()) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            auto& variants = gltfModel.getMaterialVariants();
            std::string curVarName = "Default";
            if (gltfLayer && gltfLayer->getMaterialVariant().has_value()) {
                uint32_t activeVarId = gltfLayer->getMaterialVariant().value();
                for (const auto& v : variants) {
                    if (v.m_ID == activeVarId) {
                        curVarName = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                        break;
                    }
                }
            }
            if (ImGui::BeginCombo("##ToolbarVariant", curVarName.c_str())) {
                if (ImGui::Selectable("Default", !gltfLayer || !gltfLayer->getMaterialVariant().has_value())) {
                    if (gltfLayer) {
                        gltfLayer->setMaterialVariant(std::nullopt);
                        isSceneDirty = true;
                    }
                }
                for (const auto& v : variants) {
                    bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == v.m_ID);
                    std::string vLabel = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                    if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                        if (gltfLayer) {
                            gltfLayer->setMaterialVariant(v.m_ID);
                            isSceneDirty = true;
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::EndGroup();
        ImGui::PopStyleVar(2);

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 viewportPos = ImVec2(windowPos.x + contentMin.x, windowPos.y + contentMin.y);

        int vpW = static_cast<int>(viewportSize.x);
        int vpH = static_cast<int>(viewportSize.y);
        if (vpW < 1) vpW = 1;
        if (vpH < 1) vpH = 1;

        auto aaMode = static_cast<ux3d::imgui::AntiAliasingMode>(viewportSettings.antiAliasingIdx);
        int desiredSamples = ux3d::imgui::AntiAliasing::getSampleCount(aaMode);

        if (sceneFbo.getWidth() != vpW || sceneFbo.getHeight() != vpH || sceneFbo.getSamples() != desiredSamples) {
            sceneFbo.resize(vpW, vpH, desiredSamples);
            orbitCamera.setViewportSize(static_cast<float>(vpW), static_cast<float>(vpH));
            if (gltfLayer != nullptr) {
                gltfLayer->getResourceManager().createFramebufferTexture(sceneFbo.getColorTexture());
                gltfLayer->getResourceManager().setBlitFramebufferFunction([] {}, ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
                gltfLayer->getResourceManager().setTransmissiveTextureFunction([] {});
            }
            antiAliasing.resetHistory();
        }

        // Render to offscreen Framebuffer
        sceneFbo.bind();
        glViewport(0, 0, vpW, vpH);
        if (viewportSettings.showBackground) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else {
            glClearColor(viewportSettings.clearColor.r, viewportSettings.clearColor.g, viewportSettings.clearColor.b, viewportSettings.clearColor.a);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = orbitCamera.getViewMatrix();
        glm::mat4 proj = orbitCamera.getProjectionMatrix();

        // Subpixel jitter for TAA
        glm::vec2 jitter = antiAliasing.getJitter(aaMode, vpW, vpH);
        glm::mat4 jitterMat = (aaMode == ux3d::imgui::AntiAliasingMode::TAA)
            ? glm::translate(glm::mat4(1.0f), glm::vec3(2.0f * jitter.x, 2.0f * jitter.y, 0.0f))
            : glm::mat4(1.0f);
        glm::mat4 jitteredProj = jitterMat * proj;
        glm::mat4 viewProj = jitteredProj * view;

        // 1. Render glTF scene & background skybox
        if (gltfLayer != nullptr && glRenderer != nullptr && commandGenerator != nullptr) {
            if (isSceneDirty) {
                gltfLayer->updateGltf(gltfModel);
                if (activeScene != nullptr) {
                    gltfLayer->updateScene(gltfModel, *activeScene);
                }
                isSceneDirty = false;
            }

            gltfLayer->enableCubeMap(viewportSettings.showBackground);

            rendererCamera.setViewportResolution(vpW, vpH);
            glm::mat4 cameraWorld = glm::inverse(view);
            rendererCamera.setTransform(ux3d::math::mat4x4(cameraWorld));
            if (orbitCamera.isOrthographic()) {
                float halfH = orbitCamera.getDistance() * std::tan(glm::radians(orbitCamera.getFoVY() * 0.5f));
                float halfW = halfH * orbitCamera.getAspectRatio();
                rendererCamera.setOrthographic(halfW, halfH, orbitCamera.getZNear(), orbitCamera.getZFar());
            } else {
                rendererCamera.setPerspective(orbitCamera.getAspectRatio(), glm::radians(orbitCamera.getFoVY()), orbitCamera.getZNear(), orbitCamera.getZFar());
            }
            rendererCamera.setProjection(ux3d::math::mat4x4(jitteredProj));
            gltfLayer->setActiveCamera(&rendererCamera);

            auto mode = getRenderModeFromIndex(viewportSettings.renderModeIdx);

            // Apply Face Culling override and Debug Mode cleanly via materialMapping
            for (auto& [gltfMat, renMat] : gltfLayer->m_materialMapping.map) {
                if (renMat != nullptr) {
                    if (viewportSettings.forceDoubleSided) {
                        renMat->isDoubleSided = true;
                    } else {
                        renMat->isDoubleSided = gltfMat ? gltfMat->isDoubleSidedOrDefault() : false;
                    }
                    renMat->defines.define("DEBUG", static_cast<int>(mode));
                }
            }

            // Apply Winding Order override and Render Mode cleanly via meshMapping
            // Inverted winding is default as requested by user (so cockpit and exterior hull normals are correct out of the box)
            // Ticking "Invert Winding Order" in Viewport Settings flips to standard glTF CounterClockwise winding
            for (auto& [gltfTuple, renMesh] : gltfLayer->m_meshMapping.map) {
                if (renMesh != nullptr) {
                    auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;
                    auto nodeIt = gltfLayer->m_transformHierarchyMapping.map.find(gltfNode);
                    float det = 1.0f;
                    if (nodeIt != gltfLayer->m_transformHierarchyMapping.map.end() && nodeIt->second != nullptr) {
                        det = ux3d::math::determinant(nodeIt->second->getWorldTransform());
                    }
                    bool baseClockwise = (det < 0.0f);
                    bool useInvertedWinding = !viewportSettings.invertWinding;
                    bool isClockwise = baseClockwise ^ useInvertedWinding;
                    renMesh->frontFace = isClockwise ? ux3d::renderer::gl::Winding::Clockwise : ux3d::renderer::gl::Winding::CounterClockwise;

                    renMesh->polygonMode = (mode == ux3d::renderer::RenderMode::Wireframe)
                        ? ux3d::renderer::gl::PolygonMode::Line
                        : ux3d::renderer::gl::PolygonMode::Fill;
                    renMesh->defines.define("DEBUG", static_cast<int>(mode));
                }
            }

            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);

            // Re-apply polygonMode and defines for all drawables in the active scene
            auto cubeTuple = gltfLayer->getResourceManager().getCubeMapTuple();
            auto* cubeMesh = std::get<0>(cubeTuple);
            auto* cubeMaterial = std::get<1>(cubeTuple);
            for (auto& [mesh, material] : scene.drawables) {
                if (mesh != nullptr && mesh != cubeMesh) {
                    mesh->polygonMode = (mode == ux3d::renderer::RenderMode::Wireframe)
                        ? ux3d::renderer::gl::PolygonMode::Line
                        : ux3d::renderer::gl::PolygonMode::Fill;
                    mesh->defines.define("DEBUG", static_cast<int>(mode));
                }
                if (material != nullptr && material != cubeMaterial) {
                    material->defines.define("DEBUG", static_cast<int>(mode));
                    if (viewportSettings.forceDoubleSided) {
                        material->isDoubleSided = true;
                    }
                }
            }

            // Continuously synchronize viewport lighting, exposure, and environment parameters
            commandGenerator->setExposure(std::pow(2.0f, viewportSettings.exposureEV));
            commandGenerator->setEnvironmentMapIntensity(viewportSettings.hdrIntensity);
            commandGenerator->setEnvironmentMapRotation(viewportSettings.hdrRotation);
            commandGenerator->setEnvironmentMapBlurNormalized(viewportSettings.hdrBlur);
            commandGenerator->setIBLEnabled(viewportSettings.iblEnabled);
            commandGenerator->setPunctualLightsEnabled(viewportSettings.punctualLightsEnabled);

            commandGenerator->update(scene);
            glRenderer->draw({ commandGenerator.get() });
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            if (viewportSettings.wireframeOverlay && mode != ux3d::renderer::RenderMode::Wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
                glLineWidth(1.0f);
                for (auto& [mesh, material] : scene.drawables) {
                    if (mesh != nullptr && mesh != cubeMesh) {
                        mesh->polygonMode = ux3d::renderer::gl::PolygonMode::Line;
                    }
                }
                commandGenerator->update(scene);
                glRenderer->draw({ commandGenerator.get() });
                glDisable(GL_POLYGON_OFFSET_LINE);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        }

        // 2. Render ground grid (infinite fading ground plane)
        if (viewportSettings.showGrid) {
            gridRenderer.draw(view, proj, orbitCamera.getEyePosition(), orbitCamera.getDistance());
        }

        // 3. Render bounding boxes
        if (viewportSettings.showSceneBoundingBox && activeScene != nullptr && !activeScene->getNodes().empty()) {
            ux3d::gltf::Extends ext = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
            bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                              glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                              glm::vec4(1.0f, 0.85f, 0.2f, 0.85f),
                              viewProj);
        }
        if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
            ux3d::gltf::Extends ext = getNodeExtends(selection.selectedNode, &gltfModel, gltfLayer.get());
            bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                              glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                              glm::vec4(0.72f, 0.95f, 0.15f, 0.95f),
                              viewProj);
        }

        sceneFbo.unbind();

        // 4. Post-processing Anti-Aliasing pass (MSAA resolved in FBO; FXAA & TAA resolved here)
        GLuint displayTexture = antiAliasing.render(aaMode,
                                                    sceneFbo.getColorTexture(),
                                                    vpW, vpH,
                                                    proj * view,
                                                    isSceneDirty);

        // Draw offscreen FBO texture inside ImGui viewport window
        if (displayTexture != 0) {
            ImGui::Image(static_cast<ImTextureID>(displayTexture),
                         viewportSize,
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }

        if (dumpFrameCountdown > 0) {
            dumpFrameCountdown--;
            if (dumpFrameCountdown == 0 && !dumpFramePath.empty()) {
                std::vector<uint8_t> pixels(vpW * vpH * 4);
                glBindTexture(GL_TEXTURE_2D, displayTexture != 0 ? displayTexture : sceneFbo.getColorTexture());
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glBindTexture(GL_TEXTURE_2D, 0);
                saveBmp(dumpFramePath.c_str(), vpW, vpH, pixels.data());
                std::printf("Dumped viewport frame to %s\n", dumpFramePath.c_str());
                if (exitAfterDump) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }
        }

        bool clickedCompass = false;

        // 3D Viewport Orientation Compass / Navigation Gizmo
        if (viewportSettings.showOrientationGizmo) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float radius = 35.0f;
            ImVec2 center = ImVec2(viewportPos.x + viewportSize.x - radius - 15.0f, viewportPos.y + radius + 15.0f);

            // Background subtle disc
            drawList->AddCircleFilled(center, radius + 4.0f, IM_COL32(28, 30, 38, 190));
            drawList->AddCircle(center, radius + 4.0f, IM_COL32(70, 75, 95, 180), 0, 1.5f);

            struct AxisDef {
                glm::vec3 dir;
                const char* name;
                ImU32 color;
                ImU32 dimColor;
                ux3d::imgui::PresetView preset;
            };

            AxisDef axisDefs[] = {
                { glm::vec3( 1.0f,  0.0f,  0.0f), "X",  IM_COL32(235, 75, 75, 255),   IM_COL32(140, 40, 40, 160), ux3d::imgui::PresetView::Right },
                { glm::vec3(-1.0f,  0.0f,  0.0f), "-X", IM_COL32(180, 70, 70, 200),  IM_COL32(100, 35, 35, 120), ux3d::imgui::PresetView::Left },
                { glm::vec3( 0.0f,  1.0f,  0.0f), "Y",  IM_COL32(75, 220, 95, 255),   IM_COL32(40, 130, 50, 160), ux3d::imgui::PresetView::Top },
                { glm::vec3( 0.0f, -1.0f,  0.0f), "-Y", IM_COL32(70, 170, 85, 200),  IM_COL32(35, 95, 45, 120),  ux3d::imgui::PresetView::Bottom },
                { glm::vec3( 0.0f,  0.0f,  1.0f), "Z",  IM_COL32(70, 140, 245, 255),  IM_COL32(40, 80, 150, 160), ux3d::imgui::PresetView::Front },
                { glm::vec3( 0.0f,  0.0f, -1.0f), "-Z", IM_COL32(65, 115, 200, 200), IM_COL32(35, 65, 120, 120), ux3d::imgui::PresetView::Back }
            };

            glm::mat3 rot = glm::mat3(view);

            struct ProjectedAxis {
                glm::vec3 v;
                AxisDef* def;
                ImVec2 pt;
            };
            std::vector<ProjectedAxis> projAxes;
            for (auto& a : axisDefs) {
                glm::vec3 v = rot * a.dir;
                ImVec2 pt = ImVec2(center.x + v.x * radius * 0.82f, center.y - v.y * radius * 0.82f);
                projAxes.push_back({ v, &a, pt });
            }
            std::sort(projAxes.begin(), projAxes.end(), [](const ProjectedAxis& a, const ProjectedAxis& b) {
                return a.v.z < b.v.z;
            });

            ImVec2 mousePos = io.MousePos;
            bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            float distCenterSq = (mousePos.x - center.x) * (mousePos.x - center.x) + (mousePos.y - center.y) * (mousePos.y - center.y);
            if (distCenterSq <= (radius + 6.0f) * (radius + 6.0f)) {
                clickedCompass = true;
            }

            for (const auto& pa : projAxes) {
                bool isPositive = (pa.def->name[0] != '-');
                float dotRadius = isPositive ? 7.0f : 4.5f;

                if (isPositive) {
                    drawList->AddLine(center, pa.pt, pa.def->color, 2.0f);
                }

                float distSq = (mousePos.x - pa.pt.x) * (mousePos.x - pa.pt.x) + (mousePos.y - pa.pt.y) * (mousePos.y - pa.pt.y);
                bool hovered = (distSq <= (dotRadius + 3.0f) * (dotRadius + 3.0f));

                ImU32 col = hovered ? IM_COL32(255, 255, 255, 255) : (pa.v.z >= 0.0f ? pa.def->color : pa.def->dimColor);
                drawList->AddCircleFilled(pa.pt, dotRadius, col);

                if (isPositive) {
                    ImVec2 textSize = ImGui::CalcTextSize(pa.def->name);
                    drawList->AddText(ImVec2(pa.pt.x - textSize.x * 0.5f, pa.pt.y - textSize.y * 0.5f), IM_COL32(20, 20, 25, 255), pa.def->name);
                }

                if (hovered && clicked) {
                    orbitCamera.setPresetView(pa.def->preset);
                    antiAliasing.resetHistory();
                }
            }
        }

        // Viewport mouse interaction & Picking
        bool isMouseInViewport = (io.MousePos.x >= viewportPos.x && io.MousePos.x <= viewportPos.x + viewportSize.x &&
                                  io.MousePos.y >= viewportPos.y && io.MousePos.y <= viewportPos.y + viewportSize.y);
        bool isViewportHovered = isMouseInViewport && ImGui::IsWindowHovered();

        if (isViewportHovered && !clickedCompass) {
            // [3] Mouse scroll to zoom
            if (io.MouseWheel != 0.0f) {
                orbitCamera.zoom(io.MouseWheel);
            }

            // [1] Middle drag to rotate view & [2] Shift-middle drag to pan view
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                if (io.KeyShift) {
                    // [2] Shift + Middle: Pan view
                    orbitCamera.pan(io.MouseDelta.x, io.MouseDelta.y);
                } else if (io.KeyCtrl) {
                    // Ctrl + Middle: Smooth zoom (standard Blender behavior)
                    orbitCamera.zoom(-io.MouseDelta.y * 0.05f);
                } else {
                    // [1] Middle drag alone: Rotate / Orbit view
                    orbitCamera.rotate(io.MouseDelta.x * 0.4f, io.MouseDelta.y * 0.4f);
                }
            }

            // Left-click 3D mesh picking
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
                float ndcX = ((io.MousePos.x - viewportPos.x) / viewportSize.x) * 2.0f - 1.0f;
                float ndcY = 1.0f - ((io.MousePos.y - viewportPos.y) / viewportSize.y) * 2.0f;

                glm::mat4 invVP = glm::inverse(proj * view);
                glm::vec4 nearNdc(ndcX, ndcY, -1.0f, 1.0f);
                glm::vec4 farNdc(ndcX, ndcY, 1.0f, 1.0f);
                glm::vec4 nearWorld = invVP * nearNdc;
                glm::vec4 farWorld = invVP * farNdc;

                if (std::abs(nearWorld.w) > 1e-6f && std::abs(farWorld.w) > 1e-6f) {
                    glm::vec3 pNear = glm::vec3(nearWorld) / nearWorld.w;
                    glm::vec3 pFar = glm::vec3(farWorld) / farWorld.w;
                    glm::vec3 rayOrigin = orbitCamera.isOrthographic() ? pNear : orbitCamera.getEyePosition();
                    glm::vec3 rayDir = glm::normalize(pFar - pNear);

                    auto* pickedNode = pickNodeFromRay(rayOrigin, rayDir, activeScene, gltfLayer.get());
                    if (pickedNode != nullptr) {
                        selection.selectNode(pickedNode);
                        s_scrollToSelectedNode = true;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            focusCameraOnNode(pickedNode, gltfModel, gltfLayer.get(), orbitCamera, antiAliasing, isSceneDirty);
                        }
                    } else {
                        selection = ux3d::imgui::SelectionState();
                    }
                }
            }
        }

        // 3D Coordinate Gizmo (non-transforming visual indicator for selected node)
        if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr && sceneFbo.isValid()) {
            auto* node = selection.selectedNode;
            glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawNodeCoordinateGizmo(drawList, nodeWorld, viewProj, viewportPos, viewportSize, orbitCamera.getEyePosition());
        }

            ImGui::End(); // End 3D Viewport
        }

        // -------------------------------------------------------------
        // Hierarchy Panel
        // -------------------------------------------------------------
        if (showHierarchy) {
            if (ImGui::Begin("Scene Hierarchy", &showHierarchy)) {
                if (activeScene != nullptr) {
                    std::string sceneTitle = activeScene->hasName() ? activeScene->getName().c_str() : ("Scene " + std::to_string(activeScene->m_ID));
                    if (ImGui::TreeNodeEx(sceneTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (auto* rootNode : activeScene->getNodes()) {
                            renderNodeHierarchy(rootNode, selection, gltfModel, gltfLayer.get(), orbitCamera, antiAliasing, isSceneDirty);
                        }
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
                    if (ImGui::Button("Open File...")) {
                        auto openRes = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                        if (!openRes.empty()) {
                            loadGltfModel(openRes[0], gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                            updateWindowTitle(window, currentFilePath);
                        }
                    }
                }
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Resources Panel
        // -------------------------------------------------------------
        if (showResources) {
            if (ImGui::Begin("Resources", &showResources)) {
                ux3d::imgui::ResourcesPanel::render(gltfModel, selection, gltfLayer.get(), &isSceneDirty);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Inspector Panel
        // -------------------------------------------------------------
        if (showInspector) {
            if (ImGui::Begin("Inspector", &showInspector)) {
                ux3d::imgui::ElementInspector::render(selection, gltfModel, isSceneDirty, &animationPlayer);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Viewport Settings Panel
        // -------------------------------------------------------------
        if (showViewportSettings) {
            if (ImGui::Begin("Viewport Settings", &showViewportSettings)) {
                ux3d::imgui::ViewportSettingsPanel::render(viewportSettings, commandGenerator.get(), gltfLayer.get(), orbitCamera, isSceneDirty, &gltfModel);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Statistics Panel
        // -------------------------------------------------------------
        if (showStatistics) {
            if (ImGui::Begin("Statistics", &showStatistics)) {
                ux3d::imgui::StatisticsPanel::render(gltfModel, currentFilePath);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Logger Panel
        // -------------------------------------------------------------
        if (showLogger) {
            if (ImGui::Begin("Logger", &showLogger)) {
                if (ImGui::Button("Clear")) {
                    s_consoleLogs.clear();
                }
                ImGui::SameLine();
                ImGui::Text("Messages: %zu", s_consoleLogs.size());
                ImGui::Separator();
                ImGui::BeginChild("LoggerScrollArea");
                for (const auto& entry : s_consoleLogs) {
                    ImGui::TextColored(entry.color, "%s", entry.text.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }

        // Demo Window (for debug/testing)
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // Rendering main window
        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    gridRenderer.cleanup();
    sceneFbo.unbind();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    ux3d::core::removeLoggerCallback(&consoleLogger);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
