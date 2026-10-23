// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneInspector.h"
#include "imgui/StdFileSystem.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "renderer/CommandGenerator.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "gltf/Scene.h"
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <cmath>
#include <cstring>

namespace ux3d::imgui {

void SceneInspector::render(SceneEnvironmentState& env,
                            ux3d::renderer::CommandGenerator* commandGenerator,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            SceneRevisions& revisions,
                            ux3d::gltf::Scene* scene)
{
    if (scene != nullptr) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_MDI_IMAGE_FILTER_HDR " Scene ID: %u", scene->m_ID);

        char nameBuf[256] = {};
        if (scene->hasName()) {
            std::strncpy(nameBuf, scene->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            scene->setName(nameBuf);
            revisions.mark(SceneChange::Metadata);
        }

        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Show background skybox toggle
        if (ImGui::Checkbox("Show Background Skybox", &env.showBackground)) {
            if (gltfLayer != nullptr) {
                gltfLayer->enableCubeMap(env.showBackground);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // Clear color (used when skybox is disabled)
        if (!env.showBackground) {
            float clr[4] = { env.clearColor.r, env.clearColor.g, env.clearColor.b, env.clearColor.a };
            if (ImGui::ColorEdit4("Clear Color", clr)) {
                env.clearColor = glm::vec4(clr[0], clr[1], clr[2], clr[3]);
                revisions.mark(SceneChange::RenderSettings);
            }
        }

        ImGui::Separator();

        // HDR environment map
        ImGui::Text("HDR / EXR Environment: %s", env.currentHdrPath.c_str());
        if (ImGui::Button("Load Custom HDR / EXR...")) {
            auto selection = pfd::open_file("Open HDR / EXR Panorama", "", { "HDR / OpenEXR Environment (*.hdr *.exr)", "*.hdr *.exr", "All Files (*.*)", "*.*" }).result();
            if (!selection.empty() && gltfLayer != nullptr) {
                auto path = ux3d::imgui::StdFileSystem::instance().toPath(selection[0].c_str());
                gltfLayer->setEnvironmentMap(path);
                if (gltfLayer->filterEnvironmentMap()) {
                    env.currentHdrPath = selection[0];
                    env.iblEnabled = true;
                    if (commandGenerator != nullptr) {
                        commandGenerator->setIBLEnabled(true);
                    }
                    revisions.mark(SceneChange::RenderSettings);
                }
            }
        }

        // HDR Blur
        if (ImGui::SliderFloat("HDR Blur", &env.hdrBlur, 0.0f, 1.0f, "%.2f")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapBlurNormalized(env.hdrBlur);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // HDR Intensity
        if (ImGui::SliderFloat("HDR Intensity", &env.hdrIntensity, 0.0f, 10.0f, "%.2f")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapIntensity(env.hdrIntensity);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // HDR Rotation
        if (ImGui::SliderFloat("HDR Rotation", &env.hdrRotation, 0.0f, 360.0f, "%.1f deg")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapRotation(env.hdrRotation);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        ImGui::Separator();

        // Tone Mapping
        const char* toneMappingNames[] = {
            "Khronos PBR Neutral",
            "ACES Hill",
            "ACES Narkowicz"
        };
        if (ImGui::Combo("Tone Mapping", &env.toneMappingIdx, toneMappingNames, 3)) {
            if (commandGenerator != nullptr) {
                if (env.toneMappingIdx == 0) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::PBR_NEUTRAL);
                } else if (env.toneMappingIdx == 1) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
                } else if (env.toneMappingIdx == 2) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_NARKOWICZ);
                }
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // Exposure (EV slider and linear multiplier)
        if (ImGui::SliderFloat("Exposure (EV)", &env.exposureEV, -4.0f, 4.0f, "%.2f EV")) {
            float linearExposure = std::pow(2.0f, env.exposureEV);
            if (commandGenerator != nullptr) {
                commandGenerator->setExposure(linearExposure);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        ImGui::Separator();

        // IBL toggle
        if (ImGui::Checkbox("Image Based Lighting (IBL)", &env.iblEnabled)) {
            if (commandGenerator != nullptr) {
                commandGenerator->setIBLEnabled(env.iblEnabled);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // Punctual lights toggle
        if (ImGui::Checkbox("Punctual / Analytical Lights", &env.punctualLightsEnabled)) {
            if (commandGenerator != nullptr) {
                commandGenerator->setPunctualLightsEnabled(env.punctualLightsEnabled);
            }
            revisions.mark(SceneChange::RenderSettings);
        }
    }
}

} // namespace ux3d::imgui
