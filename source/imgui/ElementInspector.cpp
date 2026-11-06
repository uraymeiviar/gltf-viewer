// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Element Inspector: ElementInspector::render (node, mesh, primitive, camera, light, texture, image, sampler,
// accessor, animation and skin views). Image previews and header details are in ElementInspectorImages.cpp.
// Everything is one key/value tree (PropertyTree.h): the material, extension and viewport views add their rows and
// groups to the table this function opens.

#include "ElementInspectorImages.h"
#include "imgui/PropertyTree.h"
#include "gltf/extensions/MsftExtensions.h"
#include "gltf/extensions/TilesExtensions.h"
#include <cfloat>

namespace ux3d::imgui {

namespace {

const ImVec4 kWarningColor(0.95f, 0.70f, 0.30f, 1.0f);

template <typename T>
std::string displayName(const T* object, const char* kind)
{
    if (object == nullptr) return "None";
    return object->hasName() ? std::string(object->getName().c_str()) : (std::string(kind) + " " + std::to_string(object->m_ID));
}

// The read-only name row every element view starts with.
template <typename T>
void nameRow(const T* object)
{
    prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The name in the glTF (read only).", "%s",
                  object->hasName() ? object->getName().c_str() : "(unnamed)");
}

// A full-width button in the value cell; returns true when clicked. tooltip (optional) on hover, with a hand cursor.
bool linkButton(const char* label, const char* tooltip = nullptr)
{
    const bool clicked = ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f));
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (tooltip != nullptr) ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

bool renderBackToMaterialButton(SelectionState& selection, const char* probeTag)
{
    if (selection.returnMaterial == nullptr) {
        return false;
    }
    ux3d::gltf::Material* material = selection.returnMaterial;
    const std::string materialName = displayName(material, "Material");
    const std::string tooltip = "Return to " + materialName;
    prop::row(ICON_MDI_ARROW_LEFT, "Back", tooltip.c_str());
    const std::string label = std::string(ICON_MDI_ARROW_LEFT " ") + materialName + "##backToMaterial";
    if (linkButton(label.c_str(), tooltip.c_str())) {
        selection.selectMaterial(material);
    }
    UiProbe::item(probeTag, 0);
    return true;
}

const char* channelDescription(uint32_t channels)
{
    return (channels == 1) ? "1 (Grayscale)" :
           (channels == 2) ? "2 (RG)" :
           (channels == 3) ? "3 (RGB)" :
           (channels == 4) ? "4 (RGBA)" : "Multi-channel";
}

// Resolution, then the KTX2 format, mip levels and file size or the channels and bit depth (decodes the image).
void imageFormatRows(const ux3d::gltf::Image* img, const ImageHeaderDetails& headerInfo)
{
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
        prop::textRow(ICON_MDI_ASPECT_RATIO, "Resolution", nullptr, "%u x %u px", width, height);
    }

    if (headerInfo.isKtx2) {
        // Header facts, not the block-grid dimensions the compressed image::Image carries.
        prop::textRow(ICON_MDI_CHIP, "KTX2 Format", nullptr, "%s (vkFormat %u)", headerInfo.vkFormatName.c_str(), headerInfo.vkFormat);
        prop::textRow(ICON_MDI_LAYERS_OUTLINE, "Mip Levels", nullptr, "%u", headerInfo.ktx2LevelCount);
        prop::textRow(ICON_MDI_HARDDISK, "File Size", nullptr, "%llu bytes", static_cast<unsigned long long>(headerInfo.fileSize));
        return;
    }
    uint32_t channels = (headerInfo.hasHeader && headerInfo.channels > 0) ? headerInfo.channels : data.getChannelCount();
    if (channels > 0) {
        prop::textRow(ICON_MDI_PALETTE, "Channels", nullptr, "%s", channelDescription(channels));
    }

    uint32_t bitsPerChannel = (headerInfo.hasHeader && headerInfo.bitsPerChannel > 0) ? headerInfo.bitsPerChannel : data.getChannelSize(ux3d::image::Channel::RED);
    if (bitsPerChannel == 0 && channels > 0 && data.getPixelSize() > 0) {
        bitsPerChannel = (data.getPixelSize() * 8) / channels;
    }
    if (bitsPerChannel == 0 && img->decodedByteSize() > 0) {
        bitsPerChannel = (data.getFormat() == ux3d::image::Format::FLOAT) ? 32 : 8;
    }

    uint32_t totalBitDepth = (headerInfo.hasHeader && headerInfo.totalBitDepth > 0) ?
                             headerInfo.totalBitDepth : (bitsPerChannel * (channels > 0 ? channels : 1));

    if (bitsPerChannel > 0) {
        std::string layout = data.getMemoryLayoutString();
        if (!layout.empty()) {
            prop::textRow(ICON_MDI_GRADIENT_VERTICAL, "Bit Depth", "Per channel (total, memory layout).",
                          "%u-bit (%u-bit, %s)", bitsPerChannel, totalBitDepth, layout.c_str());
        } else {
            prop::textRow(ICON_MDI_GRADIENT_VERTICAL, "Bit Depth", "Per channel (total).", "%u-bit (%u-bit)", bitsPerChannel, totalBitDepth);
        }
    }
}

// The file's value of a KHR node flag, read-only, with the animated value when it differs.
void nodeFlagRows(const char* icon, const char* label, bool authored, bool now, const char* tooltip, const char* probeTag,
                  long long probeId, const ImVec4& falseColor)
{
    if (!authored) {
        prop::colorTextRow(icon, label, tooltip, falseColor, "false");
    } else {
        prop::textRow(icon, label, tooltip, "true");
    }
    if (probeTag != nullptr) {
        UiProbe::item(probeTag, probeId, false, now);
    }
    if (now != authored) {
        ImGui::PushID(label);
        prop::textRow(ICON_MDI_ANIMATION_PLAY_OUTLINE, "Animated Now", nullptr, "%s", now ? "true" : "false");
        ImGui::PopID();
    }
}

const char* interpolationName(ux3d::gltf::AnimationInterpolation interpolation)
{
    if (interpolation == ux3d::gltf::AnimationInterpolation::Step) return "Step";
    if (interpolation == ux3d::gltf::AnimationInterpolation::CubicSpline) return "CubicSpline";
    return "Linear";
}

std::string accessorFormat(const ux3d::gltf::Accessor* accessor)
{
    const std::string typeName = ux3d::gltf::attributeTypeToString(accessor->getType()).c_str();
    const std::string componentName = ux3d::gltf::GlTypes::componentTypeToString(accessor->getComponentType()).c_str();
    return typeName + " " + componentName;
}

template <typename Values>
std::string numberList(const Values& values)
{
    std::string text;
    char buf[32];
    for (double value : values) {
        std::snprintf(buf, sizeof(buf), "%.6g", value);
        if (!text.empty()) text += "  ";
        text += buf;
    }
    return text;
}

void renderNode(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, SceneRevisions& revisions,
                ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto* node = selection.selectedNode;

    prop::textRow(ICON_MDI_CUBE_OUTLINE, "Node", nullptr, "ID %u", node->m_ID);
    prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The name in the glTF (read only; renaming is disabled in the viewer).", "%s",
                  node->hasName() ? node->getName().c_str() : "(unnamed)");

    // Transform (TRS). Local shows the node's own TRS; Global shows and edits the
    // world-space transform, converting the result back through the parent transform.
    if (prop::group(ICON_MDI_AXIS_ARROW, "Transform", true)) {
        int& space = *ImGui::GetStateStorage()->GetIntRef(ImGui::GetID("transformSpace"), 0);
        prop::row(ICON_MDI_EARTH, "Space", "Local: the node's own TRS. Global: the world transform (edits convert back through the parent).");
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        for (int i = 0; i < 2; ++i) {
            if (i > 0) ImGui::SameLine();
            const bool active = space == i;
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(i == 0 ? "Local##transformLocal" : "Global##transformGlobal", ImVec2(half > 1.0f ? half : 1.0f, 0.0f))) {
                space = i;
            }
            if (active) ImGui::PopStyleColor();
            UiProbe::item(i == 0 ? "insp.transform.local" : "insp.transform.global", 0);
        }

        if (space == 0) {
            auto t = node->getTranslationOrDefault();
            float trans[3] = { static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z) };
            prop::row(ICON_MDI_AXIS_ARROW, "Translation", "Local translation.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat3("##Translation", trans, 0.05f)) {
                node->setTranslation(ux3d::math::dvec3(trans[0], trans[1], trans[2]));
                revisions.mark(SceneChange::Transform);
            }
            ux3d::imgui::UiProbe::item("insp.transform.translation", 0);

            // Rotation (Euler angles in degrees)
            auto r = node->getRotationOrDefault();
            glm::quat q(static_cast<float>(r.w), static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z));
            glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
            float rot[3] = { euler.x, euler.y, euler.z };
            ImGui::PopFont();
            prop::row(ICON_MDI_ROTATE_3D_VARIANT, "Rotation", "Local rotation, Euler angles in degrees.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat3("##Rotation", rot, 0.5f)) {
                glm::quat newQ = glm::quat(glm::radians(glm::vec3(rot[0], rot[1], rot[2])));
                node->setRotation(ux3d::math::dquat(newQ.w, newQ.x, newQ.y, newQ.z));
                revisions.mark(SceneChange::Transform);
            }
            ux3d::imgui::UiProbe::item("insp.transform.rotation", 0);

            auto s = node->getScaleOrDefault();
            float scale[3] = { static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z) };
            ImGui::PopFont();
            prop::row(ICON_MDI_ARROW_EXPAND_ALL, "Scale", "Local scale.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat3("##Scale", scale, 0.02f, 0.001f, 100.0f)) {
                node->setScale(ux3d::math::dvec3(scale[0], scale[1], scale[2]));
                revisions.mark(SceneChange::Transform);
            }
            ux3d::imgui::UiProbe::item("insp.transform.scale", 0);
            ImGui::PopFont();

            prop::row(ICON_MDI_RESTORE, "Reset", "Sets the local transform to identity.");
            if (ImGui::Button("Reset Transform##local", ImVec2(-FLT_MIN, 0.0f))) {
                node->setTranslation(ux3d::math::dvec3(0.0));
                node->setRotation(ux3d::math::dquat(1.0, 0.0, 0.0, 0.0));
                node->setScale(ux3d::math::dvec3(1.0));
                revisions.mark(SceneChange::Transform);
            }
        } else if (gltfLayer == nullptr) {
            prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Global", nullptr, "No renderer scene loaded.");
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
            float globalTranslation[3] = { worldTranslation.x, worldTranslation.y, worldTranslation.z };
            prop::row(ICON_MDI_AXIS_ARROW, "Translation", "Global translation.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat3("##GlobalTranslation", globalTranslation, 0.05f)) {
                worldTranslation = glm::vec3(globalTranslation[0], globalTranslation[1], globalTranslation[2]);
                changed = true;
            }
            ux3d::imgui::UiProbe::item("insp.global.translation", 0);

            float globalRotation[3] = { worldEuler.x, worldEuler.y, worldEuler.z };
            ImGui::PopFont();
            prop::row(ICON_MDI_ROTATE_3D_VARIANT, "Rotation", "Global rotation, Euler angles in degrees.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat3("##GlobalRotation", globalRotation, 0.5f)) {
                worldRotation = glm::quat(glm::radians(glm::vec3(globalRotation[0], globalRotation[1], globalRotation[2])));
                changed = true;
            }
            ux3d::imgui::UiProbe::item("insp.global.rotation", 0);

            float globalScale[3] = { worldScale.x, worldScale.y, worldScale.z };
            ImGui::PopFont();
            prop::row(ICON_MDI_ARROW_EXPAND_ALL, "Scale", "Global scale.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat3("##GlobalScale", globalScale, 0.02f, 0.001f, 100.0f)) {
                worldScale = glm::vec3(globalScale[0], globalScale[1], globalScale[2]);
                changed = true;
            }
            ux3d::imgui::UiProbe::item("insp.global.scale", 0);
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

            prop::row(ICON_MDI_RESTORE, "Reset", "Reset to the identity world transform.");
            if (ImGui::Button("Reset Transform##global", ImVec2(-FLT_MIN, 0.0f))) {
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
            prop::tooltip("Reset to the identity world transform");
        }
        prop::endGroup();
    }

    // Attached components: each opens its own inspector.
    if (node->getMesh() != nullptr || node->getCamera() != nullptr || node->getLight() != nullptr || node->getSkin() != nullptr) {
        if (prop::group(ICON_MDI_PUZZLE_OUTLINE, "Components", true)) {
            if (auto* mesh = node->getMesh(); mesh != nullptr) {
                const std::string meshName = displayName(mesh, "Mesh");
                prop::row(ICON_MDI_VECTOR_TRIANGLE, "Mesh", "The attached mesh. Click to inspect its properties.");
                if (linkButton((meshName + "##inspectMesh").c_str(), "Inspect Mesh Properties")) {
                    selection.selectMesh(mesh);
                }
                // Quick material jumps for primitives
                int primIdx = 0;
                for (auto& prim : mesh->getPrimitives()) {
                    if (prim.getMaterial() != nullptr) {
                        auto* mat = prim.getMaterial();
                        ImGui::PushID(primIdx);
                        const std::string key = "Primitive " + std::to_string(primIdx) + " Material";
                        prop::row(ICON_MDI_PALETTE_SWATCH, key.c_str(), "The primitive's material. Click to inspect it.");
                        if (linkButton((displayName(mat, "Material") + "##inspectMaterial").c_str(), "Inspect Material")) {
                            selection.selectMaterial(mat);
                        }
                        ImGui::PopID();
                    }
                    ++primIdx;
                }
            }
            if (node->getCamera() != nullptr) {
                prop::row(ICON_MDI_CAMERA, "Camera", "The attached camera. Click to inspect it.");
                if (linkButton((displayName(node->getCamera(), "Camera") + "##inspectCamera").c_str(), "Inspect Attached Camera")) {
                    selection.selectCamera(node->getCamera());
                }
            }
            if (node->getLight() != nullptr) {
                prop::row(ICON_MDI_LIGHTBULB_OUTLINE, "Light", "The attached light. Click to inspect it.");
                if (linkButton((displayName(node->getLight(), "Light") + "##inspectLight").c_str(), "Inspect Attached Light")) {
                    selection.selectLight(node->getLight());
                }
            }
            if (auto* skin = node->getSkin(); skin != nullptr) {
                prop::row(ICON_MDI_BONE, "Skin", "The attached skin. Click to inspect it.");
                if (linkButton((displayName(skin, "Skin") + "##inspectSkin").c_str(), "Inspect Attached Skin")) {
                    selection.selectSkin(skin);
                }
            }
            prop::endGroup();
        }
    }

    // EXT_mesh_gpu_instancing: how many instances the node draws, or why the extension is not applied, and the
    // instance attributes (each opens its Accessor inspector).
    if (node->hasGpuInstancing()) {
        if (prop::group(ICON_MDI_CONTENT_COPY, "GPU Instancing", true, "EXT_mesh_gpu_instancing")) {
            const size_t instanceCount = node->getGpuInstances().size();
            if (instanceCount != 0) {
                prop::textRow(ICON_MDI_COUNTER, "Instances", nullptr, "%zu", instanceCount);
            } else {
                prop::colorTextRow(ICON_MDI_ALERT_OUTLINE, "Status", "EXT_mesh_gpu_instancing", ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s",
                                   node->getGpuInstancingStatus().empty() ? "not applied" : node->getGpuInstancingStatus().c_str());
            }
            int attributeIndex = 0;
            for (const auto& attribute : node->getGpuInstancingAttributes()) {
                ImGui::PushID(attributeIndex++);
                prop::row(ICON_MDI_DATABASE_OUTLINE, attribute.name.c_str(), "Instance attribute. Click to inspect its accessor.");
                if (attribute.accessor != nullptr) {
                    const std::string label = "Accessor " + std::to_string(attribute.accessor->m_ID);
                    if (linkButton(label.c_str())) {
                        selection.selectAccessor(attribute.accessor);
                    }
                } else {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("invalid accessor");
                }
                ImGui::PopID();
            }
            prop::endGroup();
        }
    }

    // KHR_node_visibility / KHR_node_selectability / KHR_node_hoverability: the file's values, read-only (the
    // viewer's Hide is the eye button, separate from it; an ancestor's false applies to the subtree too).
    if (node->hasNodeVisibility() || node->hasNodeSelectability() || node->hasNodeHoverability()) {
        if (prop::group(ICON_MDI_EYE_OUTLINE, "Node Flags", true, "KHR_node_visibility, KHR_node_selectability, KHR_node_hoverability (read only).")) {
            if (node->hasNodeVisibility()) {
                nodeFlagRows(ICON_MDI_EYE_OUTLINE, "Visible",
                             node->getAuthoredFileVisible(), node->isFileVisible(),
                             "KHR_node_visibility. The file's value; a hidden node and its subtree are not rendered, picked or framed.",
                             nullptr, 0, ImVec4(0.95f, 0.70f, 0.30f, 1.0f));
            }
            if (node->hasNodeSelectability()) {
                nodeFlagRows(ICON_MDI_CURSOR_DEFAULT_CLICK, "Selectable",
                             node->getAuthoredSelectable(), node->isFileSelectable(),
                             "KHR_node_selectability. The file's value; a viewport click passes through a non-selectable node and its "
                             "subtree (the hierarchy still selects it).",
                             "inspector.node_selectable", static_cast<long long>(node->m_ID), ImVec4(0.55f, 0.75f, 0.95f, 1.0f));
            }
            if (node->hasNodeHoverability()) {
                nodeFlagRows(ICON_MDI_CURSOR_DEFAULT_OUTLINE, "Hoverable",
                             node->getAuthoredHoverable(), node->isFileHoverable(),
                             "KHR_node_hoverability. The file's value; read and shown only (Glider has no viewport hover highlight).",
                             nullptr, 0, ImVec4(0.55f, 0.75f, 0.95f, 1.0f));
            }
            prop::endGroup();
        }
    }

    // 3D Tiles: EXT_instance_features feature ID sets, read-only.
    {
        const auto& unknown = node->getUnknownExtensions();
        const auto features = unknown.find(ux3d::gltf::tiles::InstanceFeatures);
        if (features != unknown.end()) {
            const auto sets = ux3d::gltf::tiles::parseFeatureIds(features->second, true);
            const std::string title = "Instance Features (" + std::to_string(sets.size()) + ")";
            if (prop::group(ICON_MDI_DATABASE_OUTLINE, title.c_str(), true, "EXT_instance_features feature ID sets (read only).")) {
                int setIndex = 0;
                for (const auto& set : sets) {
                    ImGui::PushID(setIndex++);
                    prop::textRow(ICON_MDI_IDENTIFIER, set.source.c_str(), nullptr, "%llu features%s",
                                  static_cast<unsigned long long>(set.featureCount),
                                  set.propertyTable >= 0 ? (", property table " + std::to_string(set.propertyTable)).c_str() : "");
                    ImGui::PopID();
                }
                prop::endGroup();
            }
        }
    }

    // MSFT_lod: the LOD chain and screen-coverage hints, read-only (Glider renders this node, the highest LOD).
    if (const auto lod = ux3d::gltf::msft::lod(*node); lod.present) {
        const std::string title = "Levels of Detail (" + std::to_string(lod.ids.size()) + " lower)";
        if (prop::group(ICON_MDI_STAIRS_DOWN, title.c_str(), true, "MSFT_lod. Glider draws the highest LOD only (no LOD switching).")) {
            const auto& nodes = gltfModel.getNodes();
            for (size_t i = 0; i <= lod.ids.size(); ++i) {
                const int index = i == 0 ? -1 : lod.ids[i - 1];
                std::string label = i == 0 ? std::string("this node (rendered)")
                    : (index >= 0 && static_cast<size_t>(index) < nodes.size()
                           ? "node " + std::to_string(index) + " '" + std::string(nodes.at(static_cast<size_t>(index)).getName().c_str()) + "'"
                           : "node " + std::to_string(index) + " (invalid)");
                if (i < lod.screenCoverage.size()) {
                    label += ", screen coverage " + std::to_string(lod.screenCoverage[i]).substr(0, 6);
                }
                const std::string key = "LOD" + std::to_string(i);
                prop::textRow(ICON_MDI_STAIRS, key.c_str(), "Glider draws the highest LOD only (no LOD switching).", "%s", label.c_str());
            }
            prop::endGroup();
        }
    }

    if (!node->getChildren().empty()) {
        const std::string title = "Children (" + std::to_string(node->getChildren().size()) + ")";
        if (prop::group(ICON_MDI_FILE_TREE_OUTLINE, title.c_str(), false)) {
            int cIdx = 0;
            for (auto* ch : node->getChildren()) {
                if (ch != nullptr) {
                    ImGui::PushID(cIdx);
                    const std::string key = "[" + std::to_string(cIdx) + "]";
                    prop::row(ICON_MDI_CUBE_OUTLINE, key.c_str(), "Click to select this child node.");
                    if (linkButton((displayName(ch, "Node") + "##selectChild").c_str(), "Select")) {
                        selection.selectNode(ch);
                    }
                    ImGui::PopID();
                }
                ++cIdx;
            }
            prop::endGroup();
        }
    }

    // A3.7: how Glider reads an ASOBO_sky_portal node (the raw values are in the extension list below).
    if (node->getUnknownExtensions().count("ASOBO_sky_portal") != 0) {
        namespace asobo = ux3d::gltf::asobo;
        const std::optional<asobo::SkyPortal> portal = asobo::readSkyPortal(
            asobo::parseExtensions(gltfModel, *node, asobo::Scope::Node));
        if (!portal.has_value()) {
            prop::textRow(ICON_MDI_WINDOW_CLOSED_VARIANT, "Sky Portal", "ASOBO_sky_portal is not declared in extensionsUsed: ignored.",
                          "ignored (not declared)");
        } else if (prop::group(ICON_MDI_WINDOW_CLOSED_VARIANT, "Sky Portal (MSFS)", true, "Faces into the cabin along the node's +Z axis.")) {
            prop::textRow(ICON_MDI_SHAPE_OUTLINE, "Shape", nullptr, "%s", portal->shapeType == 1 ? "disc" : "unknown (not rendered)");
            prop::textRow(ICON_MDI_RULER, "Disc Radius", nullptr, "%.3f m (source_radius %.1f cm)", portal->radiusMetres(), portal->sourceRadiusCm);
            prop::textRow(ICON_MDI_CIRCLE_OUTLINE, "Inner Circle", nullptr, "%.3f m", portal->innerCircleRadiusMetres());
            prop::textRow(ICON_MDI_ANGLE_ACUTE, "Cone", nullptr, "inner %.1f deg, outer %.1f deg", portal->innerConeDeg, portal->outerConeDeg);
            prop::endGroup();
        }
    }

    // Node Vendor Extensions (e.g. ASOBO_gizmo_object, ASOBO_collision_object, ASOBO_macro_light)
    if (!node->getUnknownExtensions().empty()) {
        const std::string title = "Node Extensions (MSFS) (" + std::to_string(node->getUnknownExtensions().size()) + ")";
        if (prop::group(ICON_MDI_PUZZLE, title.c_str(), true)) {
            ExtensionInspector::instance().renderExtensions(node, gltfModel, revisions);
            prop::endGroup();
        }
    }
}

void renderPrimitive(ux3d::gltf::Primitive& prim, int idx, SelectionState& selection, ux3d::gltf::Gltf& gltfModel,
                     SceneRevisions& revisions)
{
    const std::string primTitle = "Primitive " + std::to_string(idx);
    if (!prop::group(ICON_MDI_VECTOR_POLYGON, primTitle.c_str(), false)) {
        return;
    }
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
    prop::textRow(ICON_MDI_SHAPE_OUTLINE, "Topology", "The primitive mode.", "%s", modeStr);

    if (prim.getIndicesAccessor() != nullptr) {
        prop::textRow(ICON_MDI_TRIANGLE_OUTLINE, "Triangles", "Index count / 3.", "%u", prim.getIndicesAccessor()->getCount() / 3);
        prop::textRow(ICON_MDI_FORMAT_LIST_NUMBERED, "Indices", nullptr, "%u (Accessor %u)", prim.getIndicesAccessor()->getCount(),
                      prim.getIndicesAccessor()->m_ID);
    }

    if (prop::group(ICON_MDI_DATABASE_OUTLINE, "Vertex Attributes", false)) {
        int attrIdx = 0;
        for (const auto& attr : prim.getAttributes()) {
            ImGui::PushID(attrIdx++);
            const std::string attrName = attr.getName().c_str();
            auto* acc = attr.getAccessor();
            if (acc != nullptr) {
                std::string tooltip;
                if (acc->getMin().size() >= 3 && acc->getMax().size() >= 3) {
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "Min: [%.2f, %.2f, %.2f]\nMax: [%.2f, %.2f, %.2f]",
                                  acc->getMin()[0], acc->getMin()[1], acc->getMin()[2],
                                  acc->getMax()[0], acc->getMax()[1], acc->getMax()[2]);
                    tooltip = buf;
                }
                prop::textRow(ICON_MDI_VECTOR_POINT, attrName.c_str(), tooltip.empty() ? nullptr : tooltip.c_str(),
                              "%u verts [%s]", acc->getCount(), accessorFormat(acc).c_str());
            } else {
                prop::textRow(ICON_MDI_VECTOR_POINT, attrName.c_str(), nullptr, "(no accessor)");
            }
            ImGui::PopID();
        }
        prop::endGroup();
    }

    if (!prim.getTargets().empty()) {
        if (prop::group(ICON_MDI_VECTOR_CURVE, "Morph Targets", false)) {
            int targetIdx = 0;
            for (const auto& target : prim.getTargets()) {
                auto mapping = target.getAccessorMapping();
                std::string tooltip;
                for (const auto& [attrName, acc] : mapping) {
                    if (acc != nullptr) {
                        if (!tooltip.empty()) tooltip += "\n";
                        tooltip += "Displacement " + std::string(attrName.c_str()) + " (" + std::to_string(acc->getCount()) + " values)";
                    }
                }
                const std::string key = "Target #" + std::to_string(targetIdx);
                prop::textRow(ICON_MDI_VECTOR_CURVE, key.c_str(), tooltip.empty() ? nullptr : tooltip.c_str(),
                              "%zu displacement attributes", mapping.size());
                ++targetIdx;
            }
            prop::endGroup();
        }
    }

    // Material selector for primitive
    auto* mat = prim.getMaterial();
    const std::string currentMatName = mat ? displayName(mat, "Material") : "None";
    prop::row(ICON_MDI_PALETTE_SWATCH, "Material", "The primitive's material; the button inspects it.");
    const float buttonWidth = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(mat != nullptr ? prop::valueWidth(buttonWidth + ImGui::GetStyle().ItemSpacing.x) : -FLT_MIN);
    if (ImGui::BeginCombo("##primMaterial", currentMatName.c_str())) {
        for (auto& m : gltfModel.getElements<ux3d::gltf::Material>()) {
            bool isSel = (mat == &m);
            const std::string mLabel = displayName(&m, "Material");
            ImGui::PushID(static_cast<int>(m.m_ID));
            if (ImGui::Selectable(mLabel.c_str(), isSel)) {
                prim.setMaterial(m);
                revisions.mark(SceneChange::Material);
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (mat != nullptr) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_ARROW_RIGHT "##inspectPrimMaterial", ImVec2(buttonWidth, 0.0f))) {
            selection.selectMaterial(mat);
        }
        prop::tooltip("Inspect Material");
    }

    // Material Variants Mapping on this primitive
    if (prim.hasMaterialVariantsMapping()) {
        auto& map = prim.getMaterialVariantsMapping().value();
        if (prop::group(ICON_MDI_SWAP_HORIZONTAL, "Material Variants Mapping", false, "KHR_materials_variants")) {
            for (auto& var : gltfModel.getMaterialVariants()) {
                if (map.hasVariant(var.m_ID)) {
                    auto* altMat = map.getMaterial(var.m_ID);
                    ImGui::PushID(static_cast<int>(var.m_ID));
                    prop::row(ICON_MDI_SWAP_HORIZONTAL, displayName(&var, "Variant").c_str(), "The material for this variant.");
                    if (altMat != nullptr) {
                        if (linkButton((displayName(altMat, "Material") + "##inspectAltMat").c_str(), "Inspect")) {
                            selection.selectMaterial(altMat);
                        }
                    } else {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("None");
                    }
                    ImGui::PopID();
                }
            }
            prop::endGroup();
        }
    }

    // 3D Tiles: EXT_mesh_features feature ID sets and CESIUM_primitive_outline, read-only.
    {
        const auto& unknown = prim.getUnknownExtensions();
        const auto features = unknown.find(ux3d::gltf::tiles::MeshFeatures);
        const bool outline = unknown.count(ux3d::gltf::tiles::PrimitiveOutline) != 0;
        const bool metadata = unknown.count(ux3d::gltf::tiles::StructuralMetadata) != 0;
        if ((features != unknown.end() || outline || metadata) && prop::group(ICON_MDI_EARTH, "3D Tiles (read-only)", true)) {
            if (features != unknown.end()) {
                const auto sets = ux3d::gltf::tiles::parseFeatureIds(features->second, false);
                prop::textRow(ICON_MDI_DATABASE_OUTLINE, "Feature ID Sets", "EXT_mesh_features", "%zu", sets.size());
                int setIndex = 0;
                for (const auto& set : sets) {
                    ImGui::PushID(setIndex++);
                    prop::textRow(ICON_MDI_IDENTIFIER, set.source.c_str(), nullptr, "%llu features%s%s%s",
                                  static_cast<unsigned long long>(set.featureCount),
                                  set.propertyTable >= 0 ? (", property table " + std::to_string(set.propertyTable)).c_str() : "",
                                  set.label.empty() ? "" : ", label ",
                                  set.label.c_str());
                    ImGui::PopID();
                }
            }
            if (metadata) {
                prop::textRow(ICON_MDI_DATABASE_OUTLINE, "Structural Metadata", "EXT_structural_metadata", "Property textures / attributes");
            }
            if (outline) {
                prop::textRow(ICON_MDI_VECTOR_POLYLINE, "Outline Edges", "CESIUM_primitive_outline", "recognised, not drawn");
            }
            prop::endGroup();
        }
    }

    // Primitive Vendor Extensions (e.g. ASOBO_material_geometry_decal)
    if (!prim.getUnknownExtensions().empty()) {
        if (prop::group(ICON_MDI_PUZZLE, "Primitive Extensions (MSFS)", false)) {
            ExtensionInspector::instance().renderExtensions(&prim, gltfModel, revisions);
            prop::endGroup();
        }
    }
    prop::endGroup();
}

void renderMesh(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, SceneRevisions& revisions,
                ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto* mesh = selection.selectedMesh;
    prop::textRow(ICON_MDI_VECTOR_TRIANGLE, "Mesh", nullptr, "ID %u", mesh->m_ID);
    nameRow(mesh);
    prop::textRow(ICON_MDI_MEMORY, "GPU Payload",
                  "GPU accessor payload: unique base attribute/index buffers referenced by this mesh. Shared across instances; "
                  "excludes driver padding and generated/skin/morph buffers.",
                  "%s", memoryText(meshGpuBytes(*mesh, gltfLayer)).c_str());

    // Mesh Vendor Extensions (e.g. ASOBO_softbody_mesh)
    if (!mesh->getUnknownExtensions().empty()) {
        const std::string title = "Mesh Extensions (MSFS) (" + std::to_string(mesh->getUnknownExtensions().size()) + ")";
        if (prop::group(ICON_MDI_PUZZLE, title.c_str(), true)) {
            ExtensionInspector::instance().renderExtensions(mesh, gltfModel, revisions);
            prop::endGroup();
        }
    }

    auto& prims = mesh->getPrimitives();
    prop::textRow(ICON_MDI_COUNTER, "Primitives", nullptr, "%zu", prims.size());
    int idx = 0;
    for (auto& prim : prims) {
        renderPrimitive(prim, idx, selection, gltfModel, revisions);
        ++idx;
    }
}

void renderTexture(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer,
                   bool packagedTextures, bool ddsDeclared)
{
    renderBackToMaterialButton(selection, "texture.backToMaterial");
    auto* tex = selection.selectedTexture;
    // Same image the renderer binds, including MSFT_texture_dds (packaged MSFS files
    // have no core source at all).
    ux3d::gltf::Image* texSrc = ux3d::renderer::GltfRenderLayer::pickImageForTexture(
        tex, gltfModel.isExtensionDeclared("MSFT_texture_dds"));
    if (texSrc == nullptr) {
        texSrc = tex->getSource();
    }
    prop::textRow(ICON_MDI_TEXTURE, "Texture", nullptr, "%s", getTextureDisplayName(tex, texSrc).c_str());
    prop::textRow(ICON_MDI_IDENTIFIER, "Texture ID", "The glTF texture index.", "%u", tex->m_ID);
    if (tex->hasName()) {
        prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The name in the glTF (read only).", "%s", tex->getName().c_str());
    }
    if (texSrc != nullptr) {
        prop::textRow(ICON_MDI_MEMORY, "GPU Payload",
                      "Resident GPU payload: estimated uploaded texel/block payload including resident mip levels and linear/sRGB "
                      "variants. Shared images are counted once in Statistics.",
                      "%s", memoryText(imageGpuBytes(texSrc, gltfLayer)).c_str());
        auto* src = texSrc;
        prop::row(ICON_MDI_FILE_IMAGE_OUTLINE, "Source Image", "The image the renderer binds. Click to inspect it.");
        if (linkButton((getImageDisplayName(src) + "##inspectSourceImage").c_str(), "Inspect Source Image")) {
            selection.selectImage(texSrc);
        }
        if (!src->getUri().empty()) {
            prop::textRow(ICON_MDI_LINK_VARIANT, "File URI", src->getUri().c_str(), "%s", src->getUri().c_str());
        }
        imageFormatRows(src, getImageHeaderDetails(src));
    }
    if (tex->getSampler() != nullptr) {
        auto* sampler = tex->getSampler();
        prop::textRow(ICON_MDI_TUNE, "Sampler ID", nullptr, "%u", sampler->m_ID);
        if (sampler->hasName()) {
            prop::textRow(ICON_MDI_TAG_OUTLINE, "Sampler Name", nullptr, "%s", sampler->getName().c_str());
        }
    }
    if (texSrc != nullptr) {
        renderImagePreview(texSrc, gltfLayer, packagedTextures && isPackagedNormalMap(gltfModel, texSrc, ddsDeclared));
    }
}

void renderImage(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer,
                 bool packagedTextures, bool ddsDeclared)
{
    auto* img = selection.selectedImage;
    prop::textRow(ICON_MDI_FILE_IMAGE_OUTLINE, "Image", nullptr, "%s", getImageDisplayName(img).c_str());
    prop::textRow(ICON_MDI_IDENTIFIER, "Image ID", "The glTF image index.", "%u", img->m_ID);
    if (img->hasName()) {
        prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The name in the glTF (read only).", "%s", img->getName().c_str());
    }
    prop::textRow(ICON_MDI_MEMORY, "GPU Payload", "Resident GPU payload.", "%s", memoryText(imageGpuBytes(img, gltfLayer)).c_str());
    if (!img->getUri().empty()) {
        prop::textRow(ICON_MDI_LINK_VARIANT, "File URI", img->getUri().c_str(), "%s", img->getUri().c_str());
    }
    auto mimeStr = ux3d::imageformats::imageEncodingToString(img->getMimeType());
    if (!mimeStr.empty()) {
        prop::textRow(ICON_MDI_FILE_DOCUMENT_OUTLINE, "MIME Type", nullptr, "%s", mimeStr.c_str());
    }
    size_t byteSize = img->decodedByteSize();
    if (byteSize == 0 && img->getEncodedBytes() != nullptr) {
        byteSize = img->getEncodedBytes()->size();
    }
    auto headerInfo = getImageHeaderDetails(img);
    if (!headerInfo.isKtx2 && byteSize > 0) {
        prop::textRow(ICON_MDI_HARDDISK, "Data Size", nullptr, "%zu bytes", byteSize);
    }
    imageFormatRows(img, headerInfo);

    renderImagePreview(img, gltfLayer, packagedTextures && isPackagedNormalMap(gltfModel, img, ddsDeclared));
}

void renderAccessor(SelectionState& selection, ux3d::gltf::Gltf& gltfModel)
{
    renderBackToMaterialButton(selection, "accessor.backToMaterial");
    auto* accessor = selection.selectedAccessor;
    prop::textRow(ICON_MDI_DATABASE_OUTLINE, "Accessor", nullptr, "ID %u", accessor->m_ID);

    const std::string typeName = ux3d::gltf::attributeTypeToString(accessor->getType()).c_str();
    const std::string componentName = ux3d::gltf::GlTypes::componentTypeToString(accessor->getComponentType()).c_str();
    prop::textRow(ICON_MDI_SHAPE_OUTLINE, "Type", nullptr, "%s", typeName.empty() ? "Unknown" : typeName.c_str());
    prop::textRow(ICON_MDI_NUMERIC, "Component Type", nullptr, "%s", componentName.empty() ? "Unknown" : componentName.c_str());
    prop::textRow(ICON_MDI_COUNTER, "Count", nullptr, "%u", accessor->getCount());
    prop::textRow(ICON_MDI_PERCENT, "Normalized", nullptr, "%s", accessor->isNormalized() ? "Yes" : "No");
    prop::textRow(ICON_MDI_HARDDISK, "Size", nullptr, "%zu bytes", accessor->getSizeInBytes());

    if (auto* bufferView = accessor->getBufferView(); bufferView != nullptr) {
        if (prop::group(ICON_MDI_DATABASE, "Buffer View", true)) {
            prop::textRow(ICON_MDI_IDENTIFIER, "Buffer View", nullptr, "%u", bufferView->m_ID);
            if (bufferView->getBuffer() != nullptr) {
                prop::textRow(ICON_MDI_DATABASE, "Buffer", nullptr, "%u", bufferView->getBuffer()->m_ID);
            }
            prop::textRow(ICON_MDI_ARROW_RIGHT, "View Offset", nullptr, "%u bytes", bufferView->getByteOffset());
            prop::textRow(ICON_MDI_RULER, "View Length", nullptr, "%u bytes", bufferView->getByteLength());
            if (bufferView->getByteStride().has_value()) {
                prop::textRow(ICON_MDI_ARROW_EXPAND_HORIZONTAL, "Byte Stride", nullptr, "%u bytes", bufferView->getByteStride().value());
            }
            prop::endGroup();
        }
    }
    prop::textRow(ICON_MDI_ARROW_RIGHT, "Accessor Offset", nullptr, "%u bytes", accessor->getByteOffset());

    if (!accessor->getMin().empty()) {
        const std::string text = numberList(accessor->getMin());
        prop::textRow(ICON_MDI_ARROW_DOWN_BOLD, "Min", text.c_str(), "%s", text.c_str());
    }
    if (!accessor->getMax().empty()) {
        const std::string text = numberList(accessor->getMax());
        prop::textRow(ICON_MDI_ARROW_UP_BOLD, "Max", text.c_str(), "%s", text.c_str());
    }

    if (prop::group(ICON_MDI_LINK_VARIANT, "Used By", true)) {
        int usageCount = 0;
        for (auto& mesh : gltfModel.getElements<ux3d::gltf::Mesh>()) {
            const std::string meshLabel = displayName(&mesh, "Mesh");
            int primitiveIndex = 0;
            for (auto& primitive : mesh.getPrimitives()) {
                for (auto& attribute : primitive.getAttributes()) {
                    if (attribute.getAccessor() != accessor) {
                        continue;
                    }
                    ImGui::PushID(usageCount++);
                    const std::string key = meshLabel + " / Primitive " + std::to_string(primitiveIndex);
                    prop::textRow(ICON_MDI_VECTOR_TRIANGLE, key.c_str(), key.c_str(), "%s", attribute.getName().c_str());
                    ImGui::PopID();
                }
                if (primitive.getIndicesAccessor() == accessor) {
                    ImGui::PushID(usageCount++);
                    const std::string key = meshLabel + " / Primitive " + std::to_string(primitiveIndex);
                    prop::textRow(ICON_MDI_VECTOR_TRIANGLE, key.c_str(), key.c_str(), "indices");
                    ImGui::PopID();
                }
                ++primitiveIndex;
            }
        }
        if (usageCount == 0) {
            prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "None", "No mesh attribute or index references this accessor.",
                          "No mesh attribute or index");
        }
        prop::endGroup();
    }
}

void renderAnimation(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, SceneRevisions& revisions,
                     AnimationPlayer* animationPlayer, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto* anim = selection.selectedAnimation;
    prop::textRow(ICON_MDI_ANIMATION_OUTLINE, "Animation", nullptr, "ID %u", anim->m_ID);
    nameRow(anim);

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

    prop::textRow(ICON_MDI_TIMER_OUTLINE, "Duration", nullptr, "%.2f s", animDuration);
    prop::textRow(ICON_MDI_COUNTER, "Channels / Samplers", nullptr, "%zu / %zu", anim->getChannels().size(), anim->getSamplers().size());

    // Quick playback controls right in the inspector
    if (animationPlayer != nullptr && foundIdx >= 0) {
        bool isActiveClip = (animationPlayer->getActiveAnimationIndex() == foundIdx);
        bool isPlaying = isActiveClip && animationPlayer->isPlaying();

        prop::row(ICON_MDI_PLAY_PAUSE, "Playback", "Plays this clip in the viewport.");
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        const ImVec2 buttonSize(half > 1.0f ? half : 1.0f, 0.0f);
        if (isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
            if (ImGui::Button(ICON_MDI_PAUSE " Pause", buttonSize)) {
                animationPlayer->pause();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 1.0f));
            if (ImGui::Button(ICON_MDI_PLAY " Play", buttonSize)) {
                animationPlayer->setActiveAnimationIndex(foundIdx, gltfModel, gltfLayer);
                animationPlayer->play();
            }
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_STOP " Stop", buttonSize)) {
            animationPlayer->setActiveAnimationIndex(foundIdx, gltfModel, gltfLayer);
            animationPlayer->stop(gltfLayer);
        }

        if (isActiveClip) {
            prop::colorTextRow(ICON_MDI_CLOCK_OUTLINE, "Playing", nullptr, ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%.2f s",
                               animationPlayer->getCurrentTime());
        }

        if (animDuration > 0.0f) {
            float t = isActiveClip ? animationPlayer->getCurrentTime() : 0.0f;
            prop::row(ICON_MDI_TIMELINE_OUTLINE, "Time", "Scrubs this clip.");
            if (ImGui::SliderFloat("##AnimInspectorScrubber", &t, 0.0f, animDuration, "%.2fs")) {
                if (!isActiveClip) {
                    animationPlayer->setActiveAnimationIndex(foundIdx, gltfModel, gltfLayer);
                }
                animationPlayer->setCurrentTime(t, gltfLayer);
            }
        }
    }

    // Channels detailed tree
    if (prop::group(ICON_MDI_SOURCE_BRANCH, "Channels", true)) {
        int chIdx = 0;
        for (auto& ch : anim->getChannels()) {
            auto* targetNode = ch.getTarget().getNode();
            std::string targetNodeName = targetNode ? displayName(targetNode, "Node") : "Unbound";

            const char* pathStr = "Unknown";
            switch (ch.getTarget().getPath()) {
                case ux3d::gltf::AnimationPath::Translation: pathStr = "Translation"; break;
                case ux3d::gltf::AnimationPath::Rotation: pathStr = "Rotation"; break;
                case ux3d::gltf::AnimationPath::Scale: pathStr = "Scale"; break;
                case ux3d::gltf::AnimationPath::Weights: pathStr = "Weights"; break;
                default: break;
            }
            // KHR_animation_pointer and ASOBO_property_animation channels target a property, not a node.
            const auto& targetPointer = ch.getTarget().m_animationPointer;
            if (targetPointer.has_value()) {
                pathStr = std::get<1>(*targetPointer).c_str();
                const auto* targetObject = std::get<0>(*targetPointer);
                targetNodeName = targetObject != nullptr && targetObject->hasName() ? targetObject->getName().c_str() : "Unnamed";
            }

            ImGui::PushID(chIdx);
            const std::string chLabel = "Channel #" + std::to_string(chIdx) + " (" + pathStr + ")";
            if (prop::group(ICON_MDI_CHART_LINE, chLabel.c_str(), false)) {
                if (targetNode != nullptr) {
                    prop::row(ICON_MDI_CUBE_OUTLINE, "Target Node", "Click to inspect the target node.");
                    if (linkButton((targetNodeName + "##inspectTargetNode").c_str(), "Inspect Node")) {
                        selection.selectNode(targetNode);
                    }
                } else {
                    prop::textRow(ICON_MDI_CUBE_OUTLINE, targetPointer.has_value() ? "Target Object" : "Target Node", nullptr, "%s",
                                  targetNodeName.c_str());
                }
                prop::textRow(ICON_MDI_TUNE, "Target Property", nullptr, "%s", pathStr);

                if (ch.getSampler() != nullptr) {
                    auto* sampler = ch.getSampler();
                    prop::textRow(ICON_MDI_SINE_WAVE, "Interpolation", nullptr, "%s", interpolationName(sampler->getInterpolation()));
                    if (sampler->getInputAccessor() != nullptr) {
                        auto* inAcc = sampler->getInputAccessor();
                        float tMin = inAcc->getMin().empty() ? 0.0f : static_cast<float>(inAcc->getMin()[0]);
                        float tMax = inAcc->getMax().empty() ? 0.0f : static_cast<float>(inAcc->getMax()[0]);
                        prop::textRow(ICON_MDI_KEY_VARIANT, "Keyframes", nullptr, "%u [%.2fs .. %.2fs]", inAcc->getCount(), tMin, tMax);
                    }
                    if (sampler->getOutputAccessor() != nullptr) {
                        auto* outAcc = sampler->getOutputAccessor();
                        prop::textRow(ICON_MDI_NUMERIC, "Values", nullptr, "%u [%s]", outAcc->getCount(), accessorFormat(outAcc).c_str());
                    }
                }

                // Channel Vendor Extensions (e.g. ASOBO_property_animation)
                if (!ch.getUnknownExtensions().empty()) {
                    if (prop::group(ICON_MDI_PUZZLE, "Channel Extensions (MSFS)", false)) {
                        ExtensionInspector::instance().renderExtensions(&ch, gltfModel, revisions);
                        prop::endGroup();
                    }
                }
                prop::endGroup();
            }
            ImGui::PopID();
            ++chIdx;
        }
        prop::endGroup();
    }

    // Samplers detailed tree
    if (prop::group(ICON_MDI_SINE_WAVE, "Samplers", false)) {
        int sIdx = 0;
        for (const auto& sampler : anim->getSamplers()) {
            const char* interpStr = interpolationName(sampler.getInterpolation());
            ImGui::PushID(sIdx);
            const std::string sLabel = "Sampler #" + std::to_string(sIdx) + " [" + interpStr + "]";
            if (prop::group(ICON_MDI_SINE_WAVE, sLabel.c_str(), false)) {
                prop::textRow(ICON_MDI_SINE_WAVE, "Interpolation", nullptr, "%s", interpStr);
                if (sampler.getInputAccessor() != nullptr) {
                    auto* inAcc = sampler.getInputAccessor();
                    float tMin = inAcc->getMin().empty() ? 0.0f : static_cast<float>(inAcc->getMin()[0]);
                    float tMax = inAcc->getMax().empty() ? 0.0f : static_cast<float>(inAcc->getMax()[0]);
                    prop::textRow(ICON_MDI_CLOCK_OUTLINE, "Input (Time)", "Input time accessor.", "Accessor %u (%u keys, [%.2fs - %.2fs])",
                                  inAcc->m_ID, inAcc->getCount(), tMin, tMax);
                }
                if (sampler.getOutputAccessor() != nullptr) {
                    auto* outAcc = sampler.getOutputAccessor();
                    prop::textRow(ICON_MDI_NUMERIC, "Output (Values)", "Output values accessor.", "Accessor %u (%u values, %s)",
                                  outAcc->m_ID, outAcc->getCount(), accessorFormat(outAcc).c_str());
                }
                prop::endGroup();
            }
            ImGui::PopID();
            ++sIdx;
        }
        prop::endGroup();
    }

    // Animation Vendor Extensions (e.g. ASOBO_animation_retargeting, ASOBO_property_animation)
    if (!anim->getUnknownExtensions().empty()) {
        const std::string title = "Animation Extensions (MSFS) (" + std::to_string(anim->getUnknownExtensions().size()) + ")";
        if (prop::group(ICON_MDI_PUZZLE, title.c_str(), true)) {
            ExtensionInspector::instance().renderExtensions(anim, gltfModel, revisions);
            prop::endGroup();
        }
    }
}

void renderSkin(SelectionState& selection)
{
    auto* skin = selection.selectedSkin;
    prop::textRow(ICON_MDI_BONE, "Skin", nullptr, "ID %u", skin->m_ID);
    nameRow(skin);

    if (skin->getSkeletonRoot() != nullptr) {
        auto* root = skin->getSkeletonRoot();
        prop::row(ICON_MDI_FAMILY_TREE, "Skeleton Root", "Click to select the root node.");
        if (linkButton((displayName(root, "Node") + "##selectRoot").c_str(), "Select Root")) {
            selection.selectNode(root);
        }
    } else {
        prop::textRow(ICON_MDI_FAMILY_TREE, "Skeleton Root", nullptr, "Default (Scene Root)");
    }

    if (skin->getInverseBindMatricesAccessor() != nullptr) {
        auto* ibm = skin->getInverseBindMatricesAccessor();
        prop::textRow(ICON_MDI_MATRIX, "Inverse Bind Matrices", nullptr, "Accessor %u (%u matrices)", ibm->m_ID, ibm->getCount());
    } else {
        prop::textRow(ICON_MDI_MATRIX, "Inverse Bind Matrices", nullptr, "Identity");
    }

    auto& joints = skin->getJoints();
    const std::string jointsHeader = "Joints (" + std::to_string(joints.size()) + ")";
    if (prop::group(ICON_MDI_BONE, jointsHeader.c_str(), true)) {
        int jIdx = 0;
        for (auto* jointNode : joints) {
            if (jointNode != nullptr) {
                ImGui::PushID(jIdx);
                const std::string key = "[" + std::to_string(jIdx) + "]";
                prop::row(ICON_MDI_BONE, key.c_str(), "Click to select this joint node.");
                if (linkButton((displayName(jointNode, "Node") + "##selectJoint").c_str(), "Select")) {
                    selection.selectNode(jointNode);
                }
                ImGui::PopID();
            }
            ++jIdx;
        }
        prop::endGroup();
    }
}

} // namespace

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

    if (!prop::begin("Inspector")) return;

    if (selection.type == SelectionType::Node && selection.selectedNode != nullptr) {
        renderNode(selection, gltfModel, revisions, gltfLayer);
    } else if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
        if (selection.selectedNode != nullptr) {
            prop::row(ICON_MDI_ARROW_LEFT, "Back", "Return to the selected node.");
            if (linkButton((std::string(ICON_MDI_ARROW_LEFT " ") + displayName(selection.selectedNode, "Node") + "##backToNode").c_str(),
                           "Back to Selected Node")) {
                selection.type = SelectionType::Node;
            }
        }
        MaterialInspector::render(selection.selectedMaterial, gltfModel, revisions, selection, gltfLayer);
    } else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
        renderMesh(selection, gltfModel, revisions, gltfLayer);
    } else if (selection.type == SelectionType::Camera && selection.selectedCamera != nullptr) {
        auto* cam = selection.selectedCamera;
        prop::textRow(ICON_MDI_CAMERA, "Camera", nullptr, "ID %u", cam->m_ID);

        bool isPerspective = (cam->getType() == ux3d::gltf::Camera::ProjectionType::Perspective);
        const char* camTypes[] = { "Perspective", "Orthographic" };
        int typeIdx = isPerspective ? 0 : 1;
        prop::row(ICON_MDI_PROJECTOR, "Type", "The projection.");
        if (ImGui::Combo("##cameraType", &typeIdx, camTypes, 2)) {
            cam->setType(typeIdx == 0 ? ux3d::gltf::Camera::ProjectionType::Perspective : ux3d::gltf::Camera::ProjectionType::Orthographic);
            revisions.mark(SceneChange::SceneCamera);
        }

        if (isPerspective && cam->getCameraPerspective().has_value()) {
            auto& p = cam->getCameraPerspective().value();
            float fov = static_cast<float>(glm::degrees(p.getYFov()));
            prop::row(ICON_MDI_ANGLE_ACUTE, "Vertical FoV", "Degrees.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::SliderFloat("##cameraFov", &fov, 10.0f, 120.0f, "%.1f deg")) {
                p.setYFov(glm::radians(fov));
                revisions.mark(SceneChange::SceneCamera);
            }
            ux3d::imgui::UiProbe::item("insp.camera.fov", 0);
            float znear = static_cast<float>(p.getZNear());
            ImGui::PopFont();
            prop::row(ICON_MDI_ARROW_COLLAPSE_ALL, "Near Plane");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat("##cameraNear", &znear, 0.01f, 0.001f, 100.0f)) {
                p.setZNear(znear);
                revisions.mark(SceneChange::SceneCamera);
            }
            float zfar = p.getZFar().has_value() ? static_cast<float>(p.getZFar().value()) : 1000.0f;
            ImGui::PopFont();
            prop::row(ICON_MDI_ARROW_EXPAND_ALL, "Far Plane");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat("##cameraFar", &zfar, 1.0f, 1.0f, 10000.0f)) {
                p.setZFar(zfar);
                revisions.mark(SceneChange::SceneCamera);
            }
            ImGui::PopFont();
        }
    } else if (selection.type == SelectionType::Light && selection.selectedLight != nullptr) {
        auto* light = selection.selectedLight;
        prop::textRow(ICON_MDI_LIGHTBULB_OUTLINE, "Light", nullptr, "ID %u", light->m_ID);

        auto c = light->getColorOrDefault();
        float color[3] = { static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b) };
        prop::row(ICON_MDI_PALETTE, "Color");
        if (ImGui::ColorEdit3("##lightColor", color)) {
            light->setColor(ux3d::math::dvec3(color[0], color[1], color[2]));
            revisions.mark(SceneChange::Light);
        }

        float intensity = static_cast<float>(light->getIntensityOrDefault());
        prop::row(ICON_MDI_BRIGHTNESS_6, "Intensity");
        ImGui::PushFont(getMonoFont());
        if (ImGui::DragFloat("##lightIntensity", &intensity, 0.5f, 0.0f, 100000.0f, "%.1f")) {
            light->setIntensity(intensity);
            revisions.mark(SceneChange::Light);
        }
        ux3d::imgui::UiProbe::item("insp.light.intensity", 0);

        float range = static_cast<float>(light->getRangeOrDefault());
        ImGui::PopFont();
        prop::row(ICON_MDI_SIGNAL_DISTANCE_VARIANT, "Range");
        ImGui::PushFont(getMonoFont());
        if (ImGui::DragFloat("##lightRange", &range, 0.1f, 0.0f, 1000.0f, "%.1f")) {
            light->setRange(range);
            revisions.mark(SceneChange::Light);
        }
        ImGui::PopFont();
    } else if (selection.type == SelectionType::Texture && selection.selectedTexture != nullptr) {
        renderTexture(selection, gltfModel, gltfLayer, packagedTextures, ddsDeclared);
    } else if (selection.type == SelectionType::Image && selection.selectedImage != nullptr) {
        renderImage(selection, gltfModel, gltfLayer, packagedTextures, ddsDeclared);
    } else if (selection.type == SelectionType::Accessor && selection.selectedAccessor != nullptr) {
        renderAccessor(selection, gltfModel);
    } else if (selection.type == SelectionType::Animation && selection.selectedAnimation != nullptr) {
        renderAnimation(selection, gltfModel, revisions, animationPlayer, gltfLayer);
    } else if (selection.type == SelectionType::Skin && selection.selectedSkin != nullptr) {
        renderSkin(selection);
    } else if (selection.type == SelectionType::MaterialVariant && selection.selectedVariant != nullptr) {
        auto* var = selection.selectedVariant;
        const char* variantTip = "This variant is defined under the KHR_materials_variants extension. You can switch to it using "
                                 "the Material Variants section in the Resources or Viewport Settings panel.";
        prop::textRow(ICON_MDI_SWAP_HORIZONTAL, "Material Variant", variantTip, "ID %u", var->m_ID);
        prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", variantTip, "%s", displayName(var, "Variant").c_str());
    } else if (selection.type == SelectionType::Viewport && viewports != nullptr) {
        ViewportInstance* vp = nullptr;
        for (auto& v : *viewports) {
            if (v->id == selection.selectedViewportId) {
                vp = v.get();
                break;
            }
        }

        if (vp != nullptr) {
            prop::textRow(ICON_MDI_MONITOR, "Viewport", nullptr, "ID %u", vp->id);
            prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The viewport's name (read only).", "%s", vp->name.c_str());

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
            prop::textRow(ICON_MDI_MONITOR, "Viewport", nullptr, "ID %u not found.", selection.selectedViewportId);
        }
    }

    prop::end();
}

} // namespace ux3d::imgui
