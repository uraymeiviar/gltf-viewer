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
#include "imgui/GridRenderer.h"
#include "imgui/StdFileSystem.h"
#include "imgui/EmbeddedVFS.h"
#include "renderViewInput/CameraActor.h"

#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/Utils.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Mesh.h"
#include "gltf/Accessor.h"
#include "gltf/GlTypes.h"
#include "gltf/Material.h"
#include "gltf/ExportOptions.h"
#include "gltf/ImportOptions.h"

#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/PickingRenderLayer.h"

#include "core/Logger.h"

#include "imgui/MaterialInspector.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/ValidationPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/ElementInspector.h"
#include "imgui/AnimationPlayer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/BoundingBoxRenderer.h"
#include "imgui/Theme.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/AppSettings.h"
#include "imgui/SearchPathsPanel.h"
#include "imgui/SceneHierarchyPanel.h"
#include "imgui/StatusBar.h"
#include "imgui/ViewportInstance.h"

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

#include "imgui/stb_image_write.h"
#include <cctype>

static void saveImage(const char* filename, int width, int height, const uint8_t* rgba) {
    if (!filename || width <= 0 || height <= 0 || !rgba) return;

    // OpenGL glReadPixels returns bottom-to-top image; flip vertically to save right-side up
    stbi_flip_vertically_on_write(1);

    std::string fn = filename;
    std::string ext = "";
    auto dotPos = fn.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = fn.substr(dotPos);
        for (char& c : ext) c = static_cast<char>(std::tolower(c));
    }

    if (ext == ".jpg" || ext == ".jpeg") {
        stbi_write_jpg(filename, width, height, 4, rgba, 90);
    } else if (ext == ".png") {
        stbi_write_png(filename, width, height, 4, rgba, width * 4);
    } else {
        stbi_write_bmp(filename, width, height, 4, rgba);
    }
}

// Log console message history
using ux3d::imgui::LogEntry;
static std::vector<LogEntry> s_consoleLogs;

static void logMessage(const std::string& msg, ImVec4 color = ImVec4(0.85f, 0.85f, 0.88f, 1.0f), ux3d::core::LogLevel level = ux3d::core::LogLevel::Info)
{
    s_consoleLogs.push_back({ msg, color, level });
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
        logMessage(message, color, level);
    }
};

static void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
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

// Oriented Bounding Box calculation for selected node matching Blender's local bounding box
struct NodeOBB {
    glm::vec3 localMin{ std::numeric_limits<float>::max() };
    glm::vec3 localMax{ std::numeric_limits<float>::lowest() };
    glm::mat4 modelMatrix{ 1.0f };
    bool valid = false;
};

static void accumulateNodeOBB(const ux3d::gltf::Node* node,
                              const glm::mat4& invRootWorld,
                              const ux3d::renderer::GltfRenderLayer* gltfLayer,
                              NodeOBB& obb)
{
    if (node == nullptr) return;

    glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
    glm::mat4 toRootLocal = invRootWorld * nodeWorld;

    const auto* mesh = node->getMesh();
    if (mesh != nullptr) {
        for (const auto& primitive : mesh->getPrimitives()) {
            for (const auto& attribute : primitive.getAttributes()) {
                if (attribute.getName() != "POSITION") continue;
                const auto* accessor = attribute.getAccessor();
                if (accessor == nullptr) continue;
                if (accessor->getMin().size() < 3 || accessor->getMax().size() < 3) continue;

                glm::vec3 pMin(static_cast<float>(accessor->getMin()[0]),
                               static_cast<float>(accessor->getMin()[1]),
                               static_cast<float>(accessor->getMin()[2]));
                glm::vec3 pMax(static_cast<float>(accessor->getMax()[0]),
                               static_cast<float>(accessor->getMax()[1]),
                               static_cast<float>(accessor->getMax()[2]));

                if (accessor->isNormalized()) {
                    float minVal = std::abs(static_cast<float>(ux3d::gltf::GlTypes::minimumValueFromComponentType(accessor->getComponentType())));
                    float maxVal = std::abs(static_cast<float>(ux3d::gltf::GlTypes::maximumValueFromComponentType(accessor->getComponentType())));
                    if (minVal != 0.0f) pMin /= minVal;
                    if (maxVal != 0.0f) pMax /= maxVal;
                }

                glm::vec3 corners[8] = {
                    { pMin.x, pMin.y, pMin.z },
                    { pMax.x, pMin.y, pMin.z },
                    { pMax.x, pMin.y, pMax.z },
                    { pMin.x, pMin.y, pMax.z },
                    { pMin.x, pMax.y, pMin.z },
                    { pMax.x, pMax.y, pMin.z },
                    { pMax.x, pMax.y, pMax.z },
                    { pMin.x, pMax.y, pMax.z }
                };

                for (const auto& c : corners) {
                    glm::vec4 pt = toRootLocal * glm::vec4(c, 1.0f);
                    obb.localMin = glm::min(obb.localMin, glm::vec3(pt));
                    obb.localMax = glm::max(obb.localMax, glm::vec3(pt));
                    obb.valid = true;
                }
            }
        }
    }

    for (const auto* child : node->getChildren()) {
        accumulateNodeOBB(child, invRootWorld, gltfLayer, obb);
    }
}

static NodeOBB getNodeOBB(const ux3d::gltf::Node* selectedNode,
                          const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    NodeOBB obb;
    if (selectedNode == nullptr) return obb;

    obb.modelMatrix = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(selectedNode)) : glm::mat4(selectedNode->getTransformMatrix());
    float det = glm::determinant(obb.modelMatrix);
    glm::mat4 invRootWorld = (std::abs(det) > 1e-7f) ? glm::inverse(obb.modelMatrix) : glm::mat4(1.0f);

    accumulateNodeOBB(selectedNode, invRootWorld, gltfLayer, obb);

    if (obb.valid) {
        for (int i = 0; i < 3; ++i) {
            if (obb.localMax[i] - obb.localMin[i] < 1e-5f) {
                obb.localMin[i] -= 0.001f;
                obb.localMax[i] += 0.001f;
            }
        }
    }

    return obb;
}

// Focus and look at node using Gestaltor's native CameraActor
static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::editor::CameraActor& cameraActor,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              bool& sceneDirty)
{
    if (node == nullptr) return;
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel);
    antiAliasing.resetHistory();
    sceneDirty = true;
}


// Recent Files Persistence
static std::vector<std::string> s_recentFiles;

static std::string getRecentFilesConfigPath() {
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path newPath = dir / "recent_files.txt";
        if (!std::filesystem::exists(newPath)) {
            std::filesystem::path oldPath = std::filesystem::path(appData) / "Gestaltor" / "recent_files.txt";
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::copy_file(oldPath, newPath, ec);
            }
        }
        return newPath.string();
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

// Window State Persistence
struct WindowState {
    int width = 1600;
    int height = 900;
    int posX = 100;
    int posY = 100;
    bool maximized = false;
};

static std::string getWindowStateConfigPath() {
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path newPath = dir / "window_state.txt";
        if (!std::filesystem::exists(newPath)) {
            std::filesystem::path oldPath = std::filesystem::path(appData) / "Gestaltor" / "window_state.txt";
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::copy_file(oldPath, newPath, ec);
            }
        }
        return newPath.string();
    }
    return "window_state.txt";
}

static WindowState loadWindowState() {
    WindowState state;
    std::ifstream in(getWindowStateConfigPath());
    if (in.is_open()) {
        std::string key;
        int val = 0;
        while (in >> key >> val) {
            if (key == "width" && val >= 640) state.width = val;
            else if (key == "height" && val >= 480) state.height = val;
            else if (key == "posX") state.posX = val;
            else if (key == "posY") state.posY = val;
            else if (key == "maximized") state.maximized = (val != 0);
        }
    }
    return state;
}

static void saveWindowState(GLFWwindow* window) {
    if (window == nullptr) return;
    WindowState state;
    state.maximized = (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0);
    if (!state.maximized) {
        glfwGetWindowSize(window, &state.width, &state.height);
        glfwGetWindowPos(window, &state.posX, &state.posY);
    } else {
        WindowState prev = loadWindowState();
        state.width = prev.width;
        state.height = prev.height;
        state.posX = prev.posX;
        state.posY = prev.posY;
    }

    std::ofstream out(getWindowStateConfigPath());
    if (out.is_open()) {
        out << "width " << state.width << "\n";
        out << "height " << state.height << "\n";
        out << "posX " << state.posX << "\n";
        out << "posY " << state.posY << "\n";
        out << "maximized " << (state.maximized ? 1 : 0) << "\n";
    }
}

static void updateWindowTitle(GLFWwindow* window, const std::string& currentFilePath) {
    if (window == nullptr) return;
    std::string title = "Glider - ";
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
                          std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>>& viewports,
                          ux3d::renderer::GltfRenderLayer* gltfLayer,
                          ux3d::imgui::AnimationPlayer& animPlayer,
                          bool& sceneDirty)
{
    logMessage("Opening file: " + filePath);
    ux3d::imgui::AppSettings::instance().setCurrentModelPath(filePath);
    ux3d::gltf::ImportOptions importOptions;
    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());

    ux3d::imgui::ElementInspector::clearPreviewCache();
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
            for (auto& vp : viewports) {
                if (vp->isAttachedToCamera) {
                    bool found = false;
                    for (const auto& node : gltfModel.getNodes()) {
                        if (node.m_ID == vp->attachedCameraNodeId) {
                            vp->attachedCameraNode = &node;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        vp->isAttachedToCamera = false;
                        vp->attachedCameraNode = nullptr;
                    }
                }
                vp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                vp->antiAliasing.resetHistory();
            }
            ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
            char boundsBuf[256];
            std::snprintf(boundsBuf, sizeof(boundsBuf), "Scene Bounds: [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]",
                          extends.min.x, extends.min.y, extends.min.z,
                          extends.max.x, extends.max.y, extends.max.z);
            logMessage(boundsBuf, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
        }

        sceneDirty = true;
        logMessage("Loaded successfully: " + filePath, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));

        // Auto-run glTF 2.0 validation
        auto valReport = ux3d::imgui::GltfValidator::instance().validate(filePath, gltfModel);
        if (valReport.errorCount > 0) {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: %d %s, %d %s (%.1f ms)",
                          valReport.errorCount, valReport.errorCount == 1 ? "error" : "errors",
                          valReport.warningCount, valReport.warningCount == 1 ? "warning" : "warnings",
                          valReport.durationMs);
            logMessage(valBuf, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        } else if (valReport.warningCount > 0) {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: %d %s (%.1f ms)",
                          valReport.warningCount, valReport.warningCount == 1 ? "warning" : "warnings",
                          valReport.durationMs);
            logMessage(valBuf, ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
        } else {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: PASSED (glTF 2.0 Valid, %.1f ms)", valReport.durationMs);
            logMessage(valBuf, ImVec4(0.2f, 1.0f, 0.5f, 1.0f));
        }

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
                         ux3d::editor::CameraActor& cameraActor,
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
    if (activeScene != nullptr) {
        cameraActor.fitCameraToScene(*activeScene, &gltfModel);
    }
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



static void drawNodeCoordinateGizmo(ImDrawList* drawList,
                                    const glm::mat4& nodeWorld,
                                    const glm::mat4& viewProj,
                                    const ImVec2& vpPos,
                                    const ImVec2& vpSize,
                                    const glm::vec3& camEye,
                                    int axisConvention = 0)
{
    glm::vec3 origin = glm::vec3(nodeWorld[3]);
    float distToCam = glm::length(origin - camEye);
    float axisWorldLen = std::max(0.002f, distToCam * 0.085f);

    glm::vec3 xDir, yDir, zDir;
    if (axisConvention == 0) {
        // Blender Source Convention (Z-Up):
        // Red   (X): Blender local X = glTF column 0 (width)
        // Green (Y): Blender local Y = -glTF column 2 (length towards top of panel)
        // Blue  (Z): Blender local Z = +glTF column 1 (normal pointing out of panel face)
        xDir = glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
        yDir = glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? -glm::normalize(glm::vec3(nodeWorld[2])) : glm::vec3(0, 0, -1);
        zDir = glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
    } else {
        // glTF Native Convention (Y-Up):
        // Red   (X): glTF column 0
        // Green (Y): glTF column 1
        // Blue  (Z): glTF column 2
        xDir = glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
        yDir = glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
        zDir = glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[2])) : glm::vec3(0, 0, 1);
    }

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
        const char* label;
        float z;
    };
    std::vector<AxisItem> axes;

    ImVec2 ptX, ptY, ptZ;
    float zX, zY, zZ;
    if (project(xEnd, ptX, zX)) axes.push_back({ ptX, IM_COL32(235, 45, 45, 255), "X", zX });
    if (project(yEnd, ptY, zY)) axes.push_back({ ptY, IM_COL32(45, 215, 45, 255), "Y", zY });
    if (project(zEnd, ptZ, zZ)) axes.push_back({ ptZ, IM_COL32(45, 120, 245, 255), "Z", zZ });

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

            ImVec2 labelSize = ImGui::CalcTextSize(axis.label);
            ImVec2 labelPos(tip.x + dir.x * 10.0f - labelSize.x * 0.5f,
                            tip.y + dir.y * 10.0f - labelSize.y * 0.5f);
            drawList->AddText(ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f), IM_COL32(0, 0, 0, 220), axis.label);
            drawList->AddText(labelPos, axis.color, axis.label);
        }
    }

    drawList->AddCircleFilled(ptOrigin, 4.5f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircle(ptOrigin, 4.5f, IM_COL32(30, 30, 30, 255), 0, 1.5f);
}

static ux3d::filesystem::Path resolveRendererResourcesDir()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    if (fs.isFile(fs.toPath("resources/shaders/primitive.vert")) ||
        fs.isFile(fs.toPath("shaders/primitive.vert"))) {
        return fs.toPath("resources");
    }

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

    return fs.toPath("resources");
}

static ux3d::filesystem::Path resolveEnvironmentPath(const ux3d::filesystem::Path& rendererRes)
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto env1 = rendererRes / "environments" / "footprint_court.hdr";
    if (fs.isFile(env1)) return env1;

    auto env2 = fs.toPath("resources/environments/footprint_court.hdr");
    if (fs.isFile(env2)) return env2;

    auto env3 = fs.toPath("environments/footprint_court.hdr");
    if (fs.isFile(env3)) return env3;

    auto exeDir = fs.getExecutableDirPath();
    auto env4 = exeDir / "resources" / "environments" / "footprint_court.hdr";
    if (fs.isFile(env4)) return env4;

    return env1;
}

static std::string resolveSampleModelPath()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto p1 = fs.toPath("resources/sampleGltf/HelloCube.glb");
    if (fs.isFile(p1)) return p1.toString().c_str();

    auto p2 = fs.toPath("sampleGltf/HelloCube.glb");
    if (fs.isFile(p2)) return p2.toString().c_str();

    auto exeDir = fs.getExecutableDirPath();
    auto p3 = exeDir / "resources" / "sampleGltf" / "HelloCube.glb";
    if (fs.isFile(p3)) return p3.toString().c_str();

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
        case 21: return ux3d::renderer::RenderMode::ClearcoatFactor;
        case 22: return ux3d::renderer::RenderMode::ClearcoatRoughness;
        case 23: return ux3d::renderer::RenderMode::ClearcoatNormal;
        case 24: return ux3d::renderer::RenderMode::SheenColor;
        case 25: return ux3d::renderer::RenderMode::SheenRoughness;
        case 26: return ux3d::renderer::RenderMode::SpecularFactor;
        case 27: return ux3d::renderer::RenderMode::SpecularColor;
        case 28: return ux3d::renderer::RenderMode::TransmissionFactor;
        case 29: return ux3d::renderer::RenderMode::VolumeThickness;
        case 30: return ux3d::renderer::RenderMode::IridescenceFactor;
        case 31: return ux3d::renderer::RenderMode::IridescenceThickness;
        default: return ux3d::renderer::RenderMode::None;
    }
}

static int getIndexFromRenderMode(ux3d::renderer::RenderMode mode)
{
    using RM = ux3d::renderer::RenderMode;
    switch (mode) {
        case RM::Wireframe: return 1;
        case RM::NormalTexture: return 2;
        case RM::ShadingNormal: return 3;
        case RM::NormalGeometry: return 4;
        case RM::Tangent: return 5;
        case RM::Bitangent: return 6;
        case RM::BaseColor: return 7;
        case RM::Metallic: return 8;
        case RM::Roughness: return 9;
        case RM::MetallicRoughness: return 10;
        case RM::Occlusion: return 11;
        case RM::Emissive: return 12;
        case RM::Alpha: return 13;
        case RM::TexCoord_0: return 14;
        case RM::TexCoord_1: return 15;
        case RM::Clearcoat: return 16;
        case RM::Sheen: return 17;
        case RM::Specular: return 18;
        case RM::TransmissionVolume: return 19;
        case RM::Iridescence: return 20;
        case RM::ClearcoatFactor: return 21;
        case RM::ClearcoatRoughness: return 22;
        case RM::ClearcoatNormal: return 23;
        case RM::SheenColor: return 24;
        case RM::SheenRoughness: return 25;
        case RM::SpecularFactor: return 26;
        case RM::SpecularColor: return 27;
        case RM::TransmissionFactor: return 28;
        case RM::VolumeThickness: return 29;
        case RM::IridescenceFactor: return 30;
        case RM::IridescenceThickness: return 31;
        default: return 0;
    }
}

struct ShadingModeInfo {
    const char* label;
    ux3d::renderer::RenderMode mode;
    const char* shortcut;
    bool isSeparator;
};

static const ShadingModeInfo s_shadingModes[] = {
    { "Default Shaded", ux3d::renderer::RenderMode::None, "Ctrl+Alt+1", false },
    { "Wireframe", ux3d::renderer::RenderMode::Wireframe, "Ctrl+Alt+2", false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Texture Coordinates 0", ux3d::renderer::RenderMode::TexCoord_0, nullptr, false },
    { "Texture Coordinates 1", ux3d::renderer::RenderMode::TexCoord_1, nullptr, false },
    { "Normal Texture", ux3d::renderer::RenderMode::NormalTexture, nullptr, false },
    { "Geometry Normal", ux3d::renderer::RenderMode::NormalGeometry, nullptr, false },
    { "Geometry Tangent", ux3d::renderer::RenderMode::Tangent, nullptr, false },
    { "Geometry Bitangent", ux3d::renderer::RenderMode::Bitangent, nullptr, false },
    { "Shading Normal", ux3d::renderer::RenderMode::ShadingNormal, nullptr, false },
    { "Alpha", ux3d::renderer::RenderMode::Alpha, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Occlusion", ux3d::renderer::RenderMode::Occlusion, nullptr, false },
    { "Emissive", ux3d::renderer::RenderMode::Emissive, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Metallic Roughness", ux3d::renderer::RenderMode::MetallicRoughness, nullptr, false },
    { "    BaseColor", ux3d::renderer::RenderMode::BaseColor, nullptr, false },
    { "    Roughness", ux3d::renderer::RenderMode::Roughness, nullptr, false },
    { "    Metallic", ux3d::renderer::RenderMode::Metallic, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Clearcoat", ux3d::renderer::RenderMode::Clearcoat, nullptr, false },
    { "    Clearcoat Factor", ux3d::renderer::RenderMode::ClearcoatFactor, nullptr, false },
    { "    Clearcoat Roughness", ux3d::renderer::RenderMode::ClearcoatRoughness, nullptr, false },
    { "    Clearcoat Normal", ux3d::renderer::RenderMode::ClearcoatNormal, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Sheen", ux3d::renderer::RenderMode::Sheen, nullptr, false },
    { "    Sheen Color", ux3d::renderer::RenderMode::SheenColor, nullptr, false },
    { "    Sheen Roughness", ux3d::renderer::RenderMode::SheenRoughness, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Specular", ux3d::renderer::RenderMode::Specular, nullptr, false },
    { "    Specular Factor", ux3d::renderer::RenderMode::SpecularFactor, nullptr, false },
    { "    Specular Color", ux3d::renderer::RenderMode::SpecularColor, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Transmission/Volume", ux3d::renderer::RenderMode::TransmissionVolume, nullptr, false },
    { "    Transmission Factor", ux3d::renderer::RenderMode::TransmissionFactor, nullptr, false },
    { "    Volume Thickness", ux3d::renderer::RenderMode::VolumeThickness, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Iridescence", ux3d::renderer::RenderMode::Iridescence, nullptr, false },
    { "    Iridescence Factor", ux3d::renderer::RenderMode::IridescenceFactor, nullptr, false },
    { "    Iridescence Thickness", ux3d::renderer::RenderMode::IridescenceThickness, nullptr, false },
};

static const char* getRenderModeLabel(ux3d::renderer::RenderMode mode)
{
    using RM = ux3d::renderer::RenderMode;
    switch (mode) {
        case RM::Wireframe: return "Wireframe";
        case RM::TexCoord_0: return "TexCoord 0";
        case RM::TexCoord_1: return "TexCoord 1";
        case RM::NormalTexture: return "Normal Texture";
        case RM::ShadingNormal: return "Shading Normal";
        case RM::NormalGeometry: return "Geometry Normal";
        case RM::Tangent: return "Geometry Tangent";
        case RM::Bitangent: return "Geometry Bitangent";
        case RM::Alpha: return "Alpha";
        case RM::Occlusion: return "Occlusion";
        case RM::Emissive: return "Emissive";
        case RM::MetallicRoughness: return "Metallic Roughness";
        case RM::Roughness: return "Roughness";
        case RM::Metallic: return "Metallic";
        case RM::BaseColor: return "BaseColor";
        case RM::Clearcoat: return "Clearcoat";
        case RM::ClearcoatFactor: return "Clearcoat Factor";
        case RM::ClearcoatRoughness: return "Clearcoat Roughness";
        case RM::ClearcoatNormal: return "Clearcoat Normal";
        case RM::Sheen: return "Sheen";
        case RM::SheenColor: return "Sheen Color";
        case RM::SheenRoughness: return "Sheen Roughness";
        case RM::Specular: return "Specular";
        case RM::SpecularFactor: return "Specular Factor";
        case RM::SpecularColor: return "Specular Color";
        case RM::TransmissionVolume: return "Transmission/Volume";
        case RM::TransmissionFactor: return "Transmission Factor";
        case RM::VolumeThickness: return "Volume Thickness";
        case RM::Iridescence: return "Iridescence";
        case RM::IridescenceFactor: return "Iridescence Factor";
        case RM::IridescenceThickness: return "Iridescence Thickness";
        default: return "Default Shaded";
    }
}

static std::vector<const ux3d::gltf::Node*> collectSceneCameraNodes(const ux3d::gltf::Scene* scene)
{
    std::vector<const ux3d::gltf::Node*> cameraNodes;
    if (scene != nullptr) {
        std::function<void(const ux3d::gltf::Node*)> collect = [&](const ux3d::gltf::Node* n) {
            if (!n) return;
            if (n->getCamera() != nullptr) {
                cameraNodes.push_back(n);
            }
            for (const auto* ch : n->getChildren()) {
                collect(ch);
            }
        };
        for (const auto* rootNode : scene->getNodes()) {
            collect(rootNode);
        }
    }
    return cameraNodes;
}

static const char* const WND_HIERARCHY          = ICON_MDI_FILE_TREE " Scene Hierarchy";
static const char* const WND_RESOURCES          = ICON_MDI_FOLDER_MULTIPLE_OUTLINE " Resources";
static const char* const WND_INSPECTOR          = ICON_MDI_TUNE " Inspector";
static const char* const WND_LOGGER             = ICON_MDI_CONSOLE " Logger";
static const char* const WND_STATISTICS         = ICON_MDI_CHART_BAR " Statistics";
static const char* const WND_VALIDATION         = ICON_MDI_SHIELD_CHECK_OUTLINE " Validation###ValidationPanel";

static ImGuiID s_centralDockId = 0;

static void setupDefaultDockLayout(ImGuiID dockspaceId, ImVec2 size, bool force = false)
{
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
    if (node == nullptr || force) {
        if (node != nullptr) {
            ImGui::DockBuilderRemoveNode(dockspaceId);
        }
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, size);

        ImGuiID dock_main_id = dockspaceId;

        // 1. Split Left column (Hierarchy on top, Resources on bottom)
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
        ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.45f, nullptr, &dock_left);

        // 2. Split Right column (Inspector full height)
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.26f, nullptr, &dock_main_id);

        // 3. Center column: Viewport on top, Logger on bottom splitting the viewport (not whole window)
        ImGuiID dock_center_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);

        s_centralDockId = dock_main_id;

        ImGui::DockBuilderDockWindow(ICON_MDI_CUBE_OUTLINE " 3D Viewport###Viewport_1", dock_main_id);
        ImGui::DockBuilderDockWindow(WND_LOGGER, dock_center_bottom);
        ImGui::DockBuilderDockWindow(WND_HIERARCHY, dock_left);
        ImGui::DockBuilderDockWindow(WND_RESOURCES, dock_left_bottom);
        ImGui::DockBuilderDockWindow(WND_INSPECTOR, dock_right);

        // Dock optional panels in center-bottom if opened later
        ImGui::DockBuilderDockWindow(WND_STATISTICS, dock_center_bottom);
        ImGui::DockBuilderDockWindow(WND_VALIDATION, dock_center_bottom);

        ImGui::DockBuilderFinish(dockspaceId);
    } else {
        s_centralDockId = dockspaceId;
    }
}

int main(int argc, char** argv)
{
    // Initialize embedded Virtual File System (decompresses runtime resources in-memory)
    ux3d::imgui::EmbeddedVFS::instance().initialize();

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    WindowState winState = loadWindowState();

    // Target OpenGL 4.1 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, winState.maximized ? GLFW_TRUE : GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create GLFW window (starts at restored or default size)
    GLFWwindow* window = glfwCreateWindow(winState.width, winState.height, "Glider - glTF 2.0 Viewer", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    if (!winState.maximized) {
        glfwSetWindowPos(window, winState.posX, winState.posY);
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

    // Load FSS Fonts (fss-ui.ttf and fss-ui-mono.ttf with embedded MDI icons)
    ux3d::imgui::loadFssFonts(io);

    // Apply FSS ImGui Theme (aviation dark palette with emerald green accents)
    ux3d::imgui::applyFssTheme();

    // Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Initialize 3D Viewport components
    ux3d::imgui::GridRenderer gridRenderer;
    gridRenderer.init();
    ux3d::imgui::BoundingBoxRenderer bboxRenderer;
    bboxRenderer.init();

    ux3d::gltf::Gltf gltfModel;
    ux3d::gltf::Scene* activeScene = nullptr;
    ux3d::imgui::SelectionState selection;
    std::string currentFilePath = "";
    bool isSceneDirty = false;

    ux3d::imgui::AnimationPlayer animationPlayer;

    auto rendererResourcesDir = resolveRendererResourcesDir();
    auto shaderSource = rendererResourcesDir / "shaders";

    auto gltfLayer = std::make_unique<ux3d::renderer::GltfRenderLayer>(gladGL.get(), shaderSource / "ibl_sampler");
    auto commandGenerator = std::make_unique<ux3d::renderer::CommandGenerator>(gladGL.get(), shaderSource);
    auto glRenderer = std::make_unique<ux3d::renderer::Renderer>(gladGL.get());
    glRenderer->init(ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));

    ux3d::renderer::PickingRenderLayer pickingLayer(gladGL.get());
    ux3d::renderer::ReverseMapping currentReverseMapping;

    std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>> viewports;
    viewports.push_back(std::make_unique<ux3d::imgui::ViewportInstance>(1, "3D Viewport", gladGL.get()));
    ux3d::imgui::ViewportInstance* activeViewport = viewports[0].get();

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
    std::string selectNodeArg;
    std::string hideNodeArg;
    std::string selectMaterialArg;
    std::string selectImageArg;
    std::string selectTextureArg;
    uint32_t selectViewportArg = 0;
    std::string newViewportCamArg;
    bool showValidationArg = false;
    bool showSearchPathsArg = false;
    float animTimeArg = -1.0f;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-frame" && i + 1 < argc) {
            dumpFramePath = argv[++i];
            dumpFrameCountdown = 8;
        } else if (arg == "--exit-after-dump") {
            exitAfterDump = true;
        } else if (arg == "--anim-time" && i + 1 < argc) {
            animTimeArg = std::stof(argv[++i]);
        } else if (arg == "--select-node" && i + 1 < argc) {
            selectNodeArg = argv[++i];
        } else if (arg == "--hide-node" && i + 1 < argc) {
            hideNodeArg = argv[++i];
        } else if (arg == "--select-material" && i + 1 < argc) {
            selectMaterialArg = argv[++i];
        } else if (arg == "--select-image" && i + 1 < argc) {
            selectImageArg = argv[++i];
        } else if (arg == "--select-texture" && i + 1 < argc) {
            selectTextureArg = argv[++i];
        } else if (arg == "--select-viewport" && i + 1 < argc) {
            selectViewportArg = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--create-viewport-cam" && i + 1 < argc) {
            newViewportCamArg = argv[++i];
        } else if (arg == "--show-validation" || arg == "--validate") {
            showValidationArg = true;
        } else if (arg == "--show-search-paths" || arg == "--search-paths") {
            showSearchPathsArg = true;
        } else if (arg == "--filter-node" && i + 1 < argc) {
            ux3d::imgui::SceneHierarchyPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--filter-resource" && i + 1 < argc) {
            ux3d::imgui::ResourcesPanel::setSearchFilter(argv[++i]);
        } else if ((arg == "--env" || arg == "--hdr") && i + 1 < argc) {
            std::string customHdr = argv[++i];
            auto path = ux3d::imgui::StdFileSystem::instance().toPath(customHdr.c_str());
            gltfLayer->setEnvironmentMap(path);
            if (gltfLayer->filterEnvironmentMap()) {
                activeViewport->settings.currentHdrPath = customHdr;
                commandGenerator->setIBLEnabled(true);
                logMessage("Loaded environment map: " + customHdr);
            }
        } else if ((arg == "--aa" || arg == "--anti-aliasing") && i + 1 < argc) {
            std::string aaVal = argv[++i];
            if (aaVal == "none" || aaVal == "0") activeViewport->settings.antiAliasingIdx = 0;
            else if (aaVal == "msaa2" || aaVal == "1") activeViewport->settings.antiAliasingIdx = 1;
            else if (aaVal == "msaa4" || aaVal == "2") activeViewport->settings.antiAliasingIdx = 2;
            else if (aaVal == "msaa8" || aaVal == "3") activeViewport->settings.antiAliasingIdx = 3;
            else if (aaVal == "fxaa" || aaVal == "4") activeViewport->settings.antiAliasingIdx = 4;
            else if (aaVal == "taa" || aaVal == "5") activeViewport->settings.antiAliasingIdx = 5;
        } else if (!arg.empty() && arg[0] != '-') {
            modelToLoad = arg;
        }
    }

    ux3d::imgui::AppSettings::instance().loadSettings();
    s_recentFiles = loadRecentFiles();

    if (ux3d::imgui::StdFileSystem::instance().isFile(ux3d::imgui::StdFileSystem::instance().toFile(modelToLoad.c_str()))) {
        loadGltfModel(modelToLoad, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
        updateWindowTitle(window, currentFilePath);
        if (!selectNodeArg.empty()) {
            for (auto& node : gltfModel.getNodes()) {
                if (node.hasName() && node.getName() == selectNodeArg.c_str()) {
                    selection.selectNode(&node);
                    if (activeViewport) {
                        focusCameraOnNode(&node, gltfModel, gltfLayer.get(), activeViewport->cameraActor, activeViewport->antiAliasing, isSceneDirty);
                    }
                    break;
                }
            }
        }
        if (!hideNodeArg.empty()) {
            std::string target = hideNodeArg;
            if (target.size() >= 2 && target.front() == '"' && target.back() == '"') {
                target = target.substr(1, target.size() - 2);
            }
            for (auto& node : gltfModel.getNodes()) {
                if ((node.hasName() && (node.getName() == target || node.getName() == hideNodeArg)) ||
                    std::to_string(node.m_ID) == hideNodeArg) {
                    node.setVisible(false);
                    isSceneDirty = true;
                    std::printf("Hiding node %u: %s\n", node.m_ID, node.hasName() ? node.getName().c_str() : "");
                    break;
                }
            }
        }
        if (!selectMaterialArg.empty()) {
            for (auto& mat : gltfModel.getElements<ux3d::gltf::Material>()) {
                if ((mat.hasName() && mat.getName() == selectMaterialArg.c_str()) ||
                    std::to_string(mat.m_ID) == selectMaterialArg) {
                    selection.selectMaterial(&mat);
                    break;
                }
            }
        }
        if (!selectImageArg.empty()) {
            for (auto& img : gltfModel.getElements<ux3d::gltf::Image>()) {
                if ((img.hasName() && img.getName() == selectImageArg.c_str()) ||
                    std::to_string(img.m_ID) == selectImageArg ||
                    (!img.getUri().empty() && img.getUri().find(selectImageArg) != std::string::npos)) {
                    selection.selectImage(&img);
                    break;
                }
            }
        }
        if (!selectTextureArg.empty()) {
            for (auto& tex : gltfModel.getElements<ux3d::gltf::Texture>()) {
                if ((tex.hasName() && tex.getName() == selectTextureArg.c_str()) ||
                    std::to_string(tex.m_ID) == selectTextureArg) {
                    selection.selectTexture(&tex);
                    break;
                }
            }
        }
        if (animTimeArg >= 0.0f) {
            animationPlayer.setCurrentTime(animTimeArg, gltfLayer.get());
        }

        if (!newViewportCamArg.empty() && activeScene != nullptr) {
            std::string camTarget = newViewportCamArg;
            if (camTarget.size() >= 2 && camTarget.front() == '"' && camTarget.back() == '"') {
                camTarget = camTarget.substr(1, camTarget.size() - 2);
            }
            std::function<void(const ux3d::gltf::Node*)> checkNode = [&](const ux3d::gltf::Node* n) {
                if (!n) return;
                if (n->getCamera() != nullptr) {
                    std::string cName = n->hasName() ? n->getName() : ("Camera Node " + std::to_string(n->m_ID));
                    if (cName == camTarget || std::to_string(n->m_ID) == camTarget) {
                        uint32_t nextId = 1;
                        for (const auto& v : viewports) {
                            if (v->id >= nextId) nextId = v->id + 1;
                        }
                        std::string vpName = "Camera: " + cName;
                        auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                        newVp->isAttachedToCamera = true;
                        newVp->attachedCameraNode = n;
                        newVp->attachedCameraNodeId = n->m_ID;
                        newVp->attachedCameraNodeName = cName;
                        if (activeViewport != nullptr) {
                            newVp->settings = activeViewport->settings;
                        }
                        newVp->requestFocus = true;
                        activeViewport = newVp.get();
                        viewports.push_back(std::move(newVp));
                        isSceneDirty = true;
                        return;
                    }
                }
                for (auto* child : n->getChildren()) {
                    checkNode(child);
                }
            };
            for (auto* rootNode : activeScene->getNodes()) {
                checkNode(rootNode);
            }
        }

        if (selectViewportArg != 0) {
            for (auto& vp : viewports) {
                if (vp && vp->id == selectViewportArg) {
                    selection.selectViewport(vp->id);
                    break;
                }
            }
        }
    } else {
        updateWindowTitle(window, "untitled.glb");
    }

    // UI state
    bool showDemoWindow = false;
    static bool showInspector = true;
    static bool showHierarchy = true;
    static bool showResources = true;
    static bool showLogger = false;
    static bool showStatistics = false;
    static bool showValidation = false;
    static bool showSearchPaths = false;
    if (showValidationArg) {
        showValidation = true;
        ux3d::imgui::ValidationPanel::requestFocus();
    }
    if (showSearchPathsArg) {
        showSearchPaths = true;
    }

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
        for (auto& vp : viewports) {
            if (vp->isOpen && vp->settings.turntable && !ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                vp->cameraActor.rotateCamera(static_cast<int32_t>(vp->settings.turntableSpeed * deltaTime * 20.0f), 0);
            }
        }

        // Keyboard shortcuts
        if (!io.WantTextInput && activeViewport != nullptr) {
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                if (activeScene != nullptr) activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                activeViewport->antiAliasing.resetHistory();
            }

            // Numpad Camera Presets (Gestaltor style)
            if (!activeViewport->isAttachedToCamera) {
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad1)) {
                    activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Back : ux3d::editor::CameraOrientation::Front);
                    activeViewport->antiAliasing.resetHistory();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad3)) {
                    activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Left : ux3d::editor::CameraOrientation::Right);
                    activeViewport->antiAliasing.resetHistory();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad7)) {
                    activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Bottom : ux3d::editor::CameraOrientation::Top);
                    activeViewport->antiAliasing.resetHistory();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad5)) {
                    activeViewport->cameraActor.changeUserCameraType((activeViewport->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic) ? ux3d::renderer::Camera::Type::Perspective : ux3d::renderer::Camera::Type::Orthographic);
                    activeViewport->antiAliasing.resetHistory();
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal)) {
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    activeViewport->cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                    activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
                activeViewport->antiAliasing.resetHistory();
            }

            // Ctrl+Alt+1: Default Shaded, Ctrl+Alt+2: Wireframe
            if (io.KeyCtrl && io.KeyAlt) {
                if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                    activeViewport->settings.renderMode = ux3d::renderer::RenderMode::None;
                    activeViewport->settings.renderModeIdx = 0;
                    isSceneDirty = true;
                } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                    activeViewport->settings.renderMode = ux3d::renderer::RenderMode::Wireframe;
                    activeViewport->settings.renderModeIdx = 1;
                    isSceneDirty = true;
                }
            }
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // Main DockSpace covering the whole window (minus status bar at bottom)
        float statusBarHeight = ImGui::GetFrameHeight() + 8.0f;
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImVec2 dockspaceSize = ImVec2(mainViewport->WorkSize.x, mainViewport->WorkSize.y - statusBarHeight);
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("GliderDockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("GliderMainDockSpace_v1");

        static bool s_firstLayout = true;
        static bool s_resetLayout = false;
        if (s_firstLayout || s_resetLayout) {
            bool force = s_resetLayout;
            s_firstLayout = false;
            s_resetLayout = false;
            setupDefaultDockLayout(dockspaceId, dockspaceSize, force);
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

        // Global Keyboard Shortcuts (File & View)
        if (!io.WantTextInput && io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_O)) {
                auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                if (!openResult.empty()) {
                    loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                    updateWindowTitle(window, currentFilePath);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_1)) showInspector = !showInspector;
            if (ImGui::IsKeyPressed(ImGuiKey_2)) showHierarchy = !showHierarchy;
            if (ImGui::IsKeyPressed(ImGuiKey_3)) {
                if (activeViewport) {
                    activeViewport->isOpen = !activeViewport->isOpen;
                    if (activeViewport->isOpen) activeViewport->requestFocus = true;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_4)) showResources = !showResources;
            if (ImGui::IsKeyPressed(ImGuiKey_5)) showLogger = !showLogger;
            if (ImGui::IsKeyPressed(ImGuiKey_6)) {
                if (activeViewport) selection.selectViewport(activeViewport->id);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_7)) showStatistics = !showStatistics;
            if (ImGui::IsKeyPressed(ImGuiKey_8)) showValidation = !showValidation;
            if (ImGui::IsKeyPressed(ImGuiKey_L)) {
                showInspector = showHierarchy = showResources = true;
                showLogger = showStatistics = showValidation = false;
                for (auto& vp : viewports) vp->isOpen = true;
                s_resetLayout = true;
            }
        }

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open ...", "Ctrl+O")) {
                    auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                    if (!openResult.empty()) {
                        loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                        updateWindowTitle(window, currentFilePath);
                    }
                }
                if (ImGui::BeginMenu(ICON_MDI_HISTORY " Open Recent")) {
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
                        if (ImGui::MenuItem(ICON_MDI_DELETE_OUTLINE " Clear Menu")) {
                            s_recentFiles.clear();
                            saveRecentFiles(s_recentFiles);
                        }
                        if (!fileToOpen.empty()) {
                            loadGltfModel(fileToOpen, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                            updateWindowTitle(window, currentFilePath);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_FOLDER_SEARCH_OUTLINE " Search Paths...", nullptr, &showSearchPaths)) {
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_EXIT_TO_APP " Exit")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            // Viewport Menu (Replaces previous View menu)
            if (ImGui::BeginMenu("Viewport")) {
                if (ImGui::MenuItem(ICON_MDI_PLUS " New Viewport")) {
                    uint32_t nextId = 1;
                    for (const auto& v : viewports) {
                        if (v->id >= nextId) nextId = v->id + 1;
                    }
                    std::string vpName = "Viewport " + std::to_string(nextId);
                    auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                    if (activeViewport != nullptr) {
                        newVp->settings = activeViewport->settings;
                        if (activeScene != nullptr) {
                            newVp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                        }
                    }
                    newVp->requestFocus = true;
                    activeViewport = newVp.get();
                    selection.selectViewport(newVp->id);
                    viewports.push_back(std::move(newVp));
                }

                if (ImGui::BeginMenu(ICON_MDI_CAMERA_PLUS_OUTLINE " New Viewport From")) {
                    auto cameraNodes = collectSceneCameraNodes(activeScene);
                    if (cameraNodes.empty()) {
                        ImGui::MenuItem("(No Cameras in Scene)", nullptr, false, false);
                    } else {
                        for (const auto* camNode : cameraNodes) {
                            std::string camName = camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                            std::string itemTitle = ICON_MDI_CAMERA " " + camName;
                            if (ImGui::MenuItem(itemTitle.c_str())) {
                                uint32_t nextId = 1;
                                for (const auto& v : viewports) {
                                    if (v->id >= nextId) nextId = v->id + 1;
                                }
                                std::string vpName = "Camera: " + camName;
                                auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                                newVp->isAttachedToCamera = true;
                                newVp->attachedCameraNode = camNode;
                                newVp->attachedCameraNodeId = camNode->m_ID;
                                newVp->attachedCameraNodeName = camName;
                                if (activeViewport != nullptr) {
                                    newVp->settings = activeViewport->settings;
                                }
                                newVp->requestFocus = true;
                                activeViewport = newVp.get();
                                selection.selectViewport(newVp->id);
                                viewports.push_back(std::move(newVp));
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                bool hasActiveVp = (activeViewport != nullptr);

                bool showGrid = hasActiveVp && activeViewport->settings.showGrid;
                if (ImGui::MenuItem(ICON_MDI_GRID " Toggle Show Grid", nullptr, showGrid, hasActiveVp)) {
                    activeViewport->settings.showGrid = !activeViewport->settings.showGrid;
                }

                bool showBBox = hasActiveVp && (activeViewport->settings.showSceneBoundingBox || activeViewport->settings.showSelectedBoundingBox);
                if (ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Toggle Show Bounding Box", nullptr, showBBox, hasActiveVp)) {
                    bool next = !showBBox;
                    activeViewport->settings.showSceneBoundingBox = next;
                    activeViewport->settings.showSelectedBoundingBox = next;
                }

                bool isPersp = hasActiveVp && (activeViewport->rendererCamera.getType() == ux3d::renderer::Camera::Type::Perspective);
                if (ImGui::MenuItem(ICON_MDI_CAMERA " Toggle Persp / Orthographic", "Num 5", isPersp, hasActiveVp && !activeViewport->isAttachedToCamera)) {
                    activeViewport->cameraActor.changeUserCameraType(isPersp ? ux3d::renderer::Camera::Type::Orthographic : ux3d::renderer::Camera::Type::Perspective);
                    activeViewport->antiAliasing.resetHistory();
                }

                bool isTurntable = hasActiveVp && activeViewport->settings.turntable;
                if (ImGui::MenuItem(ICON_MDI_ROTATE_ORBIT " Toggle Orbit (Turntable)", nullptr, isTurntable, hasActiveVp)) {
                    activeViewport->settings.turntable = !activeViewport->settings.turntable;
                }

                ImGui::Separator();

                bool isAttached = hasActiveVp && activeViewport->isAttachedToCamera;
                if (ImGui::MenuItem(ICON_MDI_CAMERA_OFF " Detach from Camera", nullptr, false, isAttached)) {
                    activeViewport->isAttachedToCamera = false;
                    activeViewport->attachedCameraNode = nullptr;
                    activeViewport->attachedCameraNodeId = 0;
                    activeViewport->attachedCameraNodeName = "";
                    isSceneDirty = true;
                }

                if (ImGui::BeginMenu(ICON_MDI_COMPASS_OUTLINE " Orientation", hasActiveVp && !isAttached)) {
                    if (ImGui::MenuItem("Front", "Num 1")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Front); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Back", "Ctrl+Num 1")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Back); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Right", "Num 3")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Right); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Left", "Ctrl+Num 3")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Left); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Top", "Num 7")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Top); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Bottom", "Ctrl+Num 7")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Bottom); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Isometric")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Isometric); activeViewport->antiAliasing.resetHistory(); }
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem(ICON_MDI_HOME_OUTLINE " Reset Camera", "Home", false, hasActiveVp)) {
                    if (activeScene != nullptr) activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                    activeViewport->antiAliasing.resetHistory();
                }

                if (ImGui::MenuItem(ICON_MDI_CROSSHAIRS_GPS " Fit to Scene", "F", false, hasActiveVp && activeScene != nullptr)) {
                    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                        activeViewport->cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                    } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                        activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                    }
                    activeViewport->antiAliasing.resetHistory();
                }

                if (ImGui::BeginMenu(ICON_MDI_SHIELD_OUTLINE " Shading", hasActiveVp)) {
                    for (const auto& item : s_shadingModes) {
                        if (item.isSeparator) {
                            ImGui::Separator();
                        } else {
                            bool isSelected = (activeViewport->settings.renderMode == item.mode);
                            if (ImGui::MenuItem(item.label, item.shortcut, isSelected)) {
                                activeViewport->settings.renderMode = item.mode;
                                activeViewport->settings.renderModeIdx = getIndexFromRenderMode(item.mode);
                                isSceneDirty = true;
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Panel")) {
                ImGui::MenuItem(WND_INSPECTOR, "Ctrl+1", &showInspector);
                ImGui::MenuItem(WND_HIERARCHY, "Ctrl+2", &showHierarchy);
                ImGui::MenuItem(WND_RESOURCES, "Ctrl+4", &showResources);
                ImGui::MenuItem(WND_LOGGER, "Ctrl+5", &showLogger);
                ImGui::MenuItem(WND_STATISTICS, "Ctrl+7", &showStatistics);
                ImGui::MenuItem(WND_VALIDATION, "Ctrl+8", &showValidation);
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_REFRESH " Reset Layout", "Ctrl+L")) {
                    showInspector = showHierarchy = showResources = true;
                    showLogger = showStatistics = showValidation = false;
                    for (auto& vp : viewports) vp->isOpen = true;
                    s_resetLayout = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End(); // End DockSpace

        // -------------------------------------------------------------
        // Synchronize glTF Scene & Animation before Viewports Render
        // -------------------------------------------------------------
        if (animationPlayer.isPlaying()) {
            isSceneDirty = true;
        }

        if (isSceneDirty && gltfLayer != nullptr) {
            gltfLayer->updateGltf(gltfModel);
            if (activeScene != nullptr) {
                gltfLayer->updateScene(gltfModel, *activeScene);
            }
            gltfLayer->applyAnimationTime(animationPlayer.getCurrentTime());
        }

        bool frameSceneDirty = isSceneDirty;
        isSceneDirty = false;

        // -------------------------------------------------------------
        // Viewport Windows (Multi-Viewport)
        // -------------------------------------------------------------
        for (auto& vp : viewports) {
            if (!vp->isOpen) continue;

            std::string wndTitle = std::string(ICON_MDI_CUBE_OUTLINE " ") + vp->name + "###Viewport_" + std::to_string(vp->id);

            ImGui::SetNextWindowDockID(s_centralDockId, ImGuiCond_Appearing);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            bool isVpVisible = ImGui::Begin(wndTitle.c_str(), &vp->isOpen);
            ImGui::PopStyleVar();

            if (vp->requestFocus) {
                ImGui::SetWindowFocus();
                ImGuiWindow* w = ImGui::GetCurrentWindow();
                if (w != nullptr && w->DockNode != nullptr) {
                    w->DockNode->SelectedTabId = w->TabId;
                    if (w->DockNode->TabBar != nullptr) {
                        w->DockNode->TabBar->SelectedTabId = w->TabId;
                        w->DockNode->TabBar->NextSelectedTabId = w->TabId;
                    }
                }
                activeViewport = vp.get();
                vp->requestFocus = false;
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                activeViewport = vp.get();
            }

            if (!isVpVisible) {
                ImGui::End();
                continue;
            }

            // Quick Navigation & Inspection Toolbar Strip
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
            ImGui::BeginGroup();

            // 1. Perspective / Orthographic Toggle (E)
            bool isOrtho = (vp->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic);
            const char* projIcon = isOrtho ? ICON_MDI_PERSPECTIVE_LESS : ICON_MDI_PERSPECTIVE_MORE;
            if (ImGui::SmallButton(projIcon)) {
                if (!vp->isAttachedToCamera) {
                    vp->cameraActor.changeUserCameraType(isOrtho ? ux3d::renderer::Camera::Type::Perspective : ux3d::renderer::Camera::Type::Orthographic);
                    vp->antiAliasing.resetHistory();
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(isOrtho ? "Orthographic (Click to switch to Perspective)" : "Perspective (Click to switch to Orthographic)");
            }

            // 2. Toggle Floor Grid (D)
            ImGui::SameLine();
            bool gridActive = vp->settings.showGrid;
            if (gridActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
            }
            if (ImGui::SmallButton(ICON_MDI_GRID "##GridToggle")) {
                vp->settings.showGrid = !vp->settings.showGrid;
            }
            if (gridActive) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(vp->settings.showGrid ? "Floor Grid (Active - click to hide)" : "Floor Grid (Hidden - click to show)");
            }

            // 3. Toggle Bounding Box (E)
            ImGui::SameLine();
            bool bboxActive = vp->settings.showSceneBoundingBox || vp->settings.showSelectedBoundingBox;
            if (bboxActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
            }
            if (ImGui::SmallButton(ICON_MDI_CUBE_OUTLINE "##BBoxToggle")) {
                bool next = !bboxActive;
                vp->settings.showSceneBoundingBox = next;
                vp->settings.showSelectedBoundingBox = next;
            }
            if (bboxActive) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(bboxActive ? "Bounding Boxes (Active - click to hide)" : "Bounding Boxes (Hidden - click to show)");
            }

            // 4. Toggle Orbit (Turntable)
            ImGui::SameLine();
            bool orbitActive = vp->settings.turntable;
            if (orbitActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
            }
            if (ImGui::SmallButton(ICON_MDI_ROTATE_ORBIT "##OrbitToggle")) {
                vp->settings.turntable = !vp->settings.turntable;
            }
            if (orbitActive) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(vp->settings.turntable ? "Turntable Orbit (Active - click to stop)" : "Turntable Orbit (Stopped - click to start)");
            }

            // Material Variants (if model has variants)
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
                std::string varComboId = "##ToolbarVariant_" + std::to_string(vp->id);
                if (ImGui::BeginCombo(varComboId.c_str(), curVarName.c_str())) {
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

            // Focus (F) button
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_MDI_CROSSHAIRS_GPS " Focus (F)")) {
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    vp->cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                    vp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
                vp->antiAliasing.resetHistory();
            }

            // Right-corner pulldowns: [ Free Look | <camera-list> ] and [ Shading Mode ]
            std::string camPulldownLabel;
            if (vp->isAttachedToCamera) {
                std::string camName = vp->attachedCameraNodeName.empty() ? "Camera" : vp->attachedCameraNodeName;
                camPulldownLabel = std::string(ICON_MDI_CAMERA " ") + camName + " " ICON_MDI_MENU_DOWN;
            } else {
                camPulldownLabel = std::string(ICON_MDI_EYE_OUTLINE " Free Look ") + ICON_MDI_MENU_DOWN;
            }
            float camPulldownWidth = ImGui::CalcTextSize(camPulldownLabel.c_str()).x + 18.0f;

            const char* curModeLabel = getRenderModeLabel(vp->settings.renderMode);
            std::string shadingPulldownLabel = std::string(curModeLabel) + " " ICON_MDI_MENU_DOWN;
            float shadingPulldownWidth = ImGui::CalcTextSize(shadingPulldownLabel.c_str()).x + 18.0f;

            float totalRightWidth = camPulldownWidth + shadingPulldownWidth + ImGui::GetStyle().ItemSpacing.x;
            float rightX = ImGui::GetWindowContentRegionMax().x - totalRightWidth - 8.0f;
            if (rightX > ImGui::GetCursorPosX() + 10.0f) {
                ImGui::SameLine(rightX);
            } else {
                ImGui::SameLine();
            }

            // 1. Camera attachment pulldown [ Free Look | <camera-list> ]
            std::string camPopupId = "##CameraMenuPopup_" + std::to_string(vp->id);
            if (vp->isAttachedToCamera) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.78f, 1.0f, 1.0f));
            }
            if (ImGui::SmallButton(camPulldownLabel.c_str())) {
                ImGui::OpenPopup(camPopupId.c_str());
            }
            if (vp->isAttachedToCamera) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(vp->isAttachedToCamera ? "Camera Attached (Click to switch camera or Free Look)" : "Free Look (Click to attach to a glTF camera)");
            }

            if (ImGui::BeginPopup(camPopupId.c_str())) {
                bool isFreeLook = !vp->isAttachedToCamera;
                if (ImGui::MenuItem(ICON_MDI_EYE_OUTLINE " Free Look", nullptr, isFreeLook)) {
                    vp->isAttachedToCamera = false;
                    vp->attachedCameraNode = nullptr;
                    vp->attachedCameraNodeId = 0;
                    vp->attachedCameraNodeName = "";
                    isSceneDirty = true;
                }
                ImGui::Separator();

                auto cameraNodes = collectSceneCameraNodes(activeScene);
                if (cameraNodes.empty()) {
                    ImGui::TextDisabled("  (No Cameras in Scene)");
                } else {
                    for (const auto* camNode : cameraNodes) {
                        std::string cName = camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                        std::string camItemLabel = ICON_MDI_CAMERA " " + cName;
                        bool isSelected = vp->isAttachedToCamera && (vp->attachedCameraNodeId == camNode->m_ID || vp->attachedCameraNode == camNode);
                        if (ImGui::MenuItem(camItemLabel.c_str(), nullptr, isSelected)) {
                            vp->isAttachedToCamera = true;
                            vp->attachedCameraNode = camNode;
                            vp->attachedCameraNodeId = camNode->m_ID;
                            vp->attachedCameraNodeName = cName;
                            isSceneDirty = true;
                        }
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine();

            // 2. Shading Mode pulldown
            std::string popupId = "##ShadingModePopup_" + std::to_string(vp->id);
            if (ImGui::SmallButton(shadingPulldownLabel.c_str())) {
                ImGui::OpenPopup(popupId.c_str());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Shading & Debug Render Modes");
            }

            if (ImGui::BeginPopup(popupId.c_str())) {
                for (const auto& item : s_shadingModes) {
                    if (item.isSeparator) {
                        ImGui::Separator();
                    } else {
                        bool isSelected = (vp->settings.renderMode == item.mode);
                        if (ImGui::MenuItem(item.label, item.shortcut, isSelected)) {
                            vp->settings.renderMode = item.mode;
                            vp->settings.renderModeIdx = getIndexFromRenderMode(item.mode);
                            isSceneDirty = true;
                        }
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::EndGroup();
            ImGui::PopStyleVar(2);

            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            int vpW = static_cast<int>(viewportSize.x);
            int vpH = static_cast<int>(viewportSize.y);
            if (vpW < 1) vpW = 1;
            if (vpH < 1) vpH = 1;

            auto aaMode = static_cast<ux3d::imgui::AntiAliasingMode>(vp->settings.antiAliasingIdx);
            int desiredSamples = ux3d::imgui::AntiAliasing::getSampleCount(aaMode);

            if (vp->sceneFbo.getWidth() != vpW || vp->sceneFbo.getHeight() != vpH || vp->sceneFbo.getSamples() != desiredSamples) {
                vp->sceneFbo.resize(vpW, vpH, desiredSamples);
                if (gltfLayer != nullptr) {
                    gltfLayer->getResourceManager().createFramebufferTexture(vp->sceneFbo.getColorTexture());
                    gltfLayer->getResourceManager().setBlitFramebufferFunction([] {}, ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
                    gltfLayer->getResourceManager().setTransmissiveTextureFunction([] {});
                }
                vp->antiAliasing.resetHistory();
            }

            if (vp->pickingFbo.getWidth() != vpW || vp->pickingFbo.getHeight() != vpH) {
                vp->pickingFbo.resize(vpW, vpH, 1);
            }

            pickingLayer.prePass = [&]() {
                vp->pickingFbo.bind();
                glViewport(0, 0, vpW, vpH);
            };
            pickingLayer.postPass = [&]() {
                vp->sceneFbo.bind();
                glViewport(0, 0, vpW, vpH);
            };

            // Clear picking buffer
            if (vp->pickingFbo.isValid()) {
                vp->pickingFbo.bind();
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            // Render to offscreen Framebuffer
            vp->sceneFbo.bind();
            glViewport(0, 0, vpW, vpH);
            if (vp->settings.showBackground) {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            } else {
                glClearColor(vp->settings.clearColor.r, vp->settings.clearColor.g, vp->settings.clearColor.b, vp->settings.clearColor.a);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float aspect = (vpH > 0) ? (static_cast<float>(vpW) / static_cast<float>(vpH)) : 1.0f;
            if (gltfLayer != nullptr) {
                gltfLayer->viewportAspectRatio = aspect;
            }

            glm::mat4 view(1.0f);
            glm::mat4 proj(1.0f);
            glm::vec3 cameraEye(0.0f, 0.0f, 1.0f);
            float cameraZoom = 1.0f;

            if (vp->isAttachedToCamera && vp->attachedCameraNode != nullptr) {
                gltfLayer->setActiveGltfCameraByNode(vp->attachedCameraNode);
                auto* renCam = gltfLayer->getActiveCamera();
                if (renCam != nullptr) {
                    renCam->setViewportResolution(vpW, vpH);
                    renCam->setAspectRatio(aspect);
                    view = renCam->getViewMatrix();
                    proj = renCam->getProjectionMatrix();
                    cameraEye = glm::vec3(renCam->getPositon().x, renCam->getPositon().y, renCam->getPositon().z);
                    cameraZoom = renCam->getZoom();
                } else {
                    vp->rendererCamera.setViewportResolution(vpW, vpH);
                    vp->rendererCamera.setAspectRatio(aspect);
                    vp->cameraActor.updateUserCameraTransform();
                    gltfLayer->setActiveCamera(&vp->rendererCamera);
                    view = vp->rendererCamera.getViewMatrix();
                    proj = vp->rendererCamera.getProjectionMatrix();
                    cameraEye = glm::vec3(vp->cameraActor.getEyePosition().x, vp->cameraActor.getEyePosition().y, vp->cameraActor.getEyePosition().z);
                    cameraZoom = vp->cameraActor.getZoom();
                }
            } else {
                vp->rendererCamera.setViewportResolution(vpW, vpH);
                vp->rendererCamera.setAspectRatio(aspect);
                vp->cameraActor.updateUserCameraTransform();
                gltfLayer->setActiveCamera(&vp->rendererCamera);
                view = vp->rendererCamera.getViewMatrix();
                proj = vp->rendererCamera.getProjectionMatrix();
                cameraEye = glm::vec3(vp->cameraActor.getEyePosition().x, vp->cameraActor.getEyePosition().y, vp->cameraActor.getEyePosition().z);
                cameraZoom = vp->cameraActor.getZoom();
            }

            // Subpixel jitter for TAA
            glm::vec2 jitter = vp->antiAliasing.getJitter(aaMode, vpW, vpH);
            glm::mat4 jitterMat = (aaMode == ux3d::imgui::AntiAliasingMode::TAA)
                ? glm::translate(glm::mat4(1.0f), glm::vec3(2.0f * jitter.x, 2.0f * jitter.y, 0.0f))
                : glm::mat4(1.0f);
            glm::mat4 jitteredProj = jitterMat * proj;
            glm::mat4 viewProj = jitteredProj * view;

            if (aaMode == ux3d::imgui::AntiAliasingMode::TAA) {
                if (gltfLayer->getActiveCamera()) {
                    gltfLayer->getActiveCamera()->setProjection(ux3d::math::mat4x4(jitteredProj));
                }
            }

            // 1. Render glTF scene & background skybox
            if (gltfLayer != nullptr && glRenderer != nullptr && commandGenerator != nullptr) {
                gltfLayer->enableCubeMap(vp->settings.showBackground);

                auto mode = vp->settings.renderMode;

                // Apply Face Culling override and Debug Mode cleanly via materialMapping
                for (auto& [gltfMat, renMat] : gltfLayer->m_materialMapping.map) {
                    if (renMat != nullptr) {
                        if (vp->settings.forceDoubleSided) {
                            renMat->isDoubleSided = true;
                        } else {
                            renMat->isDoubleSided = gltfMat ? gltfMat->isDoubleSidedOrDefault() : false;
                        }
                        renMat->defines.define("DEBUG", static_cast<int>(mode));
                    }
                }

                for (auto& [gltfTuple, renMesh] : gltfLayer->m_meshMapping.map) {
                    if (renMesh != nullptr) {
                        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;
                        auto nodeIt = gltfLayer->m_transformHierarchyMapping.map.find(gltfNode);
                        float det = 1.0f;
                        if (nodeIt != gltfLayer->m_transformHierarchyMapping.map.end() && nodeIt->second != nullptr) {
                            det = ux3d::math::determinant(nodeIt->second->getWorldTransform());
                        }
                        bool baseClockwise = (det < 0.0f);
                        bool isClockwise = baseClockwise ^ vp->settings.invertWinding;
                        renMesh->frontFace = isClockwise ? ux3d::renderer::gl::Winding::Clockwise : ux3d::renderer::gl::Winding::CounterClockwise;

                        renMesh->polygonMode = (mode == ux3d::renderer::RenderMode::Wireframe)
                            ? ux3d::renderer::gl::PolygonMode::Line
                            : ux3d::renderer::gl::PolygonMode::Fill;
                        renMesh->defines.define("DEBUG", static_cast<int>(mode));
                    }
                }

                auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
                currentReverseMapping = reverseMapping;

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
                        if (vp->settings.forceDoubleSided) {
                            material->isDoubleSided = true;
                        }
                    }
                }

                // Apply GPU picking layer
                scene = pickingLayer.applyLayer(std::move(scene));

                // Continuously synchronize viewport lighting, exposure, and environment parameters
                commandGenerator->setExposure(std::pow(2.0f, vp->settings.exposureEV));
                commandGenerator->setEnvironmentMapIntensity(vp->settings.hdrIntensity);
                commandGenerator->setEnvironmentMapRotation(vp->settings.hdrRotation);
                commandGenerator->setEnvironmentMapBlurNormalized(vp->settings.hdrBlur);
                commandGenerator->setIBLEnabled(vp->settings.iblEnabled);
                commandGenerator->setPunctualLightsEnabled(vp->settings.punctualLightsEnabled);

                commandGenerator->update(scene);
                glRenderer->draw({ commandGenerator.get() });
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // Restore unjittered camera projection so subsequent passes and next frame are clean
                if (aaMode == ux3d::imgui::AntiAliasingMode::TAA && gltfLayer->getActiveCamera()) {
                    gltfLayer->getActiveCamera()->setProjection(ux3d::math::mat4x4(proj));
                }

                if (vp->settings.wireframeOverlay && mode != ux3d::renderer::RenderMode::Wireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glEnable(GL_POLYGON_OFFSET_LINE);
                    glPolygonOffset(-1.0f, -1.0f);
                    glLineWidth(1.0f);
                    auto [wireScene, _] = gltfLayer->createScene(activeScene);
                    for (auto& [mesh, material] : wireScene.drawables) {
                        if (mesh != nullptr && mesh != cubeMesh) {
                            mesh->polygonMode = ux3d::renderer::gl::PolygonMode::Line;
                        }
                    }
                    commandGenerator->update(wireScene);
                    glRenderer->draw({ commandGenerator.get() });
                    glDisable(GL_POLYGON_OFFSET_LINE);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
            } else {
                currentReverseMapping.clear();
            }

            // 2. Render ground grid
            if (vp->settings.showGrid) {
                gridRenderer.draw(view, (aaMode == ux3d::imgui::AntiAliasingMode::TAA ? jitteredProj : proj), ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z), cameraZoom);
            }

            // 3. Render bounding boxes
            if (vp->settings.showSceneBoundingBox && activeScene != nullptr && !activeScene->getNodes().empty()) {
                ux3d::gltf::Extends ext = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
                bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                                  glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                                  glm::vec4(1.0f, 0.85f, 0.2f, 0.85f),
                                  viewProj);
            }
            if (vp->settings.showSelectedBoundingBox && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                NodeOBB obb = getNodeOBB(selection.selectedNode, gltfLayer.get());
                if (obb.valid) {
                    bboxRenderer.drawOBB(obb.localMin,
                                         obb.localMax,
                                         obb.modelMatrix,
                                         glm::vec4(1.0f, 0.62f, 0.12f, 1.0f),
                                         viewProj);
                }
            }

            vp->sceneFbo.unbind();

            // 4. Post-processing Anti-Aliasing pass
            GLuint displayTexture = vp->antiAliasing.render(aaMode,
                                                            vp->sceneFbo.getColorTexture(),
                                                            vpW, vpH,
                                                            proj * view,
                                                            frameSceneDirty);

            // Draw offscreen FBO texture inside ImGui viewport window
            if (displayTexture != 0) {
                ImGui::Image(static_cast<ImTextureID>(displayTexture),
                             viewportSize,
                             ImVec2(0.0f, 1.0f),
                             ImVec2(1.0f, 0.0f));
            }

            ImVec2 imagePos = ImGui::GetItemRectMin();
            ImVec2 imageSize = ImGui::GetItemRectSize();

            bool clickedCompass = false;

            // 3D Viewport Orientation Compass / Navigation Gizmo
            if (vp->settings.showOrientationGizmo) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                float radius = 35.0f;
                ImVec2 center = ImVec2(imagePos.x + imageSize.x - radius - 15.0f, imagePos.y + radius + 15.0f);

                // Background subtle disc
                drawList->AddCircleFilled(center, radius + 4.0f, IM_COL32(28, 30, 38, 190));
                drawList->AddCircle(center, radius + 4.0f, IM_COL32(70, 75, 95, 180), 0, 1.5f);

                struct AxisDef {
                    glm::vec3 dir;
                    const char* name;
                    ImU32 color;
                    ImU32 dimColor;
                    ux3d::editor::CameraOrientation preset;
                };

                AxisDef axisDefs[] = {
                    { glm::vec3( 1.0f,  0.0f,  0.0f), "X",  IM_COL32(235, 75, 75, 255),   IM_COL32(140, 40, 40, 160), ux3d::editor::CameraOrientation::Right },
                    { glm::vec3(-1.0f,  0.0f,  0.0f), "-X", IM_COL32(180, 70, 70, 200),  IM_COL32(100, 35, 35, 120), ux3d::editor::CameraOrientation::Left },
                    { glm::vec3( 0.0f,  1.0f,  0.0f), "Y",  IM_COL32(75, 220, 95, 255),   IM_COL32(40, 130, 50, 160), ux3d::editor::CameraOrientation::Top },
                    { glm::vec3( 0.0f, -1.0f,  0.0f), "-Y", IM_COL32(70, 170, 85, 200),  IM_COL32(35, 95, 45, 120),  ux3d::editor::CameraOrientation::Bottom },
                    { glm::vec3( 0.0f,  0.0f,  1.0f), "Z",  IM_COL32(70, 140, 245, 255),  IM_COL32(40, 80, 150, 160), ux3d::editor::CameraOrientation::Front },
                    { glm::vec3( 0.0f,  0.0f, -1.0f), "-Z", IM_COL32(65, 115, 200, 200), IM_COL32(35, 65, 120, 120), ux3d::editor::CameraOrientation::Back }
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

                // Middle part / center hub -> click turns into isometric mode!
                float hubRadius = 7.0f;
                bool hoveredHub = (distCenterSq <= (hubRadius + 2.0f) * (hubRadius + 2.0f));

                drawList->AddCircleFilled(center, hubRadius, hoveredHub ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 165, 185, 220));
                drawList->AddCircle(center, hubRadius, IM_COL32(35, 40, 55, 255), 0, 1.2f);

                if (hoveredHub) {
                    ImGui::SetTooltip("Isometric View");
                    if (clicked && !vp->isAttachedToCamera) {
                        vp->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Isometric);
                        vp->antiAliasing.resetHistory();
                    }
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

                    if (hovered && clicked && !hoveredHub && !vp->isAttachedToCamera) {
                        vp->cameraActor.orientUserCamera(pa.def->preset);
                        vp->antiAliasing.resetHistory();
                    }
                }
            }

            // Viewport mouse interaction & Picking
            bool isMouseInViewport = (io.MousePos.x >= imagePos.x && io.MousePos.x < imagePos.x + imageSize.x &&
                                      io.MousePos.y >= imagePos.y && io.MousePos.y < imagePos.y + imageSize.y);
            bool isViewportHovered = isMouseInViewport && ImGui::IsWindowHovered();

            if (isViewportHovered && !clickedCompass) {
                // Only allow orbit / pan / zoom if not attached to glTF camera
                if (!vp->isAttachedToCamera) {
                    if (io.MouseWheel != 0.0f) {
                        vp->cameraActor.zoomCamera(io.MouseWheel * 120.0f);
                    }
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                        if (io.KeyShift) {
                            vp->cameraActor.panCamera(-static_cast<int32_t>(io.MouseDelta.x), -static_cast<int32_t>(io.MouseDelta.y));
                        } else if (io.KeyCtrl) {
                            vp->cameraActor.zoomCamera(-io.MouseDelta.y * 10.0f);
                        } else {
                            vp->cameraActor.rotateCamera(static_cast<int32_t>(io.MouseDelta.x), static_cast<int32_t>(io.MouseDelta.y));
                        }
                    }
                }

                // Left-click 3D mesh picking
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
                    float localX = io.MousePos.x - imagePos.x;
                    float localY = io.MousePos.y - imagePos.y;
                    int px = static_cast<int>(localX);
                    int py = vpH - static_cast<int>(localY) - 1;

                    if (px < 0) px = 0;
                    if (px >= vpW) px = vpW - 1;
                    if (py < 0) py = 0;
                    if (py >= vpH) py = vpH - 1;

                    if (vp->pickingFbo.isValid()) {
                        uint8_t pixel[4] = { 0, 0, 0, 0 };
                        glPixelStorei(GL_PACK_ALIGNMENT, 1);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->pickingFbo.getFbo());
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                        glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                        uint32_t pickingId = (static_cast<uint32_t>(pixel[0]) << 16) |
                                             (static_cast<uint32_t>(pixel[1]) << 8)  |
                                              static_cast<uint32_t>(pixel[2]);

                        if (pickingId != 0) {
                            auto* pickedMesh = pickingLayer.getMesh(pickingId);
                            if (pickedMesh != nullptr) {
                                auto it = currentReverseMapping.find(pickedMesh);
                                if (it != currentReverseMapping.end() && it->second != nullptr) {
                                    auto* pickedNode = const_cast<ux3d::gltf::Node*>(it->second);
                                    selection.selectNode(pickedNode);
                                    ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                                    isSceneDirty = true;
                                    logMessage("Picked: " + (pickedNode->hasName() ? pickedNode->getName() : ("Node " + std::to_string(pickedNode->m_ID))));
                                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                        focusCameraOnNode(pickedNode, gltfModel, gltfLayer.get(), vp->cameraActor, vp->antiAliasing, isSceneDirty);
                                    }
                                } else {
                                    selection = ux3d::imgui::SelectionState();
                                    isSceneDirty = true;
                                }
                            } else {
                                selection = ux3d::imgui::SelectionState();
                                isSceneDirty = true;
                            }
                        } else {
                            selection = ux3d::imgui::SelectionState();
                            isSceneDirty = true;
                        }
                    }
                }
            }

            // 3D Coordinate Gizmo
            if (vp->settings.showNodeCoordinateGizmo && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr && vp->sceneFbo.isValid()) {
                auto* node = selection.selectedNode;
                glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawNodeCoordinateGizmo(drawList, nodeWorld, viewProj, imagePos, imageSize, ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z), vp->settings.gizmoAxisConvention);
            }

            ImGui::End(); // End Viewport Window
        }

        // -------------------------------------------------------------
        // Hierarchy Panel
        // -------------------------------------------------------------
        if (showHierarchy) {
            ux3d::editor::CameraActor dummyActor;
            ux3d::imgui::AntiAliasing dummyAA;
            ux3d::imgui::SceneHierarchyPanel::render(
                gltfModel, activeScene, selection, gltfLayer.get(),
                activeViewport ? activeViewport->cameraActor : dummyActor,
                activeViewport ? activeViewport->antiAliasing : dummyAA,
                isSceneDirty, &showHierarchy, &viewports, &activeViewport
            );
        }

        // -------------------------------------------------------------
        // Resources Panel
        // -------------------------------------------------------------
        if (showResources) {
            ux3d::imgui::ResourcesPanel::render(gltfModel, selection, gltfLayer.get(), &isSceneDirty, &showResources);
        }

        // -------------------------------------------------------------
        // Inspector Panel
        // -------------------------------------------------------------
        if (showInspector) {
            if (ImGui::Begin(WND_INSPECTOR, &showInspector)) {
                ux3d::imgui::ElementInspector::render(selection, gltfModel, isSceneDirty, &animationPlayer, gltfLayer.get(), &viewports, commandGenerator.get());
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Statistics Panel
        // -------------------------------------------------------------
        if (showStatistics) {
            if (ImGui::Begin(WND_STATISTICS, &showStatistics)) {
                ux3d::imgui::StatisticsPanel::render(gltfModel, currentFilePath);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Validation Panel
        // -------------------------------------------------------------
        if (showValidation) {
            ux3d::editor::CameraActor dummyActor;
            ux3d::imgui::ValidationPanel::render(&showValidation, currentFilePath, gltfModel, selection, activeViewport ? activeViewport->cameraActor : dummyActor, gltfLayer.get());
        }

        // -------------------------------------------------------------
        // Search Paths Window
        // -------------------------------------------------------------
        if (showSearchPaths) {
            ux3d::imgui::renderSearchPathsWindow(&showSearchPaths, [&]() {
                if (!currentFilePath.empty()) {
                    loadGltfModel(currentFilePath, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                }
            });
        }

        // -------------------------------------------------------------
        // Logger Panel
        // -------------------------------------------------------------
        if (showLogger) {
            if (ImGui::Begin(WND_LOGGER, &showLogger)) {
                static bool s_loggerWrap = false;
                static bool s_loggerAutoScroll = true;

                if (ImGui::Button(ICON_MDI_DELETE_OUTLINE " Clear")) {
                    s_consoleLogs.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy")) {
                    std::string allLogs;
                    for (const auto& entry : s_consoleLogs) {
                        allLogs += entry.text;
                        allLogs += "\n";
                    }
                    ImGui::SetClipboardText(allLogs.c_str());
                }
                ImGui::SameLine();
                ImGui::Checkbox(ICON_MDI_WRAP " Wrap", &s_loggerWrap);
                ImGui::SameLine();
                ImGui::Checkbox("Auto-Scroll", &s_loggerAutoScroll);
                ImGui::SameLine();
                ImGui::PushFont(ux3d::imgui::getMonoFont());
                ImGui::TextDisabled("| Messages: %zu", s_consoleLogs.size());
                ImGui::PopFont();
                ImGui::Separator();

                ImGuiWindowFlags childFlags = 0;
                if (!s_loggerWrap) {
                    childFlags |= ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
                }

                ImGui::BeginChild("LoggerScrollArea", ImVec2(0, 0), false, childFlags);
                if (s_loggerWrap) {
                    ImGui::PushTextWrapPos(0.0f);
                }
                ImGui::PushFont(ux3d::imgui::getMonoFont());
                for (const auto& entry : s_consoleLogs) {
                    ImGui::TextColored(entry.color, "%s", entry.text.c_str());
                }
                ImGui::PopFont();
                if (s_loggerWrap) {
                    ImGui::PopTextWrapPos();
                }
                if (s_loggerAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
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

        // -------------------------------------------------------------
        // Application Status Bar (Bottom)
        // -------------------------------------------------------------
        ux3d::imgui::StatusBar::render(gltfModel, gltfLayer.get(), animationPlayer, isSceneDirty, showLogger, showValidation, s_consoleLogs, activeViewport ? &activeViewport->antiAliasing : nullptr);

        // Rendering main window
        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (dumpFrameCountdown > 0) {
            dumpFrameCountdown--;
            if (dumpFrameCountdown == 0 && !dumpFramePath.empty()) {
                std::vector<uint8_t> pixels(displayW * displayH * 4);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, displayW, displayH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                saveImage(dumpFramePath.c_str(), displayW, displayH, pixels.data());
                std::printf("Dumped window frame to %s\n", dumpFramePath.c_str());
                if (exitAfterDump) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ux3d::imgui::ElementInspector::clearPreviewCache();
    gridRenderer.cleanup();
    viewports.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    ux3d::core::removeLoggerCallback(&consoleLogger);

    saveWindowState(window);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return main(__argc, __argv);
}
#endif
