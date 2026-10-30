// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/ObjectActions.h"
#include "imgui/Theme.h"
#include "imgui/UiProbe.h"
#include <portable-file-dialogs.h>
#include <cstdlib>

namespace ux3d::imgui::application {

namespace {

std::vector<std::string> chooseOpenModel(const char* title, bool scripted)
{
    // Exercise the real menu dispatch headlessly; ordinary sessions always use
    // the native dialog, even if this environment variable happens to be set.
    if (scripted) {
        if (const char* path = std::getenv("GLIDER_TEST_OPEN_FILE"))
            return *path ? std::vector<std::string>{path} : std::vector<std::string>{};
    }
    return pfd::open_file(title, "", {"glTF files (*.gltf *.glb)", "*.gltf *.glb"}).result();
}

struct ShadingModeInfo {
    const char* label;
    ux3d::renderer::RenderMode mode;
    const char* shortcut;
    bool isSeparator;
};

constexpr ShadingModeInfo s_shadingModes[] = {
    {"Shaded + post effects", ux3d::renderer::RenderMode::None, "Ctrl+Alt+1", false},
    {"Wireframe", ux3d::renderer::RenderMode::Wireframe, "Ctrl+Alt+2", false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Texture Coordinates 0", ux3d::renderer::RenderMode::TexCoord_0, nullptr, false},
    {"Texture Coordinates 1", ux3d::renderer::RenderMode::TexCoord_1, nullptr, false},
    {"Normal Texture", ux3d::renderer::RenderMode::NormalTexture, nullptr, false},
    {"Geometry Normal", ux3d::renderer::RenderMode::NormalGeometry, nullptr, false},
    {"Geometry Tangent", ux3d::renderer::RenderMode::Tangent, nullptr, false},
    {"Geometry Bitangent", ux3d::renderer::RenderMode::Bitangent, nullptr, false},
    {"Decal Prepass Depth", ux3d::renderer::RenderMode::DecalPrepassDepth, nullptr, false},
    {"Decal Prepass Normal", ux3d::renderer::RenderMode::DecalPrepassNormal, nullptr, false},
    {"SSR", ux3d::renderer::RenderMode::SSR, nullptr, false},
    {"SSR Ray", ux3d::renderer::RenderMode::SSRRay, nullptr, false},
    {"Shadow Cascades", ux3d::renderer::RenderMode::ShadowCascades, nullptr, false},
    {"Shadow Visibility", ux3d::renderer::RenderMode::ShadowVisibility, nullptr, false},
    {"SSAO", ux3d::renderer::RenderMode::SSAO, nullptr, false},
    {"Sky Visibility", ux3d::renderer::RenderMode::SkyVisibility, nullptr, false},
    {"Shading Normal", ux3d::renderer::RenderMode::ShadingNormal, nullptr, false},
    {"Alpha", ux3d::renderer::RenderMode::Alpha, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Occlusion", ux3d::renderer::RenderMode::Occlusion, nullptr, false},
    {"Emissive", ux3d::renderer::RenderMode::Emissive, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Metallic Roughness", ux3d::renderer::RenderMode::MetallicRoughness, nullptr, false},
    {"    BaseColor", ux3d::renderer::RenderMode::BaseColor, nullptr, false},
    {"    Roughness", ux3d::renderer::RenderMode::Roughness, nullptr, false},
    {"    Metallic", ux3d::renderer::RenderMode::Metallic, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Clearcoat", ux3d::renderer::RenderMode::Clearcoat, nullptr, false},
    {"    Clearcoat Factor", ux3d::renderer::RenderMode::ClearcoatFactor, nullptr, false},
    {"    Clearcoat Roughness", ux3d::renderer::RenderMode::ClearcoatRoughness, nullptr, false},
    {"    Clearcoat Normal", ux3d::renderer::RenderMode::ClearcoatNormal, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Sheen", ux3d::renderer::RenderMode::Sheen, nullptr, false},
    {"    Sheen Color", ux3d::renderer::RenderMode::SheenColor, nullptr, false},
    {"    Sheen Roughness", ux3d::renderer::RenderMode::SheenRoughness, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Specular", ux3d::renderer::RenderMode::Specular, nullptr, false},
    {"    Specular Factor", ux3d::renderer::RenderMode::SpecularFactor, nullptr, false},
    {"    Specular Color", ux3d::renderer::RenderMode::SpecularColor, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Transmission/Volume", ux3d::renderer::RenderMode::TransmissionVolume, nullptr, false},
    {"    Transmission Factor", ux3d::renderer::RenderMode::TransmissionFactor, nullptr, false},
    {"    Volume Thickness", ux3d::renderer::RenderMode::VolumeThickness, nullptr, false},
    {"", ux3d::renderer::RenderMode::None, nullptr, true},
    {"Iridescence", ux3d::renderer::RenderMode::Iridescence, nullptr, false},
    {"    Iridescence Factor", ux3d::renderer::RenderMode::IridescenceFactor, nullptr, false},
    {"    Iridescence Thickness", ux3d::renderer::RenderMode::IridescenceThickness, nullptr, false},
};

} // namespace

ux3d::renderer::RenderMode Application::getRenderModeFromIndex(int idx)
{
    switch (idx) {
    case 1:
        return ux3d::renderer::RenderMode::Wireframe;
    case 2:
        return ux3d::renderer::RenderMode::NormalTexture;
    case 3:
        return ux3d::renderer::RenderMode::ShadingNormal;
    case 4:
        return ux3d::renderer::RenderMode::NormalGeometry;
    case 5:
        return ux3d::renderer::RenderMode::Tangent;
    case 6:
        return ux3d::renderer::RenderMode::Bitangent;
    case 7:
        return ux3d::renderer::RenderMode::BaseColor;
    case 8:
        return ux3d::renderer::RenderMode::Metallic;
    case 9:
        return ux3d::renderer::RenderMode::Roughness;
    case 10:
        return ux3d::renderer::RenderMode::MetallicRoughness;
    case 11:
        return ux3d::renderer::RenderMode::Occlusion;
    case 12:
        return ux3d::renderer::RenderMode::Emissive;
    case 13:
        return ux3d::renderer::RenderMode::Alpha;
    case 14:
        return ux3d::renderer::RenderMode::TexCoord_0;
    case 15:
        return ux3d::renderer::RenderMode::TexCoord_1;
    case 16:
        return ux3d::renderer::RenderMode::Clearcoat;
    case 17:
        return ux3d::renderer::RenderMode::Sheen;
    case 18:
        return ux3d::renderer::RenderMode::Specular;
    case 19:
        return ux3d::renderer::RenderMode::TransmissionVolume;
    case 20:
        return ux3d::renderer::RenderMode::Iridescence;
    case 21:
        return ux3d::renderer::RenderMode::ClearcoatFactor;
    case 22:
        return ux3d::renderer::RenderMode::ClearcoatRoughness;
    case 23:
        return ux3d::renderer::RenderMode::ClearcoatNormal;
    case 24:
        return ux3d::renderer::RenderMode::SheenColor;
    case 25:
        return ux3d::renderer::RenderMode::SheenRoughness;
    case 26:
        return ux3d::renderer::RenderMode::SpecularFactor;
    case 27:
        return ux3d::renderer::RenderMode::SpecularColor;
    case 28:
        return ux3d::renderer::RenderMode::TransmissionFactor;
    case 29:
        return ux3d::renderer::RenderMode::VolumeThickness;
    case 30:
        return ux3d::renderer::RenderMode::IridescenceFactor;
    case 31:
        return ux3d::renderer::RenderMode::IridescenceThickness;
    case 32:
        return ux3d::renderer::RenderMode::SSR;
    case 33:
        return ux3d::renderer::RenderMode::SSRRay;
    case 34:
        return ux3d::renderer::RenderMode::ShadowCascades;
    case 35:
        return ux3d::renderer::RenderMode::ShadowVisibility;
    case 36:
        return ux3d::renderer::RenderMode::SSAO;
    case 37:
        return ux3d::renderer::RenderMode::SkyVisibility;
    default:
        return ux3d::renderer::RenderMode::None;
    }
}

std::optional<ux3d::renderer::RenderMode> Application::getRenderModeFromName(const std::string& name)
{
    auto normalize = [](std::string text) {
        std::string result;
        for (char c : toLowerAscii(text)) {
            if (c != ' ' && c != '_' && c != '-') {
                result.push_back(c);
            }
        }
        return result;
    };
    const std::string wanted = normalize(name);
    if (wanted.empty()) {
        return std::nullopt;
    }
    if (wanted == "default" || wanted == "shaded" || wanted == "none" || wanted == "shadedpost" || wanted == "shaded+posteffects") {
        return ux3d::renderer::RenderMode::None;
    }
    for (const ShadingModeInfo& info : s_shadingModes) {
        if (info.isSeparator) {
            continue;
        }
        std::string label = info.label;
        while (!label.empty() && label.front() == ' ') {
            label.erase(label.begin());
        }
        if (normalize(label) == wanted) {
            return info.mode;
        }
    }
    return std::nullopt;
}

std::vector<std::string> Application::renderModeNames()
{
    // The agent registry's render_mode values: the View menu labels, lower case without
    // spaces, which getRenderModeFromName accepts.
    std::vector<std::string> names{"shaded"};
    for (const ShadingModeInfo& info : s_shadingModes) {
        if (info.isSeparator || info.mode == ux3d::renderer::RenderMode::None) {
            continue;
        }
        std::string name;
        for (char c : toLowerAscii(info.label)) {
            if (c != ' ' && c != '_' && c != '-') {
                name.push_back(c);
            }
        }
        names.push_back(name);
    }
    return names;
}

std::string Application::renderModeName(ux3d::renderer::RenderMode mode)
{
    if (mode == ux3d::renderer::RenderMode::None) {
        return "shaded";
    }
    for (const std::string& name : renderModeNames()) {
        if (getRenderModeFromName(name) == mode) {
            return name;
        }
    }
    return "shaded";
}

int Application::getIndexFromRenderMode(ux3d::renderer::RenderMode mode)
{
    using RM = ux3d::renderer::RenderMode;
    switch (mode) {
    case RM::Wireframe:
        return 1;
    case RM::NormalTexture:
        return 2;
    case RM::ShadingNormal:
        return 3;
    case RM::NormalGeometry:
        return 4;
    case RM::Tangent:
        return 5;
    case RM::Bitangent:
        return 6;
    case RM::BaseColor:
        return 7;
    case RM::Metallic:
        return 8;
    case RM::Roughness:
        return 9;
    case RM::MetallicRoughness:
        return 10;
    case RM::Occlusion:
        return 11;
    case RM::Emissive:
        return 12;
    case RM::Alpha:
        return 13;
    case RM::TexCoord_0:
        return 14;
    case RM::TexCoord_1:
        return 15;
    case RM::Clearcoat:
        return 16;
    case RM::Sheen:
        return 17;
    case RM::Specular:
        return 18;
    case RM::TransmissionVolume:
        return 19;
    case RM::Iridescence:
        return 20;
    case RM::ClearcoatFactor:
        return 21;
    case RM::ClearcoatRoughness:
        return 22;
    case RM::ClearcoatNormal:
        return 23;
    case RM::SheenColor:
        return 24;
    case RM::SheenRoughness:
        return 25;
    case RM::SpecularFactor:
        return 26;
    case RM::SpecularColor:
        return 27;
    case RM::TransmissionFactor:
        return 28;
    case RM::VolumeThickness:
        return 29;
    case RM::IridescenceFactor:
        return 30;
    case RM::IridescenceThickness:
        return 31;
    case RM::SSR:
        return 32;
    case RM::SSRRay:
        return 33;
    case RM::ShadowCascades:
        return 34;
    case RM::ShadowVisibility:
        return 35;
    case RM::SSAO:
        return 36;
    case RM::SkyVisibility:
        return 37;
    default:
        return 0;
    }
}

const char* Application::getRenderModeLabel(ux3d::renderer::RenderMode mode)
{
    using RM = ux3d::renderer::RenderMode;
    switch (mode) {
    case RM::Wireframe:
        return "Wireframe";
    case RM::TexCoord_0:
        return "TexCoord 0";
    case RM::TexCoord_1:
        return "TexCoord 1";
    case RM::NormalTexture:
        return "Normal Texture";
    case RM::ShadingNormal:
        return "Shading Normal";
    case RM::NormalGeometry:
        return "Geometry Normal";
    case RM::Tangent:
        return "Geometry Tangent";
    case RM::Bitangent:
        return "Geometry Bitangent";
    case RM::DecalPrepassDepth:
        return "Decal Prepass Depth";
    case RM::DecalPrepassNormal:
        return "Decal Prepass Normal";
    case RM::SSR:
        return "SSR";
    case RM::SSRRay:
        return "SSR Ray";
    case RM::ShadowCascades:
        return "Shadow Cascades";
    case RM::ShadowVisibility:
        return "Shadow Visibility";
    case RM::SSAO:
        return "SSAO";
    case RM::SkyVisibility:
        return "Sky Visibility";
    case RM::Alpha:
        return "Alpha";
    case RM::Occlusion:
        return "Occlusion";
    case RM::Emissive:
        return "Emissive";
    case RM::MetallicRoughness:
        return "Metallic Roughness";
    case RM::Roughness:
        return "Roughness";
    case RM::Metallic:
        return "Metallic";
    case RM::BaseColor:
        return "BaseColor";
    case RM::Clearcoat:
        return "Clearcoat";
    case RM::ClearcoatFactor:
        return "Clearcoat Factor";
    case RM::ClearcoatRoughness:
        return "Clearcoat Roughness";
    case RM::ClearcoatNormal:
        return "Clearcoat Normal";
    case RM::Sheen:
        return "Sheen";
    case RM::SheenColor:
        return "Sheen Color";
    case RM::SheenRoughness:
        return "Sheen Roughness";
    case RM::Specular:
        return "Specular";
    case RM::SpecularFactor:
        return "Specular Factor";
    case RM::SpecularColor:
        return "Specular Color";
    case RM::TransmissionVolume:
        return "Transmission/Volume";
    case RM::TransmissionFactor:
        return "Transmission Factor";
    case RM::VolumeThickness:
        return "Volume Thickness";
    case RM::Iridescence:
        return "Iridescence";
    case RM::IridescenceFactor:
        return "Iridescence Factor";
    case RM::IridescenceThickness:
        return "Iridescence Thickness";
    default:
        return "Default Shaded";
    }
}

void Application::setupDefaultDockLayout(ImGuiID dockspaceId, ImVec2 size, bool force)
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
        ImGuiID dock_center_bottom =
            ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);

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

void Application::createViewport(const ux3d::gltf::Node* cameraNode)
{
    uint32_t nextId = 1;
    for (const auto& v : viewports) {
        if (v->id >= nextId) nextId = v->id + 1;
    }
    const std::string camName =
        cameraNode != nullptr
            ? (cameraNode->hasName() ? cameraNode->getName() : ("Camera Node " + std::to_string(cameraNode->m_ID)))
            : std::string();
    std::string vpName = cameraNode != nullptr ? ("Camera: " + camName) : ("Viewport " + std::to_string(nextId));
    auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
    newVp->isAttachedToCamera = cameraNode != nullptr;
    newVp->attachedCameraNode = cameraNode;
    newVp->attachedCameraNodeId = cameraNode != nullptr ? cameraNode->m_ID : 0;
    newVp->attachedCameraNodeName = camName;
    if (activeViewport != nullptr) {
        newVp->settings = activeViewport->settings;
        // AA is renderer-wide (settings scopes); the new viewport inherits the active one's view settings and overrides.
        if (cameraNode == nullptr && activeScene != nullptr) {
            newVp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
        }
    }
    newVp->requestFocus = true;
    activeViewport = newVp.get();
    selection.selectViewport(newVp->id);
    viewports.push_back(std::move(newVp));
}

void Application::drawViewportMenuBody()
{
    if (ImGui::MenuItem(ICON_MDI_PLUS " New Viewport")) {
        pendingNewViewport = true;
    }

    if (ImGui::BeginMenu(ICON_MDI_CAMERA_PLUS_OUTLINE " New Viewport From")) {
        auto cameraNodes = collectSceneCameraNodes(activeScene);
        if (cameraNodes.empty()) {
            ImGui::MenuItem("(No Cameras in Scene)", nullptr, false, false);
        } else {
            for (const auto* camNode : cameraNodes) {
                std::string camName =
                    camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                std::string itemTitle = ICON_MDI_CAMERA " " + camName;
                if (ImGui::MenuItem(itemTitle.c_str())) {
                    pendingNewViewportCamera = camNode;
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

    bool showBBox = hasActiveVp &&
                    (activeViewport->settings.showSceneBoundingBox || activeViewport->settings.showSelectedBoundingBox);
    if (ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Toggle Show Bounding Box", nullptr, showBBox, hasActiveVp)) {
        bool next = !showBBox;
        activeViewport->settings.showSceneBoundingBox = next;
        activeViewport->settings.showSelectedBoundingBox = next;
    }

    bool isPersp =
        hasActiveVp && (activeViewport->rendererCamera.getType() == ux3d::renderer::Camera::Type::Perspective);
    if (ImGui::MenuItem(ICON_MDI_CAMERA " Toggle Persp / Orthographic", "Num 5", isPersp,
                        hasActiveVp && !activeViewport->isAttachedToCamera)) {
        activeViewport->cameraActor.changeUserCameraType(isPersp ? ux3d::renderer::Camera::Type::Orthographic
                                                                 : ux3d::renderer::Camera::Type::Perspective);
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
        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
    }

    if (ImGui::BeginMenu(ICON_MDI_COMPASS_OUTLINE " Orientation", hasActiveVp && !isAttached)) {
        if (ImGui::MenuItem("Front", "Num 1")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Front);
            activeViewport->antiAliasing.resetHistory();
        }
        if (ImGui::MenuItem("Back", "Ctrl+Num 1")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Back);
            activeViewport->antiAliasing.resetHistory();
        }
        if (ImGui::MenuItem("Right", "Num 3")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Right);
            activeViewport->antiAliasing.resetHistory();
        }
        if (ImGui::MenuItem("Left", "Ctrl+Num 3")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Left);
            activeViewport->antiAliasing.resetHistory();
        }
        if (ImGui::MenuItem("Top", "Num 7")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Top);
            activeViewport->antiAliasing.resetHistory();
        }
        if (ImGui::MenuItem("Bottom", "Ctrl+Num 7")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Bottom);
            activeViewport->antiAliasing.resetHistory();
        }
        if (ImGui::MenuItem("Isometric")) {
            activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Isometric);
            activeViewport->antiAliasing.resetHistory();
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem(ICON_MDI_HOME_OUTLINE " Reset Camera", "Home", false, hasActiveVp)) {
        if (activeScene != nullptr) activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
        activeViewport->antiAliasing.resetHistory();
    }

    if (ImGui::BeginMenu(ICON_MDI_SHIELD_OUTLINE " Shading", hasActiveVp)) {
        drawShadingMenu(*activeViewport);
        ImGui::EndMenu();
    }

    ImGui::Separator();
    drawViewportImageMenuItems();
}

void Application::drawShadingMenu(ux3d::imgui::ViewportInstance& viewport)
{
    if (ImGui::MenuItem("Shaded", nullptr, !viewport.settings.postEffects && viewport.settings.renderMode == ux3d::renderer::RenderMode::None)) {
        viewport.settings.renderMode = ux3d::renderer::RenderMode::None;
        viewport.settings.renderModeIdx = 38;
        viewport.settings.postEffects = false;
        viewport.antiAliasing.resetHistory();
        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
    }
    const bool showDecalPrepassViews = decalBuffer.isValid() && viewportNeedsDecalBuffer(viewport);
    for (const auto& item : s_shadingModes) {
        // A2.1: the decal prepass modes are shown only while the buffer is active.
        if ((item.mode == ux3d::renderer::RenderMode::DecalPrepassDepth ||
             item.mode == ux3d::renderer::RenderMode::DecalPrepassNormal) &&
            !showDecalPrepassViews) {
            continue;
        }
        if (item.isSeparator) {
            ImGui::Separator();
        } else {
            bool isSelected = (viewport.settings.renderMode == item.mode) && (item.mode != ux3d::renderer::RenderMode::None || viewport.settings.postEffects);
            if (ImGui::MenuItem(item.label, item.shortcut, isSelected)) {
                viewport.settings.postEffects = true;
                viewport.settings.renderMode = item.mode;
                viewport.settings.renderModeIdx = getIndexFromRenderMode(item.mode);
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            }
        }
    }
}

void Application::showViewportMenuFor(ux3d::imgui::ViewportInstance* vp)
{
    activeViewport = vp;
    drawViewportMenuBody();
    if (pendingNewViewport || pendingNewViewportCamera != nullptr) {
        createViewport(pendingNewViewportCamera);
        pendingNewViewport = false;
        pendingNewViewportCamera = nullptr;
    }
}

void Application::drawMenus(FrameContext& frame)
{
    auto& io = ImGui::GetIO();
    const auto uiChromeStart = ux3d::imgui::profiling::Clock::now();
    pendingNewViewport = false;
    pendingNewViewportCamera = nullptr;
    // Main DockSpace covering the whole window (minus status bar at bottom)
    float statusBarHeight = ImGui::GetFrameHeight() + 8.0f;
    ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImVec2 dockspaceSize = ImVec2(mainViewport->WorkSize.x, mainViewport->WorkSize.y - statusBarHeight);
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(dockspaceSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);
    windowFlags |=
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("GliderDockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("GliderMainDockSpace_v1");

    if (s_firstLayout || s_resetLayout) {
        const bool reset = s_resetLayout;
        bool force = s_resetLayout || s_isolatedState;
        s_firstLayout = false;
        s_resetLayout = false;
        setupDefaultDockLayout(dockspaceId, dockspaceSize, force);
        if (reset) {
            resetImguiLayoutPersistence();
        }
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

    // Global Keyboard Shortcuts (File & View)
    if (!io.WantTextInput && io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_O)) {
            auto openResult =
                chooseOpenModel("Open glTF / GLB", uiScenario.active());
            if (!openResult.empty()) {
                loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, viewports,
                              gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
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
            for (auto& vp : viewports)
                vp->isOpen = true;
            s_resetLayout = true;
        }
    }

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open ...", "Ctrl+O")) {
                auto openResult =
                    chooseOpenModel("Open glTF / GLB", uiScenario.active());
                if (!openResult.empty()) {
                    loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, viewports,
                                  gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
                    updateWindowTitle(window, currentFilePath);
                }
            }
            UiProbe::item("file.open",0);
            if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open without texture ...")) {
                const auto paths = chooseOpenModel("Open glTF / GLB without textures", uiScenario.active());
                if (!paths.empty()) {
                    loadGltfModel(paths[0], gltfModel, activeScene, selection, currentFilePath, viewports,
                        gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments, false, true);
                }
            }
            UiProbe::item("file.open_without_textures",0);
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
                        loadGltfModel(fileToOpen, gltfModel, activeScene, selection, currentFilePath, viewports,
                                      gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
                        updateWindowTitle(window, currentFilePath);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(ICON_MDI_CLOSE " Close", nullptr, false, modelLoaded)) {
                // Reset to the initial default scene (the embedded sample model).
                const std::string defaultModelPath = resolveSampleModelPath();
                loadGltfModel(defaultModelPath, gltfModel, activeScene, selection, currentFilePath, viewports,
                              gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
                updateWindowTitle(window, currentFilePath);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MDI_COG " Settings...", nullptr, &showSearchPaths)) {
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MDI_EXIT_TO_APP " Exit")) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        // Viewport Menu (Replaces previous View menu)
        const bool viewportMenuOpen = ImGui::BeginMenu("Viewport");
        UiProbe::item("menu.viewport", 0, viewportMenuOpen);
        if (viewportMenuOpen) {
            drawViewportMenuBody();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Object")) {
            ux3d::imgui::ObjectActionContext objectContext{gltfModel,
                                                           activeScene,
                                                           selection,
                                                           gltfLayer.get(),
                                                           activeViewport ? &activeViewport->cameraActor : nullptr,
                                                           activeViewport ? &activeViewport->antiAliasing : nullptr,
                                                           sceneRevisions};
            ux3d::imgui::drawObjectMenuItems(objectContext, ux3d::imgui::selectedNode(objectContext), false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Animation")) {
            const auto& animationElements = gltfModel.getElements<ux3d::gltf::Animation>();
            const bool hasAnimations = !animationElements.empty() && animationPlayer.getMaxDuration() > 0.0f;
            const bool playing = animationPlayer.isPlaying();
            if (ImGui::MenuItem(playing ? ICON_MDI_STOP " Stop" : ICON_MDI_PLAY " Play", "Alt+A", false,
                                hasAnimations)) {
                animationPlayer.setPlaying(!playing);
            }
            if (ImGui::MenuItem(ICON_MDI_SKIP_BACKWARD " Rewind", "Shift+Left", false, hasAnimations)) {
                animationPlayer.setCurrentTime(0.0f, gltfLayer.get());
                if (activeViewport != nullptr) activeViewport->antiAliasing.resetHistory();
            }
            if (ImGui::MenuItem(ICON_MDI_SKIP_FORWARD " FForward", "Shift+Right", false, hasAnimations)) {
                animationPlayer.setCurrentTime(animationPlayer.getMaxDuration(), gltfLayer.get());
                if (activeViewport != nullptr) activeViewport->antiAliasing.resetHistory();
            }
            if (ImGui::BeginMenu(ICON_MDI_PLAY_BOX_OUTLINE " Active", hasAnimations)) {
                animationPlayer.renderActiveAnimationsContent(gltfModel, gltfLayer.get(), sceneRevisions);
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
                for (auto& vp : viewports)
                    vp->isOpen = true;
                s_resetLayout = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End(); // End DockSpace

    frame.markStage("ui.chrome", uiChromeStart);
}

} // namespace ux3d::imgui::application
