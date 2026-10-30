// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Element Inspector: ElementInspector::render (node, mesh, primitive, camera, light, texture, image, sampler,
// accessor, animation and skin views). Image previews and header details are in ElementInspectorImages.cpp.

#include "ElementInspectorImages.h"

namespace ux3d::imgui {

static bool renderBackToMaterialButton(SelectionState& selection, const char* probeTag)
{
    if (selection.returnMaterial == nullptr) {
        return false;
    }
    ux3d::gltf::Material* material = selection.returnMaterial;
    const std::string materialName = material->hasName()
        ? material->getName().c_str()
        : ("Material " + std::to_string(material->m_ID));
    if (ImGui::Button(ICON_MDI_ARROW_LEFT " Back to Material")) {
        selection.selectMaterial(material);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("Return to %s", materialName.c_str());
    }
    UiProbe::item(probeTag, 0);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", materialName.c_str());
    ImGui::Separator();
    return true;
}

void ElementInspector::render(SelectionState& selection,
                              ux3d::gltf::Gltf& gltfModel,
                              SceneRevisions& revisions,
                              AnimationPlayer* animationPlayer,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              std::vector<std::unique_ptr<ViewportInstance>>* viewports,
                              ux3d::renderer::CommandGenerator* commandGenerator)
{
    if (selection.type == SelectionType::None) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an element from the Hierarchy or Resources panel.");
        return;
    }

    ViewportInstance* activeViewport = nullptr;
    if (viewports != nullptr && !viewports->empty()) {
        if (viewports->size() == 1) {
            activeViewport = (*viewports)[0].get();
        } else {
            ImGuiContext* g = ImGui::GetCurrentContext();
            if (g != nullptr) {
                for (int i = g->WindowsFocusOrder.Size - 1; i >= 0; --i) {
                    ImGuiWindow* win = g->WindowsFocusOrder[i];
                    if (!win || !win->Name) continue;
                    for (auto& vp : *viewports) {
                        std::string idTag = "###Viewport_" + std::to_string(vp->id);
                        if (std::strstr(win->Name, idTag.c_str()) != nullptr) {
                            activeViewport = vp.get();
                            break;
                        }
                    }
                    if (activeViewport != nullptr) break;
                }
            }
            if (activeViewport == nullptr) {
                activeViewport = (*viewports)[0].get();
            }
        }
    }
    ExtensionInspector::instance().setActiveViewport(activeViewport);

    const bool packagedTextures = gltfModel.isExtensionDeclared("ASOBO_asset_optimized");
    const bool ddsDeclared = gltfModel.isExtensionDeclared("MSFT_texture_dds");

    // -----------------------------------------------------------------
    // Node Inspector
    // -----------------------------------------------------------------
    if (selection.type == SelectionType::Node && selection.selectedNode != nullptr) {
        auto* node = selection.selectedNode;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Node ID: %u", node->m_ID);

        char nameBuf[256] = {};
        if (node->hasName()) {
            std::strncpy(nameBuf, node->getName().c_str(), sizeof(nameBuf) - 1);
        }
        ImGui::InputText("Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Renaming is disabled in the viewer");
        }

        ImGui::Separator();

        // Transform (TRS). Local shows the node's own TRS; Global shows and edits the
        // world-space transform, converting the result back through the parent transform.
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTabBar("##TransformSpaceTabs")) {
                if (ImGui::BeginTabItem("Local")) {
                    ux3d::imgui::UiProbe::item("insp.transform.local", 0);
                    ImGui::PushFont(getMonoFont());
                    // Translation
                    auto t = node->getTranslationOrDefault();
                    float trans[3] = { static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z) };
                    if (ImGui::DragFloat3(ICON_MDI_AXIS_ARROW "##Translation", trans, 0.05f)) {
                        node->setTranslation(ux3d::math::dvec3(trans[0], trans[1], trans[2]));
                        revisions.mark(SceneChange::Transform);
                    }
                    ux3d::imgui::UiProbe::item("insp.transform.translation", 0);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Local Translation");

                    // Rotation (Euler angles in degrees)
                    auto r = node->getRotationOrDefault();
                    glm::quat q(static_cast<float>(r.w), static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z));
                    glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
                    float rot[3] = { euler.x, euler.y, euler.z };
                    if (ImGui::DragFloat3(ICON_MDI_ROTATE_3D_VARIANT "##Rotation", rot, 0.5f)) {
                        glm::quat newQ = glm::quat(glm::radians(glm::vec3(rot[0], rot[1], rot[2])));
                        node->setRotation(ux3d::math::dquat(newQ.w, newQ.x, newQ.y, newQ.z));
                        revisions.mark(SceneChange::Transform);
                    }
                    ux3d::imgui::UiProbe::item("insp.transform.rotation", 0);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Local Rotation (deg)");

                    // Scale
                    auto s = node->getScaleOrDefault();
                    float scale[3] = { static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z) };
                    if (ImGui::DragFloat3(ICON_MDI_ARROW_EXPAND_ALL "##Scale", scale, 0.02f, 0.001f, 100.0f)) {
                        node->setScale(ux3d::math::dvec3(scale[0], scale[1], scale[2]));
                        revisions.mark(SceneChange::Transform);
                    }
                    ux3d::imgui::UiProbe::item("insp.transform.scale", 0);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Local Scale");
                    ImGui::PopFont();

                    if (ImGui::Button("Reset Transform")) {
                        node->setTranslation(ux3d::math::dvec3(0.0));
                        node->setRotation(ux3d::math::dquat(1.0, 0.0, 0.0, 0.0));
                        node->setScale(ux3d::math::dvec3(1.0));
                        revisions.mark(SceneChange::Transform);
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Global")) {
                    ux3d::imgui::UiProbe::item("insp.transform.global", 0);
                    if (gltfLayer == nullptr) {
                        ImGui::TextDisabled("No renderer scene loaded.");
                    } else {
                        const glm::mat4 world = gltfLayer->getWorldTransform(node);
                        const glm::mat4 parentWorld = gltfLayer->getParentWorldTransform(node);
                        glm::vec3 worldTranslation = glm::vec3(world[3]);
                        glm::quat worldRotation = gltfLayer->getWorldRotationQuat(node);
                        glm::vec3 worldEuler = glm::degrees(glm::eulerAngles(worldRotation));
                        glm::vec3 worldScale(
                            glm::length(glm::vec3(world[0])),
                            glm::length(glm::vec3(world[1])),
                            glm::length(glm::vec3(world[2])));

                        bool changed = false;
                        ImGui::PushFont(getMonoFont());
                        float globalTranslation[3] = { worldTranslation.x, worldTranslation.y, worldTranslation.z };
                        if (ImGui::DragFloat3(ICON_MDI_AXIS_ARROW "##GlobalTranslation", globalTranslation, 0.05f)) {
                            worldTranslation = glm::vec3(globalTranslation[0], globalTranslation[1], globalTranslation[2]);
                            changed = true;
                        }
                        ux3d::imgui::UiProbe::item("insp.global.translation", 0);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Global Translation");

                        float globalRotation[3] = { worldEuler.x, worldEuler.y, worldEuler.z };
                        if (ImGui::DragFloat3(ICON_MDI_ROTATE_3D_VARIANT "##GlobalRotation", globalRotation, 0.5f)) {
                            worldRotation = glm::quat(glm::radians(glm::vec3(globalRotation[0], globalRotation[1], globalRotation[2])));
                            changed = true;
                        }
                        ux3d::imgui::UiProbe::item("insp.global.rotation", 0);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Global Rotation (deg)");

                        float globalScale[3] = { worldScale.x, worldScale.y, worldScale.z };
                        if (ImGui::DragFloat3(ICON_MDI_ARROW_EXPAND_ALL "##GlobalScale", globalScale, 0.02f, 0.001f, 100.0f)) {
                            worldScale = glm::vec3(globalScale[0], globalScale[1], globalScale[2]);
                            changed = true;
                        }
                        ux3d::imgui::UiProbe::item("insp.global.scale", 0);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Global Scale");
                        ImGui::PopFont();

                        if (changed) {
                            const glm::mat4 newWorld =
                                glm::translate(glm::mat4(1.0f), worldTranslation) *
                                glm::mat4_cast(worldRotation) *
                                glm::scale(glm::mat4(1.0f), worldScale);
                            glm::vec3 localScale, localTranslation, skew;
                            glm::quat localRotation;
                            glm::vec4 perspective;
                            if (glm::decompose(glm::inverse(parentWorld) * newWorld, localScale, localRotation, localTranslation, skew, perspective)) {
                                node->setTranslation(ux3d::math::dvec3(localTranslation.x, localTranslation.y, localTranslation.z));
                                node->setRotation(ux3d::math::dquat(localRotation.w, localRotation.x, localRotation.y, localRotation.z));
                                node->setScale(ux3d::math::dvec3(localScale.x, localScale.y, localScale.z));
                                revisions.mark(SceneChange::Transform);
                            }
                        }

                        if (ImGui::Button("Reset Transform")) {
                            glm::vec3 localScale, localTranslation, skew;
                            glm::quat localRotation;
                            glm::vec4 perspective;
                            if (glm::decompose(glm::inverse(parentWorld), localScale, localRotation, localTranslation, skew, perspective)) {
                                node->setTranslation(ux3d::math::dvec3(localTranslation.x, localTranslation.y, localTranslation.z));
                                node->setRotation(ux3d::math::dquat(localRotation.w, localRotation.x, localRotation.y, localRotation.z));
                                node->setScale(ux3d::math::dvec3(localScale.x, localScale.y, localScale.z));
                                revisions.mark(SceneChange::Transform);
                            }
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Reset to the identity world transform");
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        // Attached Components
        if (node->getMesh() != nullptr) {
            ImGui::Separator();
            auto* mesh = node->getMesh();
            std::string meshName = mesh->hasName() ? mesh->getName().c_str() : ("Mesh " + std::to_string(mesh->m_ID));
            ImGui::Text("Attached Mesh: %s", meshName.c_str());

            if (ImGui::Button("Inspect Mesh Properties")) {
                selection.selectMesh(mesh);
            }

            // Quick material jumps for primitives
            int primIdx = 0;
            for (auto& prim : mesh->getPrimitives()) {
                if (prim.getMaterial() != nullptr) {
                    auto* mat = prim.getMaterial();
                    std::string matLabel = "Inspect Material: " + (mat->hasName() ? mat->getName().c_str() : ("Material " + std::to_string(mat->m_ID)));
                    ImGui::PushID(primIdx);
                    if (ImGui::Button(matLabel.c_str())) {
                        selection.selectMaterial(mat);
                    }
                    ImGui::PopID();
                }
                ++primIdx;
            }
        }

        if (node->getCamera() != nullptr) {
            ImGui::Separator();
            if (ImGui::Button("Inspect Attached Camera")) {
                selection.selectCamera(node->getCamera());
            }
        }

        if (node->getLight() != nullptr) {
            ImGui::Separator();
            if (ImGui::Button("Inspect Attached Light")) {
                selection.selectLight(node->getLight());
            }
        }

        if (node->getSkin() != nullptr) {
            ImGui::Separator();
            auto* skin = node->getSkin();
            std::string skinLabel = skin->hasName() ? skin->getName().c_str() : ("Skin " + std::to_string(skin->m_ID));
            ImGui::Text("Attached Skin: %s", skinLabel.c_str());
            if (ImGui::Button("Inspect Attached Skin")) {
                selection.selectSkin(skin);
            }
        }

        if (!node->getChildren().empty()) {
            ImGui::Separator();
            std::string chHdr = "Children (" + std::to_string(node->getChildren().size()) + ")";
            if (ImGui::TreeNode(chHdr.c_str())) {
                int cIdx = 0;
                for (auto* ch : node->getChildren()) {
                    if (ch != nullptr) {
                        std::string cName = ch->hasName() ? ch->getName().c_str() : ("Node " + std::to_string(ch->m_ID));
                        ImGui::Text("[%d] %s", cIdx, cName.c_str());
                        ImGui::SameLine();
                        std::string btnLabel = "Select##Ch" + std::to_string(cIdx);
                        if (ImGui::SmallButton(btnLabel.c_str())) {
                            selection.selectNode(ch);
                        }
                    }
                    ++cIdx;
                }
                ImGui::TreePop();
            }
        }

        // A3.7: how Glider reads an ASOBO_sky_portal node (the raw values are in the extension list below).
        if (selection.selectedNode->getUnknownExtensions().count("ASOBO_sky_portal") != 0) {
            namespace asobo = ux3d::gltf::asobo;
            const std::optional<asobo::SkyPortal> portal = asobo::readSkyPortal(
                asobo::parseExtensions(gltfModel, *selection.selectedNode, asobo::Scope::Node));
            if (portal.has_value() && ImGui::CollapsingHeader("Sky Portal (MSFS)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Shape: %s", portal->shapeType == 1 ? "disc" : "unknown (not rendered)");
                ImGui::Text("Disc radius: %.3f m (source_radius %.1f cm)", portal->radiusMetres(), portal->sourceRadiusCm);
                ImGui::Text("Inner circle: %.3f m", portal->innerCircleRadiusMetres());
                ImGui::Text("Cone: inner %.1f deg, outer %.1f deg", portal->innerConeDeg, portal->outerConeDeg);
                ImGui::TextDisabled("Faces into the cabin along the node's +Z axis.");
            } else if (!portal.has_value()) {
                ImGui::TextDisabled("ASOBO_sky_portal is not declared in extensionsUsed: ignored.");
            }
        }

        // Node Vendor Extensions (e.g. ASOBO_gizmo_object, ASOBO_collision_object, ASOBO_macro_light)
        if (!selection.selectedNode->getUnknownExtensions().empty()) {
            std::string nodeExtHeader = "Node Extensions (MSFS) (" + std::to_string(selection.selectedNode->getUnknownExtensions().size()) + ")";
            if (ImGui::CollapsingHeader(nodeExtHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ExtensionInspector::instance().renderExtensions(selection.selectedNode, gltfModel, revisions);
            }
        }
    }

    // -----------------------------------------------------------------
    // Material Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
        if (selection.selectedNode != nullptr) {
            if (ImGui::Button("< Back to Selected Node")) {
                selection.type = SelectionType::Node;
            }
            ImGui::Separator();
        }

        MaterialInspector::render(selection.selectedMaterial, gltfModel, revisions, selection, gltfLayer);
    }

    // -----------------------------------------------------------------
    // Mesh Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
        auto* mesh = selection.selectedMesh;
        ImGui::Text("GPU accessor payload: %s",memoryText(meshGpuBytes(*mesh,gltfLayer)).c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unique base attribute/index buffers referenced by this mesh. Shared across instances; excludes driver padding and generated/skin/morph buffers.");
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Mesh ID: %u", mesh->m_ID);

        char nameBuf[256] = {};
        if (mesh->hasName()) {
            std::strncpy(nameBuf, mesh->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            mesh->setName(nameBuf);
        }

        ImGui::Separator();

        // Mesh Vendor Extensions (e.g. ASOBO_softbody_mesh)
        if (!mesh->getUnknownExtensions().empty()) {
            std::string meshExtHeader = "Mesh Extensions (MSFS) (" + std::to_string(mesh->getUnknownExtensions().size()) + ")";
            if (ImGui::CollapsingHeader(meshExtHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ExtensionInspector::instance().renderExtensions(mesh, gltfModel, revisions);
            }
        }

        auto& prims = mesh->getPrimitives();
        ImGui::Text("Primitives: %zu", prims.size());

        int idx = 0;
        for (auto& prim : prims) {
            std::string primTitle = "Primitive " + std::to_string(idx);
            if (ImGui::TreeNode(primTitle.c_str())) {
                // Primitive Topology Mode
                const char* modeStr = "Triangles";
                switch (prim.getPrimitiveMode()) {
                    case ux3d::gltf::PrimitiveMode::Points: modeStr = "Points"; break;
                    case ux3d::gltf::PrimitiveMode::Lines: modeStr = "Lines"; break;
                    case ux3d::gltf::PrimitiveMode::LineLoop: modeStr = "LineLoop"; break;
                    case ux3d::gltf::PrimitiveMode::LineStrip: modeStr = "LineStrip"; break;
                    case ux3d::gltf::PrimitiveMode::Triangles: modeStr = "Triangles"; break;
                    case ux3d::gltf::PrimitiveMode::TriangleStrip: modeStr = "TriangleStrip"; break;
                    case ux3d::gltf::PrimitiveMode::TriangleFan: modeStr = "TriangleFan"; break;
                    default: break;
                }
                ImGui::Text("Topology Mode: %s", modeStr);

                // Indices & vertices
                if (prim.getIndicesAccessor() != nullptr) {
                    ImGui::Text("Triangles: %u", prim.getIndicesAccessor()->getCount() / 3);
                    ImGui::Text("Indices: %u (Accessor ID: %u)", prim.getIndicesAccessor()->getCount(), prim.getIndicesAccessor()->m_ID);
                }

                // Attributes
                if (ImGui::TreeNode("Vertex Attributes")) {
                    for (const auto& attr : prim.getAttributes()) {
                        std::string attrName = attr.getName().c_str();
                        auto* acc = attr.getAccessor();
                        if (acc != nullptr) {
                            std::string typeStr = ux3d::gltf::attributeTypeToString(acc->getType()).c_str();
                            std::string compStr = ux3d::gltf::GlTypes::componentTypeToString(acc->getComponentType()).c_str();
                            ImGui::BulletText("%s: %u verts [%s %s]", attrName.c_str(), acc->getCount(), typeStr.c_str(), compStr.c_str());
                            if (!acc->getMin().empty() && !acc->getMax().empty() && acc->getMin().size() >= 3 && acc->getMax().size() >= 3) {
                                ImGui::Indent(15.0f);
                                ImGui::TextDisabled("Min: [%.2f, %.2f, %.2f] Max: [%.2f, %.2f, %.2f]",
                                    acc->getMin()[0], acc->getMin()[1], acc->getMin()[2],
                                    acc->getMax()[0], acc->getMax()[1], acc->getMax()[2]);
                                ImGui::Unindent(15.0f);
                            }
                        } else {
                            ImGui::BulletText("%s (no accessor)", attrName.c_str());
                        }
                    }
                    ImGui::TreePop();
                }

                // Morph Targets
                if (!prim.getTargets().empty()) {
                    if (ImGui::TreeNode("Morph Targets")) {
                        int targetIdx = 0;
                        for (const auto& target : prim.getTargets()) {
                            auto mapping = target.getAccessorMapping();
                            ImGui::Text("Target #%d (%zu displacement attributes)", targetIdx, mapping.size());
                            for (const auto& [attrName, acc] : mapping) {
                                if (acc != nullptr) {
                                    ImGui::BulletText("Displacement %s (%u values)", attrName.c_str(), acc->getCount());
                                }
                            }
                            ++targetIdx;
                        }
                        ImGui::TreePop();
                    }
                }

                // Material selector for primitive
                auto* mat = prim.getMaterial();
                std::string currentMatName = mat ? (mat->hasName() ? mat->getName().c_str() : ("Material " + std::to_string(mat->m_ID))) : "None";

                if (ImGui::BeginCombo("Material", currentMatName.c_str())) {
                    for (auto& m : gltfModel.getElements<ux3d::gltf::Material>()) {
                        bool isSel = (mat == &m);
                        std::string mLabel = m.hasName() ? m.getName().c_str() : ("Material " + std::to_string(m.m_ID));
                        if (ImGui::Selectable(mLabel.c_str(), isSel)) {
                            prim.setMaterial(m);
                            revisions.mark(SceneChange::Material);
                        }
                    }
                    ImGui::EndCombo();
                }

                if (mat != nullptr && ImGui::Button("Inspect Material")) {
                    selection.selectMaterial(mat);
                }

                // Material Variants Mapping on this primitive
                if (prim.hasMaterialVariantsMapping()) {
                    auto& map = prim.getMaterialVariantsMapping().value();
                    if (ImGui::TreeNode("Material Variants Mapping")) {
                        for (auto& var : gltfModel.getMaterialVariants()) {
                            if (map.hasVariant(var.m_ID)) {
                                auto* altMat = map.getMaterial(var.m_ID);
                                std::string varName = var.hasName() ? var.getName().c_str() : ("Variant " + std::to_string(var.m_ID));
                                std::string altMatName = altMat ? (altMat->hasName() ? altMat->getName().c_str() : ("Material " + std::to_string(altMat->m_ID))) : "None";
                                ImGui::BulletText("%s -> %s", varName.c_str(), altMatName.c_str());
                                if (altMat != nullptr) {
                                    ImGui::SameLine();
                                    std::string btnId = "Inspect##AltMat" + std::to_string(var.m_ID);
                                    if (ImGui::SmallButton(btnId.c_str())) {
                                        selection.selectMaterial(altMat);
                                    }
                                }
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                // Primitive Vendor Extensions (e.g. ASOBO_material_geometry_decal)
                if (!prim.getUnknownExtensions().empty()) {
                    if (ImGui::TreeNode("Primitive Extensions (MSFS)")) {
                        ExtensionInspector::instance().renderExtensions(&prim, gltfModel, revisions);
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }
            ++idx;
        }
    }

    // -----------------------------------------------------------------
    // Camera Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Camera && selection.selectedCamera != nullptr) {
        auto* cam = selection.selectedCamera;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera ID: %u", cam->m_ID);

        bool isPerspective = (cam->getType() == ux3d::gltf::Camera::ProjectionType::Perspective);
        const char* camTypes[] = { "Perspective", "Orthographic" };
        int typeIdx = isPerspective ? 0 : 1;
        if (ImGui::Combo("Type", &typeIdx, camTypes, 2)) {
            cam->setType(typeIdx == 0 ? ux3d::gltf::Camera::ProjectionType::Perspective : ux3d::gltf::Camera::ProjectionType::Orthographic);
            revisions.mark(SceneChange::SceneCamera);
        }

        if (isPerspective && cam->getCameraPerspective().has_value()) {
            auto& p = cam->getCameraPerspective().value();
            ImGui::PushFont(getMonoFont());
            float fov = static_cast<float>(glm::degrees(p.getYFov()));
            if (ImGui::SliderFloat("Vertical FoV (deg)", &fov, 10.0f, 120.0f, "%.1f")) {
                p.setYFov(glm::radians(fov));
                revisions.mark(SceneChange::SceneCamera);
            }
            float znear = static_cast<float>(p.getZNear());
            if (ImGui::DragFloat("Near Plane", &znear, 0.01f, 0.001f, 100.0f)) {
                p.setZNear(znear);
                revisions.mark(SceneChange::SceneCamera);
            }
            float zfar = p.getZFar().has_value() ? static_cast<float>(p.getZFar().value()) : 1000.0f;
            if (ImGui::DragFloat("Far Plane", &zfar, 1.0f, 1.0f, 10000.0f)) {
                p.setZFar(zfar);
                revisions.mark(SceneChange::SceneCamera);
            }
            ImGui::PopFont();
        }
    }

    // -----------------------------------------------------------------
    // Light Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Light && selection.selectedLight != nullptr) {
        auto* light = selection.selectedLight;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Light ID: %u", light->m_ID);

        // Light color
        auto c = light->getColorOrDefault();
        float color[3] = { static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b) };
        if (ImGui::ColorEdit3("Color", color)) {
            light->setColor(ux3d::math::dvec3(color[0], color[1], color[2]));
            revisions.mark(SceneChange::Light);
        }

        ImGui::PushFont(getMonoFont());
        // Intensity
        float intensity = static_cast<float>(light->getIntensityOrDefault());
        if (ImGui::DragFloat("Intensity", &intensity, 0.5f, 0.0f, 100000.0f, "%.1f")) {
            light->setIntensity(intensity);
            revisions.mark(SceneChange::Light);
        }

        // Range
        float range = static_cast<float>(light->getRangeOrDefault());
        if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 1000.0f, "%.1f")) {
            light->setRange(range);
            revisions.mark(SceneChange::Light);
        }
        ImGui::PopFont();
    }

    // -----------------------------------------------------------------
    // Texture / Image Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Texture && selection.selectedTexture != nullptr) {
        renderBackToMaterialButton(selection, "texture.backToMaterial");
        auto* tex = selection.selectedTexture;
        // Same image the renderer binds, including MSFT_texture_dds (packaged MSFS files
        // have no core source at all).
        ux3d::gltf::Image* texSrc = ux3d::renderer::GltfRenderLayer::pickImageForTexture(
            tex, gltfModel.isExtensionDeclared("MSFT_texture_dds"));
        if (texSrc == nullptr) {
            texSrc = tex->getSource();
        }
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", getTextureDisplayName(tex, texSrc).c_str());
        ImGui::Separator();
        ImGui::Text("glTF Texture ID: %u", tex->m_ID);
        if (tex->hasName()) {
            ImGui::Text("glTF Name: %s", tex->getName().c_str());
        }
        if (texSrc != nullptr) {
            ImGui::Text("Resident GPU payload: %s",memoryText(imageGpuBytes(texSrc,gltfLayer)).c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Estimated uploaded texel/block payload including resident mip levels and linear/sRGB variants. Shared images are counted once in Statistics.");
            auto* src = texSrc;
            std::string imgName = getImageDisplayName(src);
            ImGui::Text("Source Image: %s", imgName.c_str());
            if (!src->getUri().empty()) {
                ImGui::Text("File URI: %s", src->getUri().c_str());
            }

            ensureImageDataDecoded(src);
            const auto& srcData = src->getData();
            auto headerInfo = getImageHeaderDetails(src);

            uint32_t width = srcData.getWidth();
            uint32_t height = srcData.getHeight();
            if (headerInfo.isKtx2 && headerInfo.width > 0 && headerInfo.height > 0) {
                width = headerInfo.width;
                height = headerInfo.height;
            }
            auto res = src->getResolution();
            if (width == 0 && height == 0) {
                if (headerInfo.hasHeader && headerInfo.width > 0 && headerInfo.height > 0) {
                    width = headerInfo.width;
                    height = headerInfo.height;
                } else if (res.size() >= 2) {
                    width = res[0];
                    height = res[1];
                }
            }
            if (width > 0 && height > 0) {
                ImGui::Text("Resolution: %u x %u px", width, height);
            }

            if (headerInfo.isKtx2) {
                ImGui::Text("KTX2: %s (vkFormat %u), %u mip levels",
                            headerInfo.vkFormatName.c_str(), headerInfo.vkFormat, headerInfo.ktx2LevelCount);
                ImGui::Text("File Size: %llu bytes", static_cast<unsigned long long>(headerInfo.fileSize));
            } else {
                uint32_t channels = (headerInfo.hasHeader && headerInfo.channels > 0) ? headerInfo.channels : srcData.getChannelCount();
                if (channels > 0) {
                    const char* chanDesc = (channels == 1) ? "1 (Grayscale)" :
                                           (channels == 2) ? "2 (RG)" :
                                           (channels == 3) ? "3 (RGB)" :
                                           (channels == 4) ? "4 (RGBA)" : "Multi-channel";
                    ImGui::Text("Channels: %s", chanDesc);
                }

                uint32_t bitsPerChannel = (headerInfo.hasHeader && headerInfo.bitsPerChannel > 0) ? headerInfo.bitsPerChannel : srcData.getChannelSize(ux3d::image::Channel::RED);
                if (bitsPerChannel == 0 && channels > 0 && srcData.getPixelSize() > 0) {
                    bitsPerChannel = (srcData.getPixelSize() * 8) / channels;
                }
                if (bitsPerChannel == 0 && !srcData.getPixels().empty()) {
                    bitsPerChannel = (srcData.getFormat() == ux3d::image::Format::FLOAT) ? 32 : 8;
                }

                uint32_t totalBitDepth = (headerInfo.hasHeader && headerInfo.totalBitDepth > 0) ?
                                         headerInfo.totalBitDepth : (bitsPerChannel * (channels > 0 ? channels : 1));

                if (bitsPerChannel > 0) {
                    std::string layout = srcData.getMemoryLayoutString();
                    if (!layout.empty()) {
                        ImGui::Text("Bit Depth: %u-bit per channel (%u-bit total, %s)", bitsPerChannel, totalBitDepth, layout.c_str());
                    } else {
                        ImGui::Text("Bit Depth: %u-bit per channel (%u-bit total)", bitsPerChannel, totalBitDepth);
                    }
                }
            }

            if (ImGui::Button(ICON_MDI_FILE_IMAGE_OUTLINE " Inspect Source Image")) {
                selection.selectImage(texSrc);
            }
        }
        if (tex->getSampler() != nullptr) {
            auto* sampler = tex->getSampler();
            ImGui::Separator();
            ImGui::Text("Sampler ID: %u", sampler->m_ID);
            if (sampler->hasName()) {
                ImGui::Text("Sampler Name: %s", sampler->getName().c_str());
            }
        }
        if (texSrc != nullptr) {
            renderImagePreview(texSrc, gltfLayer,
                               packagedTextures && isPackagedNormalMap(gltfModel, texSrc, ddsDeclared));
        }
    } else if (selection.type == SelectionType::Image && selection.selectedImage != nullptr) {
        ImGui::Text("Resident GPU payload: %s",memoryText(imageGpuBytes(selection.selectedImage,gltfLayer)).c_str());
        auto* img = selection.selectedImage;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", getImageDisplayName(img).c_str());
        ImGui::Separator();
        ImGui::Text("glTF Image ID: %u", img->m_ID);
        if (img->hasName()) {
            ImGui::Text("glTF Name: %s", img->getName().c_str());
        }
        if (!img->getUri().empty()) {
            ImGui::Text("File URI: %s", img->getUri().c_str());
        }
        auto mimeStr = ux3d::imageformats::imageEncodingToString(img->getMimeType());
        if (!mimeStr.empty()) {
            ImGui::Text("MIME Type: %s", mimeStr.c_str());
        }
        size_t byteSize = img->getData().getPixels().size();
        if (byteSize == 0 && img->getEncodedBytes() != nullptr) {
            byteSize = img->getEncodedBytes()->size();
        }
        auto headerInfo = getImageHeaderDetails(img);
        if (headerInfo.isKtx2) {
            // Header facts, not the block-grid dimensions the compressed image::Image carries.
            ImGui::Text("KTX2: %s (vkFormat %u)", headerInfo.vkFormatName.c_str(), headerInfo.vkFormat);
            ImGui::Text("Mip Levels: %u", headerInfo.ktx2LevelCount);
            ImGui::Text("File Size: %llu bytes", static_cast<unsigned long long>(headerInfo.fileSize));
        } else if (byteSize > 0) {
            ImGui::Text("Data Size: %zu bytes", byteSize);
        }

        ensureImageDataDecoded(img);
        const auto& data = img->getData();

        uint32_t width = data.getWidth();
        uint32_t height = data.getHeight();
        if (headerInfo.isKtx2 && headerInfo.width > 0 && headerInfo.height > 0) {
            width = headerInfo.width;
            height = headerInfo.height;
        }
        auto res = img->getResolution();
        if (width == 0 && height == 0) {
            if (headerInfo.hasHeader && headerInfo.width > 0 && headerInfo.height > 0) {
                width = headerInfo.width;
                height = headerInfo.height;
            } else if (res.size() >= 2) {
                width = res[0];
                height = res[1];
            }
        }
        if (width > 0 && height > 0) {
            ImGui::Text("Resolution: %u x %u px", width, height);
        }

        if (!headerInfo.isKtx2) {
            uint32_t channels = (headerInfo.hasHeader && headerInfo.channels > 0) ? headerInfo.channels : data.getChannelCount();
            if (channels > 0) {
                const char* chanDesc = (channels == 1) ? "1 (Grayscale)" :
                                       (channels == 2) ? "2 (RG)" :
                                       (channels == 3) ? "3 (RGB)" :
                                       (channels == 4) ? "4 (RGBA)" : "Multi-channel";
                ImGui::Text("Channels: %s", chanDesc);
            }

            uint32_t bitsPerChannel = (headerInfo.hasHeader && headerInfo.bitsPerChannel > 0) ? headerInfo.bitsPerChannel : data.getChannelSize(ux3d::image::Channel::RED);
            if (bitsPerChannel == 0 && channels > 0 && data.getPixelSize() > 0) {
                bitsPerChannel = (data.getPixelSize() * 8) / channels;
            }
            if (bitsPerChannel == 0 && !data.getPixels().empty()) {
                bitsPerChannel = (data.getFormat() == ux3d::image::Format::FLOAT) ? 32 : 8;
            }

            uint32_t totalBitDepth = (headerInfo.hasHeader && headerInfo.totalBitDepth > 0) ?
                                     headerInfo.totalBitDepth : (bitsPerChannel * (channels > 0 ? channels : 1));

            if (bitsPerChannel > 0) {
                std::string layout = data.getMemoryLayoutString();
                if (!layout.empty()) {
                    ImGui::Text("Bit Depth: %u-bit per channel (%u-bit total, %s)", bitsPerChannel, totalBitDepth, layout.c_str());
                } else {
                    ImGui::Text("Bit Depth: %u-bit per channel (%u-bit total)", bitsPerChannel, totalBitDepth);
                }
            }
        }

        renderImagePreview(img, gltfLayer,
                           packagedTextures && isPackagedNormalMap(gltfModel, img, ddsDeclared));
    }

    // -----------------------------------------------------------------
    // Accessor Inspector (TEXCOORD_n targets from the material inspector)
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Accessor && selection.selectedAccessor != nullptr) {
        renderBackToMaterialButton(selection, "accessor.backToMaterial");
        auto* accessor = selection.selectedAccessor;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Accessor ID: %u", accessor->m_ID);
        ImGui::Separator();

        const std::string typeName = ux3d::gltf::attributeTypeToString(accessor->getType()).c_str();
        const std::string componentName = ux3d::gltf::GlTypes::componentTypeToString(accessor->getComponentType()).c_str();
        ImGui::Text("Type: %s", typeName.empty() ? "Unknown" : typeName.c_str());
        ImGui::Text("Component Type: %s", componentName.empty() ? "Unknown" : componentName.c_str());
        ImGui::Text("Count: %u", accessor->getCount());
        ImGui::Text("Normalized: %s", accessor->isNormalized() ? "Yes" : "No");
        ImGui::Text("Size: %zu bytes", accessor->getSizeInBytes());

        if (auto* bufferView = accessor->getBufferView(); bufferView != nullptr) {
            ImGui::Text("Buffer View: %u", bufferView->m_ID);
            if (bufferView->getBuffer() != nullptr) {
                ImGui::Text("Buffer: %u", bufferView->getBuffer()->m_ID);
            }
            ImGui::Text("View Offset: %u bytes", bufferView->getByteOffset());
            ImGui::Text("View Length: %u bytes", bufferView->getByteLength());
            if (bufferView->getByteStride().has_value()) {
                ImGui::Text("Byte Stride: %u bytes", bufferView->getByteStride().value());
            }
        }
        ImGui::Text("Accessor Offset: %u bytes", accessor->getByteOffset());

        if (!accessor->getMin().empty() || !accessor->getMax().empty()) {
            ImGui::Spacing();
            if (!accessor->getMin().empty()) {
                ImGui::TextUnformatted("Min:");
                for (double value : accessor->getMin()) {
                    ImGui::SameLine();
                    ImGui::Text("%.6g", value);
                }
            }
            if (!accessor->getMax().empty()) {
                ImGui::TextUnformatted("Max:");
                for (double value : accessor->getMax()) {
                    ImGui::SameLine();
                    ImGui::Text("%.6g", value);
                }
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Used by");
        int usageCount = 0;
        for (auto& mesh : gltfModel.getElements<ux3d::gltf::Mesh>()) {
            const std::string meshLabel = mesh.hasName()
                ? mesh.getName().c_str()
                : ("Mesh " + std::to_string(mesh.m_ID));
            int primitiveIndex = 0;
            for (auto& primitive : mesh.getPrimitives()) {
                for (auto& attribute : primitive.getAttributes()) {
                    if (attribute.getAccessor() != accessor) {
                        continue;
                    }
                    ++usageCount;
                    ImGui::BulletText("%s / Primitive %d: %s", meshLabel.c_str(), primitiveIndex,
                                      attribute.getName().c_str());
                }
                if (primitive.getIndicesAccessor() == accessor) {
                    ++usageCount;
                    ImGui::BulletText("%s / Primitive %d: indices", meshLabel.c_str(), primitiveIndex);
                }
                ++primitiveIndex;
            }
        }
        if (usageCount == 0) {
            ImGui::TextDisabled("No mesh attribute or index references this accessor.");
        }
    }

    // -----------------------------------------------------------------
    // Animation Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Animation && selection.selectedAnimation != nullptr) {
        auto* anim = selection.selectedAnimation;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Animation ID: %u", anim->m_ID);

        char nameBuf[256] = {};
        if (anim->hasName()) {
            std::strncpy(nameBuf, anim->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            anim->setName(nameBuf);
        }

        // Find this animation index in gltfModel
        int animIdx = 0;
        int foundIdx = -1;
        float animDuration = 0.0f;
        for (const auto& a : gltfModel.getElements<ux3d::gltf::Animation>()) {
            if (&a == anim) {
                foundIdx = animIdx;
            }
            ++animIdx;
        }

        // Calculate duration of this specific animation
        for (const auto& ch : anim->getChannels()) {
            if (ch.getSampler() != nullptr && ch.getSampler()->getInputAccessor() != nullptr) {
                auto* acc = ch.getSampler()->getInputAccessor();
                if (!acc->getMax().empty()) {
                    float t = static_cast<float>(acc->getMax()[0]);
                    if (t > animDuration) animDuration = t;
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Duration: %.2f seconds", animDuration);
        ImGui::Text("Channels: %zu | Samplers: %zu", anim->getChannels().size(), anim->getSamplers().size());

        // Quick playback controls right in the inspector!
        if (animationPlayer != nullptr && foundIdx >= 0) {
            bool isActiveClip = (animationPlayer->getActiveAnimationIndex() == foundIdx);
            bool isPlaying = isActiveClip && animationPlayer->isPlaying();

            if (isPlaying) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
                if (ImGui::Button(ICON_MDI_PAUSE " Pause", ImVec2(90, 0))) {
                    animationPlayer->pause();
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 1.0f));
                if (ImGui::Button(ICON_MDI_PLAY " Play", ImVec2(90, 0))) {
                    animationPlayer->setActiveAnimationIndex(foundIdx, gltfModel, gltfLayer);
                    animationPlayer->play();
                }
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();
            if (ImGui::Button(ICON_MDI_STOP " Stop", ImVec2(70, 0))) {
                animationPlayer->setActiveAnimationIndex(foundIdx, gltfModel, gltfLayer);
                animationPlayer->stop(gltfLayer);
            }

            if (isActiveClip) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[Playing: %.2fs]", animationPlayer->getCurrentTime());
            }

            if (animDuration > 0.0f) {
                float t = isActiveClip ? animationPlayer->getCurrentTime() : 0.0f;
                if (ImGui::SliderFloat("##AnimInspectorScrubber", &t, 0.0f, animDuration, "%.2fs")) {
                    if (!isActiveClip) {
                        animationPlayer->setActiveAnimationIndex(foundIdx, gltfModel, gltfLayer);
                    }
                    animationPlayer->setCurrentTime(t, gltfLayer);
                }
            }
        }

        ImGui::Separator();

        // Channels detailed tree
        if (ImGui::TreeNodeEx("Channels", ImGuiTreeNodeFlags_DefaultOpen)) {
            int chIdx = 0;
            for (auto& ch : anim->getChannels()) {
                auto* targetNode = ch.getTarget().getNode();
                std::string targetNodeName = targetNode ? (targetNode->hasName() ? targetNode->getName().c_str() : ("Node " + std::to_string(targetNode->m_ID))) : "Unbound";

                const char* pathStr = "Unknown";
                switch (ch.getTarget().getPath()) {
                    case ux3d::gltf::AnimationPath::Translation: pathStr = "Translation"; break;
                    case ux3d::gltf::AnimationPath::Rotation: pathStr = "Rotation"; break;
                    case ux3d::gltf::AnimationPath::Scale: pathStr = "Scale"; break;
                    case ux3d::gltf::AnimationPath::Weights: pathStr = "Weights"; break;
                    default: break;
                }

                std::string chLabel = "Channel #" + std::to_string(chIdx) + " (" + pathStr + ")";
                if (ImGui::TreeNode(chLabel.c_str())) {
                    ImGui::Text("Target Node: %s", targetNodeName.c_str());
                    if (targetNode != nullptr) {
                        ImGui::SameLine();
                        std::string selBtn = "Inspect Node##Ch" + std::to_string(chIdx);
                        if (ImGui::SmallButton(selBtn.c_str())) {
                            selection.selectNode(targetNode);
                        }
                    }

                    ImGui::Text("Target Property: %s", pathStr);

                    if (ch.getSampler() != nullptr) {
                        auto* sampler = ch.getSampler();
                        const char* interpStr = "Linear";
                        if (sampler->getInterpolation() == ux3d::gltf::AnimationInterpolation::Step) interpStr = "Step";
                        else if (sampler->getInterpolation() == ux3d::gltf::AnimationInterpolation::CubicSpline) interpStr = "CubicSpline";

                        ImGui::Text("Interpolation: %s", interpStr);
                        if (sampler->getInputAccessor() != nullptr) {
                            auto* inAcc = sampler->getInputAccessor();
                            float tMin = inAcc->getMin().empty() ? 0.0f : static_cast<float>(inAcc->getMin()[0]);
                            float tMax = inAcc->getMax().empty() ? 0.0f : static_cast<float>(inAcc->getMax()[0]);
                            ImGui::Text("Keyframes: %u (range: [%.2fs .. %.2fs])", inAcc->getCount(), tMin, tMax);
                        }
                        if (sampler->getOutputAccessor() != nullptr) {
                            auto* outAcc = sampler->getOutputAccessor();
                            std::string tStr = ux3d::gltf::attributeTypeToString(outAcc->getType()).c_str();
                            std::string cStr = ux3d::gltf::GlTypes::componentTypeToString(outAcc->getComponentType()).c_str();
                            ImGui::Text("Values: %u [%s %s]", outAcc->getCount(), tStr.c_str(), cStr.c_str());
                        }
                    }

                    // Channel Vendor Extensions (e.g. ASOBO_property_animation)
                    if (!ch.getUnknownExtensions().empty()) {
                        if (ImGui::TreeNode("Channel Extensions (MSFS)")) {
                            ExtensionInspector::instance().renderExtensions(&ch, gltfModel, revisions);
                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }
                ++chIdx;
            }
            ImGui::TreePop();
        }

        // Samplers detailed tree
        if (ImGui::TreeNode("Samplers")) {
            int sIdx = 0;
            for (const auto& sampler : anim->getSamplers()) {
                const char* interpStr = "Linear";
                if (sampler.getInterpolation() == ux3d::gltf::AnimationInterpolation::Step) interpStr = "Step";
                else if (sampler.getInterpolation() == ux3d::gltf::AnimationInterpolation::CubicSpline) interpStr = "CubicSpline";

                std::string sLabel = "Sampler #" + std::to_string(sIdx) + " [" + interpStr + "]";
                if (ImGui::TreeNode(sLabel.c_str())) {
                    ImGui::Text("Interpolation: %s", interpStr);
                    if (sampler.getInputAccessor() != nullptr) {
                        auto* inAcc = sampler.getInputAccessor();
                        float tMin = inAcc->getMin().empty() ? 0.0f : static_cast<float>(inAcc->getMin()[0]);
                        float tMax = inAcc->getMax().empty() ? 0.0f : static_cast<float>(inAcc->getMax()[0]);
                        ImGui::Text("Input Time Accessor ID: %u (%u keys, [%.2fs - %.2fs])", inAcc->m_ID, inAcc->getCount(), tMin, tMax);
                    }
                    if (sampler.getOutputAccessor() != nullptr) {
                        auto* outAcc = sampler.getOutputAccessor();
                        std::string tStr = ux3d::gltf::attributeTypeToString(outAcc->getType()).c_str();
                        std::string cStr = ux3d::gltf::GlTypes::componentTypeToString(outAcc->getComponentType()).c_str();
                        ImGui::Text("Output Values Accessor ID: %u (%u values, %s %s)", outAcc->m_ID, outAcc->getCount(), tStr.c_str(), cStr.c_str());
                    }
                    ImGui::TreePop();
                }
                ++sIdx;
            }
            ImGui::TreePop();
        }

        // Animation Vendor Extensions (e.g. ASOBO_animation_retargeting, ASOBO_property_animation)
        if (!anim->getUnknownExtensions().empty()) {
            std::string animExtHeader = "Animation Extensions (MSFS) (" + std::to_string(anim->getUnknownExtensions().size()) + ")";
            if (ImGui::CollapsingHeader(animExtHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ExtensionInspector::instance().renderExtensions(anim, gltfModel, revisions);
            }
        }
    }

    // -----------------------------------------------------------------
    // Skin Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Skin && selection.selectedSkin != nullptr) {
        auto* skin = selection.selectedSkin;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Skin ID: %u", skin->m_ID);

        char nameBuf[256] = {};
        if (skin->hasName()) {
            std::strncpy(nameBuf, skin->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            skin->setName(nameBuf);
        }

        ImGui::Separator();

        // Skeleton Root
        if (skin->getSkeletonRoot() != nullptr) {
            auto* root = skin->getSkeletonRoot();
            std::string rootName = root->hasName() ? root->getName().c_str() : ("Node " + std::to_string(root->m_ID));
            ImGui::Text("Skeleton Root: %s", rootName.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Select Root")) {
                selection.selectNode(root);
            }
        } else {
            ImGui::Text("Skeleton Root: Default (Scene Root)");
        }

        // Inverse Bind Matrices
        if (skin->getInverseBindMatricesAccessor() != nullptr) {
            auto* ibm = skin->getInverseBindMatricesAccessor();
            ImGui::Text("Inverse Bind Matrices: Accessor %u (%u matrices)", ibm->m_ID, ibm->getCount());
        } else {
            ImGui::Text("Inverse Bind Matrices: Identity");
        }

        // Joints
        auto& joints = skin->getJoints();
        std::string jointsHeader = "Joints (" + std::to_string(joints.size()) + ")";
        if (ImGui::TreeNodeEx(jointsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int jIdx = 0;
            for (auto* jointNode : joints) {
                if (jointNode != nullptr) {
                    std::string jName = jointNode->hasName() ? jointNode->getName().c_str() : ("Node " + std::to_string(jointNode->m_ID));
                    ImGui::Text("[%d] %s", jIdx, jName.c_str());
                    ImGui::SameLine();
                    std::string selBtn = "Select##Joint" + std::to_string(jIdx);
                    if (ImGui::SmallButton(selBtn.c_str())) {
                        selection.selectNode(jointNode);
                    }
                }
                ++jIdx;
            }
            ImGui::TreePop();
        }
    }

    // -----------------------------------------------------------------
    // Material Variant Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::MaterialVariant && selection.selectedVariant != nullptr) {
        auto* var = selection.selectedVariant;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material Variant ID: %u", var->m_ID);
        std::string vName = var->hasName() ? var->getName().c_str() : ("Variant " + std::to_string(var->m_ID));
        ImGui::Text("Variant Name: %s", vName.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("This variant is defined under KHR_materials_variants extension. You can switch to it using the Material Variants section in the Resources or Viewport Settings panel.");
    }

    // -----------------------------------------------------------------
    // Viewport Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Viewport && viewports != nullptr) {
        ViewportInstance* vp = nullptr;
        for (auto& v : *viewports) {
            if (v->id == selection.selectedViewportId) {
                vp = v.get();
                break;
            }
        }

        if (vp != nullptr) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_MDI_MONITOR " Viewport ID: %u", vp->id);

            char nameBuf[256] = {};
            std::strncpy(nameBuf, vp->name.c_str(), sizeof(nameBuf) - 1);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                vp->name = nameBuf;
            }

            ImGui::Separator();

            ViewportSettingsPanel::render(
                vp->settings,
                commandGenerator,
                gltfLayer,
                vp->cameraActor,
                revisions,
                &gltfModel,
                vp->isAttachedToCamera,
                vp->attachedCameraNodeName,
                [vp, &revisions]() {
                    vp->isAttachedToCamera = false;
                    vp->attachedCameraNode = nullptr;
                    vp->attachedCameraNodeId = 0;
                    vp->attachedCameraNodeName = "";
                    revisions.mark(SceneChange::ViewCamera);
                },
                &vp->antiAliasing
            );
        } else {
            ImGui::TextDisabled("Viewport (ID %u) not found.", selection.selectedViewportId);
        }
    }
}

} // namespace ux3d::imgui
