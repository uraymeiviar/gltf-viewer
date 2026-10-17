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
#include <memory>
#include <string>
#include <vector>
#include <utility>

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

// Recursive scene hierarchy node renderer with double-click to frame
static void renderNodeHierarchy(ux3d::gltf::Node* node,
                                ux3d::imgui::SelectionState& selection,
                                ux3d::gltf::Gltf& gltfModel,
                                ux3d::imgui::OrbitCamera& camera,
                                ux3d::imgui::AntiAliasing& antiAliasing,
                                bool& sceneDirty)
{
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode == node) {
        flags |= ImGuiTreeNodeFlags_Selected;
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
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        selection.selectNode(node);
        ux3d::gltf::Extends ext = ux3d::gltf::utils::getExtends({ node }, &gltfModel);
        camera.fitToBounds(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                           glm::vec3(ext.max.x, ext.max.y, ext.max.z));
        antiAliasing.resetHistory();
    }

    if (open && !node->getChildren().empty()) {
        for (auto* child : node->getChildren()) {
            renderNodeHierarchy(child, selection, gltfModel, camera, antiAliasing, sceneDirty);
        }
        ImGui::TreePop();
    }
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

        animPlayer.setModel(gltfModel);

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
        return true;
    } else {
        logMessage("Failed to load: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }
}

// glTF saver helper
static bool saveGltfModel(const std::string& filePath,
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
        return true;
    } else {
        logMessage("Failed to save: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }
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
        ImGui::DockBuilderDockWindow("Console", dock_bottom);
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

    std::string defaultSample = resolveSampleModelPath();
    if (ux3d::imgui::StdFileSystem::instance().isFile(ux3d::imgui::StdFileSystem::instance().toFile(defaultSample.c_str()))) {
        loadGltfModel(defaultSample, gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
    }

    // UI state
    bool showDemoWindow = false;
    bool showAboutPopup = false;
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

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                    if (!openResult.empty()) {
                        loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                    }
                }
                if (ImGui::MenuItem("Open Sample (HelloCube.glb)")) {
                    loadGltfModel("resources/sampleGltf/HelloCube.glb", gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S", false, !currentFilePath.empty())) {
                    saveGltfModel(currentFilePath, gltfModel, currentFilePath);
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, activeScene != nullptr)) {
                    auto dest = pfd::save_file("Save glTF / GLB", "", { "glTF files (*.gltf)", "*.gltf", "Binary glTF (*.glb)", "*.glb" }).result();
                    if (!dest.empty()) {
                        saveGltfModel(dest, gltfModel, currentFilePath);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
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
                ImGui::MenuItem("Show Grid", nullptr, &viewportSettings.showGrid);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Default Layout")) {
                    setupDefaultDockLayout(dockspaceId, mainViewport->WorkSize, true);
                }
                ImGui::Separator();
                ImGui::MenuItem("ImGui Demo Window", nullptr, &showDemoWindow);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About Gestaltor")) {
                    showAboutPopup = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End(); // End DockSpace

        // -------------------------------------------------------------
        // 3D Viewport Panel
        // -------------------------------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("3D Viewport");
        ImGui::PopStyleVar();

        // Animation Player Toolbar at top of viewport
        animationPlayer.renderToolbar(gltfModel);

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
            if (activeScene != nullptr && isSceneDirty) {
                gltfLayer->updateScene(gltfModel, *activeScene);
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
        if (viewportSettings.showSelectedBoundingBox && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
            ux3d::gltf::Extends ext = ux3d::gltf::utils::getExtends({ selection.selectedNode }, &gltfModel);
            bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                              glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                              glm::vec4(0.2f, 0.85f, 1.0f, 0.9f),
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

        // Handle Mouse Camera Controls when hovering Viewport (Default Blender style)
        bool isViewportHovered = ImGui::IsItemHovered();
        if (isViewportHovered && !ImGuizmo::IsUsing()) {
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
        }

        // ImGuizmo 3D Transform Gizmo
        if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr && sceneFbo.isValid()) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

            auto* node = selection.selectedNode;
            glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
            float matrix[16];
            std::memcpy(matrix, glm::value_ptr(nodeWorld), sizeof(matrix));

            if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), currentGizmoOp, currentGizmoMode, matrix)) {
                glm::mat4 manipulatedWorld = glm::make_mat4(matrix);
                glm::mat4 parentWorld = gltfLayer ? glm::mat4(gltfLayer->getParentWorldTransform(node)) : glm::mat4(1.0f);
                glm::mat4 newLocal = glm::inverse(parentWorld) * manipulatedWorld;

                node->decomposeTransformMatrix(ux3d::math::dmat4x4(newLocal));
                isSceneDirty = true;
            }
        }

        ImGui::End(); // End 3D Viewport

        // -------------------------------------------------------------
        // Hierarchy Panel
        // -------------------------------------------------------------
        ImGui::Begin("Scene Hierarchy");
        if (activeScene != nullptr) {
            std::string sceneTitle = activeScene->hasName() ? activeScene->getName().c_str() : ("Scene " + std::to_string(activeScene->m_ID));
            if (ImGui::TreeNodeEx(sceneTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto* rootNode : activeScene->getNodes()) {
                    renderNodeHierarchy(rootNode, selection, gltfModel, orbitCamera, antiAliasing, isSceneDirty);
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
            if (ImGui::Button("Open File...")) {
                auto openRes = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                if (!openRes.empty()) {
                    loadGltfModel(openRes[0], gltfModel, activeScene, selection, currentFilePath, orbitCamera, gltfLayer.get(), animationPlayer, isSceneDirty);
                }
            }
        }
        ImGui::End();

        // -------------------------------------------------------------
        // Resources Panel
        // -------------------------------------------------------------
        ImGui::Begin("Resources");
        ux3d::imgui::ResourcesPanel::render(gltfModel, selection, gltfLayer.get(), &isSceneDirty);
        ImGui::End();

        // -------------------------------------------------------------
        // Inspector Panel
        // -------------------------------------------------------------
        ImGui::Begin("Inspector");
        ux3d::imgui::ElementInspector::render(selection, gltfModel, isSceneDirty, &animationPlayer);
        ImGui::End();

        // -------------------------------------------------------------
        // Viewport Settings Panel
        // -------------------------------------------------------------
        ImGui::Begin("Viewport Settings");
        ux3d::imgui::ViewportSettingsPanel::render(viewportSettings, commandGenerator.get(), gltfLayer.get(), orbitCamera, isSceneDirty, &gltfModel);
        ImGui::End();

        // -------------------------------------------------------------
        // Statistics Panel
        // -------------------------------------------------------------
        ImGui::Begin("Statistics");
        ux3d::imgui::StatisticsPanel::render(gltfModel, currentFilePath);
        ImGui::End();

        // -------------------------------------------------------------
        // Console Panel
        // -------------------------------------------------------------
        ImGui::Begin("Console");
        if (ImGui::Button("Clear")) {
            s_consoleLogs.clear();
        }
        ImGui::SameLine();
        ImGui::Text("Messages: %zu", s_consoleLogs.size());
        ImGui::Separator();
        ImGui::BeginChild("ConsoleScrollArea");
        for (const auto& entry : s_consoleLogs) {
            ImGui::TextColored(entry.color, "%s", entry.text.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();

        // Demo Window (for debug/testing)
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // About Popup
        if (showAboutPopup) {
            ImGui::OpenPopup("About Gestaltor");
            showAboutPopup = false;
        }
        if (ImGui::BeginPopupModal("About Gestaltor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Gestaltor - glTF 2.0 Visual Editor");
            ImGui::Text("Migrated to Dear ImGui + GLFW + GLAD");
            ImGui::Text("Standalone executable with zero runtime dependencies!");
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
