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
#include "imgui/ResourcesPanel.h"
#include "imgui/ElementInspector.h"
#include "imgui/AnimationPlayer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/BoundingBoxRenderer.h"
#include "imgui/Theme.h"
#include "imgui/IconsMaterialDesignIcons.h"

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

// Recursive scene hierarchy node renderer with single-click focus/orbit
static void renderNodeHierarchy(ux3d::gltf::Node* node,
                                ux3d::imgui::SelectionState& selection,
                                ux3d::gltf::Gltf& gltfModel,
                                ux3d::renderer::GltfRenderLayer* gltfLayer,
                                ux3d::editor::CameraActor& cameraActor,
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
    if (node->getMesh() != nullptr) tag += " [" ICON_MDI_CUBE_OUTLINE " Mesh]";
    if (node->getCamera() != nullptr) tag += " [" ICON_MDI_CAMERA " Camera]";
    if (node->getLight() != nullptr) tag += " [" ICON_MDI_LIGHTBULB_OUTLINE " Light]";
    std::string label = name + tag;

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
    }

    if (open && !node->getChildren().empty()) {
        for (auto* child : node->getChildren()) {
            renderNodeHierarchy(child, selection, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
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
        std::filesystem::path dir = std::filesystem::path(appData) / "Gestaltor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return (dir / "window_state.txt").string();
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
                          ux3d::editor::CameraActor& cameraActor,
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
            cameraActor.fitCameraToScene(*activeScene, &gltfModel);
            ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
            char boundsBuf[256];
            std::snprintf(boundsBuf, sizeof(boundsBuf), "Scene Bounds: [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]",
                          extends.min.x, extends.min.y, extends.min.z,
                          extends.max.x, extends.max.y, extends.max.z);
            logMessage(boundsBuf, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
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
        default: return ux3d::renderer::RenderMode::None;
    }
}

static const char* const WND_VIEWPORT           = ICON_MDI_CUBE_OUTLINE " 3D Viewport";
static const char* const WND_HIERARCHY          = ICON_MDI_FILE_TREE " Scene Hierarchy";
static const char* const WND_RESOURCES          = ICON_MDI_FOLDER_MULTIPLE_OUTLINE " Resources";
static const char* const WND_INSPECTOR          = ICON_MDI_TUNE " Inspector";
static const char* const WND_VIEWPORT_SETTINGS = ICON_MDI_COG_OUTLINE " Viewport Settings";
static const char* const WND_LOGGER             = ICON_MDI_CONSOLE " Logger";
static const char* const WND_STATISTICS         = ICON_MDI_CHART_BAR " Statistics";

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

        ImGui::DockBuilderDockWindow(WND_VIEWPORT, dock_main_id);
        ImGui::DockBuilderDockWindow(WND_LOGGER, dock_center_bottom);
        ImGui::DockBuilderDockWindow(WND_HIERARCHY, dock_left);
        ImGui::DockBuilderDockWindow(WND_RESOURCES, dock_left_bottom);
        ImGui::DockBuilderDockWindow(WND_INSPECTOR, dock_right);

        // Dock optional panels in center-bottom if opened later
        ImGui::DockBuilderDockWindow(WND_VIEWPORT_SETTINGS, dock_center_bottom);
        ImGui::DockBuilderDockWindow(WND_STATISTICS, dock_center_bottom);

        ImGui::DockBuilderFinish(dockspaceId);
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
    GLFWwindow* window = glfwCreateWindow(winState.width, winState.height, "Gestaltor - glTF 2.0 Visual Editor", nullptr, nullptr);
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
    ux3d::imgui::Framebuffer sceneFbo;
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

    ux3d::renderer::PickingRenderLayer pickingLayer(gladGL.get());
    ux3d::imgui::Framebuffer pickingFbo;
    ux3d::renderer::ReverseMapping currentReverseMapping;

    ux3d::renderer::Camera rendererCamera(gladGL.get());
    ux3d::editor::CameraActor cameraActor;
    cameraActor.setCamera(&rendererCamera);

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
    std::string selectMaterialArg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-frame" && i + 1 < argc) {
            dumpFramePath = argv[++i];
            dumpFrameCountdown = 8;
        } else if (arg == "--exit-after-dump") {
            exitAfterDump = true;
        } else if (arg == "--select-node" && i + 1 < argc) {
            selectNodeArg = argv[++i];
        } else if (arg == "--select-material" && i + 1 < argc) {
            selectMaterialArg = argv[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            modelToLoad = arg;
        }
    }

    s_recentFiles = loadRecentFiles();

    if (ux3d::imgui::StdFileSystem::instance().isFile(ux3d::imgui::StdFileSystem::instance().toFile(modelToLoad.c_str()))) {
        loadGltfModel(modelToLoad, gltfModel, activeScene, selection, currentFilePath, cameraActor, gltfLayer.get(), animationPlayer, isSceneDirty);
        updateWindowTitle(window, currentFilePath);
        if (!selectNodeArg.empty()) {
            for (auto& node : gltfModel.getNodes()) {
                if (node.hasName() && node.getName() == selectNodeArg.c_str()) {
                    selection.selectNode(&node);
                    focusCameraOnNode(&node, gltfModel, gltfLayer.get(), cameraActor, antiAliasing, isSceneDirty);
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
    static bool showViewportSettings = false;
    static bool showStatistics = false;

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
            cameraActor.rotateCamera(static_cast<int32_t>(viewportSettings.turntableSpeed * deltaTime * 20.0f), 0);
        }

        // Keyboard shortcuts
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                if (activeScene != nullptr) cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                antiAliasing.resetHistory();
            }

            // Numpad Camera Presets (Gestaltor style)
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1)) {
                cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Back : ux3d::editor::CameraOrientation::Front);
                antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad3)) {
                cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Left : ux3d::editor::CameraOrientation::Right);
                antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad7)) {
                cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Bottom : ux3d::editor::CameraOrientation::Top);
                antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad5)) {
                cameraActor.changeUserCameraType((rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic) ? ux3d::renderer::Camera::Type::Perspective : ux3d::renderer::Camera::Type::Orthographic);
                antiAliasing.resetHistory();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal)) {
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                    cameraActor.fitCameraToScene(*activeScene, &gltfModel);
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

        ImGuiID dockspaceId = ImGui::GetID("GestaltorMainDockSpace_v3");

        static bool s_firstLayout = true;
        static bool s_resetLayout = false;
        if (s_firstLayout || s_resetLayout) {
            bool force = s_resetLayout;
            s_firstLayout = false;
            s_resetLayout = false;
            setupDefaultDockLayout(dockspaceId, mainViewport->WorkSize, force);
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

        // Global Keyboard Shortcuts (File & View)
        if (!io.WantTextInput && io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_O)) {
                auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                if (!openResult.empty()) {
                    loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, cameraActor, gltfLayer.get(), animationPlayer, isSceneDirty);
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
                showInspector = showHierarchy = showViewport = showResources = showLogger = true;
                showViewportSettings = showStatistics = false;
                s_resetLayout = true;
            }
        }

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open ...", "Ctrl+O")) {
                    auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                    if (!openResult.empty()) {
                        loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, cameraActor, gltfLayer.get(), animationPlayer, isSceneDirty);
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
                            loadGltfModel(fileToOpen, gltfModel, activeScene, selection, currentFilePath, cameraActor, gltfLayer.get(), animationPlayer, isSceneDirty);
                            updateWindowTitle(window, currentFilePath);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_EXIT_TO_APP " Exit")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem(ICON_MDI_HOME_OUTLINE " Reset Camera", "Home")) {
                    if (activeScene != nullptr) cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
                std::string fitLabel = std::string(ICON_MDI_CROSSHAIRS_GPS " ") +
                    ((selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) ? "Fit to Selection" : "Fit to Scene");
                if (ImGui::MenuItem(fitLabel.c_str(), "F", false, activeScene != nullptr)) {
                    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                        cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                    } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                        cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem(WND_INSPECTOR, "Ctrl+1", &showInspector);
                ImGui::MenuItem(WND_HIERARCHY, "Ctrl+2", &showHierarchy);
                ImGui::MenuItem(WND_VIEWPORT, "Ctrl+3", &showViewport);
                ImGui::MenuItem(WND_RESOURCES, "Ctrl+4", &showResources);
                ImGui::MenuItem(WND_LOGGER, "Ctrl+5", &showLogger);
                ImGui::MenuItem(WND_VIEWPORT_SETTINGS, "Ctrl+6", &showViewportSettings);
                ImGui::MenuItem(WND_STATISTICS, "Ctrl+7", &showStatistics);
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_REFRESH " Reset Layout", "Ctrl+L")) {
                    showInspector = showHierarchy = showViewport = showResources = showLogger = true;
                    showViewportSettings = showStatistics = false;
                    s_resetLayout = true;
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
            ImGui::Begin(WND_VIEWPORT, &showViewport);
            ImGui::PopStyleVar();

        // Animation Player Toolbar at top of viewport
        animationPlayer.renderToolbar(gltfModel, gltfLayer.get(), isSceneDirty);

        // Quick Navigation & Inspection Toolbar Strip
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
        ImGui::BeginGroup();
        if (ImGui::SmallButton("Front")) { cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Front); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Back")) { cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Back); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Right")) { cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Right); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Left")) { cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Left); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Top")) { cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Top); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Iso")) { cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Isometric); antiAliasing.resetHistory(); }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const char* projLabel = (rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic) ? ICON_MDI_CAMERA " Ortho" : ICON_MDI_CAMERA " Persp";
        if (ImGui::SmallButton(projLabel)) {
            cameraActor.changeUserCameraType((rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic) ? ux3d::renderer::Camera::Type::Perspective : ux3d::renderer::Camera::Type::Orthographic);
            antiAliasing.resetHistory();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(viewportSettings.turntable ? ICON_MDI_ROTATE_3D " [Turntable ON]" : ICON_MDI_ROTATE_3D " Turntable")) {
            viewportSettings.turntable = !viewportSettings.turntable;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(viewportSettings.wireframeOverlay ? ICON_MDI_VECTOR_SQUARE " [Wire ON]" : ICON_MDI_VECTOR_SQUARE " Wire")) {
            viewportSettings.wireframeOverlay = !viewportSettings.wireframeOverlay;
            isSceneDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MDI_CROSSHAIRS_GPS " Focus (F)")) {
            if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
            } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                cameraActor.fitCameraToScene(*activeScene, &gltfModel);
            }
            antiAliasing.resetHistory();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(viewportSettings.showSceneBoundingBox ? ICON_MDI_CUBE_SCAN " [BBox ON]" : ICON_MDI_CUBE_SCAN " BBox")) {
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

        ImVec2 viewportPos = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        int vpW = static_cast<int>(viewportSize.x);
        int vpH = static_cast<int>(viewportSize.y);
        if (vpW < 1) vpW = 1;
        if (vpH < 1) vpH = 1;

        auto aaMode = static_cast<ux3d::imgui::AntiAliasingMode>(viewportSettings.antiAliasingIdx);
        int desiredSamples = ux3d::imgui::AntiAliasing::getSampleCount(aaMode);

        if (sceneFbo.getWidth() != vpW || sceneFbo.getHeight() != vpH || sceneFbo.getSamples() != desiredSamples) {
            sceneFbo.resize(vpW, vpH, desiredSamples);
            if (gltfLayer != nullptr) {
                gltfLayer->getResourceManager().createFramebufferTexture(sceneFbo.getColorTexture());
                gltfLayer->getResourceManager().setBlitFramebufferFunction([] {}, ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
                gltfLayer->getResourceManager().setTransmissiveTextureFunction([] {});
            }
            antiAliasing.resetHistory();
        }

        if (pickingFbo.getWidth() != vpW || pickingFbo.getHeight() != vpH) {
            pickingFbo.resize(vpW, vpH, 1);
        }

        pickingLayer.prePass = [&]() {
            pickingFbo.bind();
            glViewport(0, 0, vpW, vpH);
        };
        pickingLayer.postPass = [&]() {
            sceneFbo.bind();
            glViewport(0, 0, vpW, vpH);
        };

        // Clear picking buffer
        if (pickingFbo.isValid()) {
            pickingFbo.bind();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

        float aspect = (vpH > 0) ? (static_cast<float>(vpW) / static_cast<float>(vpH)) : 1.0f;
        rendererCamera.setViewportResolution(vpW, vpH);
        rendererCamera.setAspectRatio(aspect);
        if (gltfLayer != nullptr) {
            gltfLayer->viewportAspectRatio = aspect;
        }

        cameraActor.updateUserCameraTransform();

        glm::mat4 view = rendererCamera.getViewMatrix();
        glm::mat4 proj = rendererCamera.getProjectionMatrix();

        // Subpixel jitter for TAA
        glm::vec2 jitter = antiAliasing.getJitter(aaMode, vpW, vpH);
        glm::mat4 jitterMat = (aaMode == ux3d::imgui::AntiAliasingMode::TAA)
            ? glm::translate(glm::mat4(1.0f), glm::vec3(2.0f * jitter.x, 2.0f * jitter.y, 0.0f))
            : glm::mat4(1.0f);
        glm::mat4 jitteredProj = jitterMat * proj;
        glm::mat4 viewProj = jitteredProj * view;

        if (aaMode == ux3d::imgui::AntiAliasingMode::TAA) {
            rendererCamera.setProjection(ux3d::math::mat4x4(jitteredProj));
        }

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
            // Standard glTF winding is CounterClockwise (CW when determinant < 0 for mirrored geometry).
            // Ticking "Invert Winding Order" in Viewport Settings inverts the winding order if needed.
            for (auto& [gltfTuple, renMesh] : gltfLayer->m_meshMapping.map) {
                if (renMesh != nullptr) {
                    auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;
                    auto nodeIt = gltfLayer->m_transformHierarchyMapping.map.find(gltfNode);
                    float det = 1.0f;
                    if (nodeIt != gltfLayer->m_transformHierarchyMapping.map.end() && nodeIt->second != nullptr) {
                        det = ux3d::math::determinant(nodeIt->second->getWorldTransform());
                    }
                    bool baseClockwise = (det < 0.0f);
                    bool isClockwise = baseClockwise ^ viewportSettings.invertWinding;
                    renMesh->frontFace = isClockwise ? ux3d::renderer::gl::Winding::Clockwise : ux3d::renderer::gl::Winding::CounterClockwise;

                    renMesh->polygonMode = (mode == ux3d::renderer::RenderMode::Wireframe)
                        ? ux3d::renderer::gl::PolygonMode::Line
                        : ux3d::renderer::gl::PolygonMode::Fill;
                    renMesh->defines.define("DEBUG", static_cast<int>(mode));
                }
            }

            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            currentReverseMapping = reverseMapping;

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

            // Apply Gestaltor native GPU picking layer
            scene = pickingLayer.applyLayer(std::move(scene));

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

        // 2. Render ground grid (infinite fading ground plane)
        if (viewportSettings.showGrid) {
            gridRenderer.draw(view, proj, cameraActor.getEyePosition(), cameraActor.getZoom());
        }

        // 3. Render bounding boxes
        if (viewportSettings.showSceneBoundingBox && activeScene != nullptr && !activeScene->getNodes().empty()) {
            ux3d::gltf::Extends ext = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
            bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                              glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                              glm::vec4(1.0f, 0.85f, 0.2f, 0.85f),
                              viewProj);
        }
        if (viewportSettings.showSelectedBoundingBox && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
            NodeOBB obb = getNodeOBB(selection.selectedNode, gltfLayer.get());
            if (obb.valid) {
                // Orange oriented bounding box matching Blender source
                bboxRenderer.drawOBB(obb.localMin,
                                     obb.localMax,
                                     obb.modelMatrix,
                                     glm::vec4(1.0f, 0.62f, 0.12f, 1.0f),
                                     viewProj);
            }
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

        ImVec2 imagePos = ImGui::GetItemRectMin();
        ImVec2 imageSize = ImGui::GetItemRectSize();

        bool clickedCompass = false;

        // 3D Viewport Orientation Compass / Navigation Gizmo
        if (viewportSettings.showOrientationGizmo) {
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
                    cameraActor.orientUserCamera(pa.def->preset);
                    antiAliasing.resetHistory();
                }
            }
        }

        // Viewport mouse interaction & Picking
        bool isMouseInViewport = (io.MousePos.x >= imagePos.x && io.MousePos.x < imagePos.x + imageSize.x &&
                                  io.MousePos.y >= imagePos.y && io.MousePos.y < imagePos.y + imageSize.y);
        bool isViewportHovered = isMouseInViewport && ImGui::IsWindowHovered();

        if (isViewportHovered && !clickedCompass) {
            // [3] Mouse scroll to zoom
            if (io.MouseWheel != 0.0f) {
                cameraActor.zoomCamera(io.MouseWheel * 120.0f);
            }

            // [1] Middle drag to rotate view & [2] Shift-middle drag to pan view
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                if (io.KeyShift) {
                    // [2] Shift + Middle: Pan view
                    cameraActor.panCamera(-static_cast<int32_t>(io.MouseDelta.x), -static_cast<int32_t>(io.MouseDelta.y));
                } else if (io.KeyCtrl) {
                    // Ctrl + Middle: Smooth zoom
                    cameraActor.zoomCamera(-io.MouseDelta.y * 10.0f);
                } else {
                    // [1] Middle drag alone: Rotate / Orbit view
                    cameraActor.rotateCamera(static_cast<int32_t>(io.MouseDelta.x), static_cast<int32_t>(io.MouseDelta.y));
                }
            }

            // Left-click 3D mesh picking (Gestaltor native GPU color picking)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
                float localX = io.MousePos.x - imagePos.x;
                float localY = io.MousePos.y - imagePos.y;
                int px = static_cast<int>(localX);
                int py = vpH - static_cast<int>(localY) - 1;

                if (px < 0) px = 0;
                if (px >= vpW) px = vpW - 1;
                if (py < 0) py = 0;
                if (py >= vpH) py = vpH - 1;

                if (pickingFbo.isValid()) {
                    uint8_t pixel[4] = { 0, 0, 0, 0 };
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, pickingFbo.getFbo());
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
                                s_scrollToSelectedNode = true;
                                isSceneDirty = true;
                                logMessage("Picked: " + (pickedNode->hasName() ? pickedNode->getName() : ("Node " + std::to_string(pickedNode->m_ID))));
                                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                    focusCameraOnNode(pickedNode, gltfModel, gltfLayer.get(), cameraActor, antiAliasing, isSceneDirty);
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

        // 3D Coordinate Gizmo (non-transforming visual indicator for selected node)
        if (viewportSettings.showNodeCoordinateGizmo && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr && sceneFbo.isValid()) {
            auto* node = selection.selectedNode;
            glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawNodeCoordinateGizmo(drawList, nodeWorld, viewProj, imagePos, imageSize, cameraActor.getEyePosition(), viewportSettings.gizmoAxisConvention);
        }

            ImGui::End(); // End 3D Viewport
        }

        // -------------------------------------------------------------
        // Hierarchy Panel
        // -------------------------------------------------------------
        if (showHierarchy) {
            if (ImGui::Begin(WND_HIERARCHY, &showHierarchy)) {
                if (activeScene != nullptr) {
                    std::string sceneTitle = activeScene->hasName() ? activeScene->getName().c_str() : ("Scene " + std::to_string(activeScene->m_ID));
                    if (ImGui::TreeNodeEx(sceneTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (auto* rootNode : activeScene->getNodes()) {
                            renderNodeHierarchy(rootNode, selection, gltfModel, gltfLayer.get(), cameraActor, antiAliasing, isSceneDirty);
                        }
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
                    if (ImGui::Button("Open File...")) {
                        auto openRes = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                        if (!openRes.empty()) {
                            loadGltfModel(openRes[0], gltfModel, activeScene, selection, currentFilePath, cameraActor, gltfLayer.get(), animationPlayer, isSceneDirty);
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
            if (ImGui::Begin(WND_RESOURCES, &showResources)) {
                ux3d::imgui::ResourcesPanel::render(gltfModel, selection, gltfLayer.get(), &isSceneDirty);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Inspector Panel
        // -------------------------------------------------------------
        if (showInspector) {
            if (ImGui::Begin(WND_INSPECTOR, &showInspector)) {
                ux3d::imgui::ElementInspector::render(selection, gltfModel, isSceneDirty, &animationPlayer);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Viewport Settings Panel
        // -------------------------------------------------------------
        if (showViewportSettings) {
            if (ImGui::Begin(WND_VIEWPORT_SETTINGS, &showViewportSettings)) {
                ux3d::imgui::ViewportSettingsPanel::render(viewportSettings, commandGenerator.get(), gltfLayer.get(), cameraActor, isSceneDirty, &gltfModel);
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
        // Logger Panel
        // -------------------------------------------------------------
        if (showLogger) {
            if (ImGui::Begin(WND_LOGGER, &showLogger)) {
                if (ImGui::Button(ICON_MDI_DELETE_OUTLINE " Clear")) {
                    s_consoleLogs.clear();
                }
                ImGui::SameLine();
                ImGui::PushFont(ux3d::imgui::getMonoFont());
                ImGui::Text("Messages: %zu", s_consoleLogs.size());
                ImGui::PopFont();
                ImGui::Separator();
                ImGui::BeginChild("LoggerScrollArea");
                ImGui::PushFont(ux3d::imgui::getMonoFont());
                for (const auto& entry : s_consoleLogs) {
                    ImGui::TextColored(entry.color, "%s", entry.text.c_str());
                }
                ImGui::PopFont();
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
    gridRenderer.cleanup();
    sceneFbo.unbind();

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
