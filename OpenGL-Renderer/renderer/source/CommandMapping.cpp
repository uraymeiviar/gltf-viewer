// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝, David Labode

#include "renderer/CommandMapping.h"
#include "renderer/Command.h"
#include "renderer/CommandGenerator.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/gl/ShaderCache.h"

using namespace ux3d;
using namespace ux3d::renderer;

CommandMapping::CommandMapping(gl::GL* gl, CommandGenerator& scene, gl::ShaderCache& _shaderCache)
    : m_gl(gl)
    , m_shaderCache(_shaderCache)
    , m_scene(scene)
{
}

std::vector<CmdDesc> CommandMapping::update(const core::LinkedList<MeshMaterialTuple>& drawables)
{
    std::vector<CmdDesc> commands;
    commands.reserve(drawables.size());
    for (const auto& meshMat : drawables) {
        auto command = makeCommand(meshMat);
        if (command) {
            commands.push_back(command.value());
        }
    }
    return commands;
}

std::optional<CmdDesc> CommandMapping::makeCommand(MeshMaterialTuple meshMat)
{
    const auto [mesh, material] = meshMat;
    auto desc = CmdDesc(m_gl, renderer::CommandTag::Draw, material->renderPass);
    renderer::Command* command = &desc.command;
    desc.pass = material->renderPass;

    if (core::assertion(material->renderPass != nullptr, "Material has not valid RenderPass assigned") == false) {
        return std::nullopt;
    }

    command->uniformBlocks.clear();
    command->uniforms.clear();
    command->samplers.clear();

    // Compile vertex and fragment shaders, and pass proper defines to both.

    Defines fragmentDefines;
    fragmentDefines.define("DEBUG_NONE", static_cast<int>(RenderMode::None));
    fragmentDefines.define("DEBUG_WIREFRAME", static_cast<int>(RenderMode::Wireframe));
    fragmentDefines.define("DEBUG_NORMAL_TEXTURE", static_cast<int>(RenderMode::NormalTexture));
    fragmentDefines.define("DEBUG_NORMAL_SHADING", static_cast<int>(RenderMode::ShadingNormal));
    fragmentDefines.define("DEBUG_NORMAL_GEOMETRY", static_cast<int>(RenderMode::NormalGeometry));
    fragmentDefines.define("DEBUG_TANGENT", static_cast<int>(RenderMode::Tangent));
    fragmentDefines.define("DEBUG_BITANGENT", static_cast<int>(RenderMode::Bitangent));
    fragmentDefines.define("DEBUG_ALPHA", static_cast<int>(RenderMode::Alpha));

    fragmentDefines.define("DEBUG_UV_0", static_cast<int>(RenderMode::TexCoord_0));
    fragmentDefines.define("DEBUG_UV_1", static_cast<int>(RenderMode::TexCoord_1));

    fragmentDefines.define("DEBUG_OCCLUSION", static_cast<int>(RenderMode::Occlusion));
    fragmentDefines.define("DEBUG_EMISSIVE", static_cast<int>(RenderMode::Emissive));

    fragmentDefines.define("DEBUG_METALLIC_ROUGHNESS", static_cast<int>(RenderMode::MetallicRoughness));
    fragmentDefines.define("DEBUG_ROUGHNESS", static_cast<int>(RenderMode::Roughness));
    fragmentDefines.define("DEBUG_METALLIC", static_cast<int>(RenderMode::Metallic));
    fragmentDefines.define("DEBUG_BASE_COLOR", static_cast<int>(RenderMode::BaseColor));

    fragmentDefines.define("DEBUG_CLEARCOAT", static_cast<int>(RenderMode::Clearcoat));
    fragmentDefines.define("DEBUG_CLEARCOAT_FACTOR", static_cast<int>(RenderMode::ClearcoatFactor));
    fragmentDefines.define("DEBUG_CLEARCOAT_ROUGHNESS", static_cast<int>(RenderMode::ClearcoatRoughness));
    fragmentDefines.define("DEBUG_CLEARCOAT_NORMAL", static_cast<int>(RenderMode::ClearcoatNormal));

    fragmentDefines.define("DEBUG_SHEEN", static_cast<int>(RenderMode::Sheen));
    fragmentDefines.define("DEBUG_SHEEN_COLOR", static_cast<int>(RenderMode::SheenColor));
    fragmentDefines.define("DEBUG_SHEEN_ROUGHNESS", static_cast<int>(RenderMode::SheenRoughness));

    fragmentDefines.define("DEBUG_SPECULAR", static_cast<int>(RenderMode::Specular));
    fragmentDefines.define("DEBUG_SPECULAR_FACTOR", static_cast<int>(RenderMode::SpecularFactor));
    fragmentDefines.define("DEBUG_SPECULAR_COLOR", static_cast<int>(RenderMode::SpecularColor));

    fragmentDefines.define("DEBUG_TRANSMISSION_VOLUME", static_cast<int>(RenderMode::TransmissionVolume));
    fragmentDefines.define("DEBUG_TRANSMISSION_FACTOR", static_cast<int>(RenderMode::TransmissionFactor));
    fragmentDefines.define("DEBUG_VOLUME_THICKNESS", static_cast<int>(RenderMode::VolumeThickness));

    fragmentDefines.define("DEBUG_IRIDESCENCE", static_cast<int>(RenderMode::Iridescence));
    fragmentDefines.define("DEBUG_IRIDESCENCE_FACTOR", static_cast<int>(RenderMode::IridescenceFactor));
    fragmentDefines.define("DEBUG_IRIDESCENCE_THICKNESS", static_cast<int>(RenderMode::IridescenceThickness));

    fragmentDefines.defineAllOf(mesh->defines);
    fragmentDefines.defineAllOf(material->defines);

    bool hasUnlit = fragmentDefines.isDefined("MATERIAL_UNLIT") && material->renderSettings.enableUnlit;
    bool hasSG = fragmentDefines.isDefined("MATERIAL_SPECULARGLOSSINESS") && material->renderSettings.enableSpecularGlossiness;

    // if unlit, we cant use SG
    // if we have SG, we prefer SG (best practice) but if we have neither objects we use MR default values
    bool materialSG = hasSG && !hasUnlit;
    bool materialMR = (!materialSG && fragmentDefines.isDefined("MATERIAL_METALLICROUGHNESS")) || (fragmentDefines.isDefined("MATERIAL_METALLICROUGHNESS") == false && hasSG == false);

    // Supress individual glTF extensions based on global settings.
    if (!materialMR) {
        fragmentDefines.undefine("MATERIAL_METALLICROUGHNESS");
    }
    if (!materialSG) {
        fragmentDefines.undefine("MATERIAL_SPECULARGLOSSINESS");
    }
    if (!hasUnlit) {
        fragmentDefines.undefine("MATERIAL_UNLIT");
    }
    if (!material->renderSettings.enableClearCoat) {
        fragmentDefines.undefine("MATERIAL_CLEARCOAT");
    }
    if (!material->renderSettings.enableSheen) {
        fragmentDefines.undefine("MATERIAL_SHEEN");
    }
    if (!material->renderSettings.enableTransmission) {
        fragmentDefines.undefine("MATERIAL_TRANSMISSION");
    }
    if (!material->renderSettings.enableVolume) {
        fragmentDefines.undefine("MATERIAL_VOLUME");
    }
    if (!material->renderSettings.enableIridescence) {
        fragmentDefines.undefine("MATERIAL_IRIDESCENCE");
    }
    if (!material->renderSettings.enableIOR) {
        fragmentDefines.undefine("MATERIAL_IOR");
    }
    if (!material->renderSettings.enableSpecular) {
        fragmentDefines.undefine("MATERIAL_SPECULAR");
    }
    if (!material->renderSettings.enableEmissiveStrength) {
        fragmentDefines.undefine("MATERIAL_EMISSIVE_STRENGTH");
    }

    switch (m_scene.m_toneMapping) {
    case ToneMappingsEnum::PBR_NEUTRAL:
        fragmentDefines.define("TONEMAP_PBR_NEUTRAL");
        break;
    case ToneMappingsEnum::ACES_HILL:
        fragmentDefines.define("TONEMAP_ACES_HILL");
        break;
    case ToneMappingsEnum::ACES_HILL_EXPOSURE_BOOST:
        fragmentDefines.define("TONEMAP_ACES_HILL_EXPOSURE_BOOST");
        break;
    case ToneMappingsEnum::ACES_NARKOWICZ:
        fragmentDefines.define("TONEMAP_ACES_NARKOWICZ");
        break;
    default:
        break;
    }
    fragmentDefines.defineIf(m_scene.m_useIBL, "USE_IBL");
    bool uploadPunctualLights = m_scene.m_usePunctualLights && m_scene.m_lights.getLightCount() > 0;
    fragmentDefines.defineIf(uploadPunctualLights, "USE_PUNCTUAL");
    fragmentDefines.defineIf(uploadPunctualLights, "LIGHT_COUNT", m_scene.m_lights.getLightCount());
    GLuint fragmentShaderID = m_shaderCache.selectShader(material->fragmentShader, fragmentDefines);
    GLuint vertexShaderID = m_shaderCache.selectShader(mesh->vertexShader, mesh->defines);
    if (vertexShaderID == gl::invalid || fragmentShaderID == gl::invalid) {
        return std::nullopt;
    }

    gl::Program* program = m_shaderCache.getProgram(vertexShaderID, fragmentShaderID);
    if (program == nullptr) {
        return std::nullopt;
    }

    if (program != command->program || true) { // ToDo: optimize in the future to not recreate vertexLayout every frame
        command->program = program;
        command->vertexLayout = program->createVertexLayout(mesh->attributes);
    }

    command->culling = material->isDoubleSided ? gl::Culling::None : gl::Culling::Back;
    command->winding = mesh->frontFace;
    command->polygonMode = mesh->polygonMode;
    command->primitiveTopology = mesh->primitiveTopology;

    if (mesh->indexBuffer != nullptr) {
        command->indexBuffer = mesh->indexBuffer;
        command->primitiveCount = static_cast<GLsizei>(mesh->indexCount);
        command->indexFormat = mesh->indexFormat;
        command->indexByteOffset = static_cast<GLint>(mesh->indexByteOffset);
    } else {
        command->primitiveCount = mesh->vertexCount;
    }

    for (const gl::Uniform& uniform : mesh->uniforms) {
        command->uniforms.push_back(&uniform);
    }

    for (const gl::UniformBlock& block : mesh->uniformBlocks) {
        command->uniformBlocks.push_back(&block);
    }

    if (mesh->getMorphTargetSampler().texture != nullptr) {
        command->samplers.push_back(&mesh->getMorphTargetSampler());
    }

    if (mesh->jointsSampler != nullptr && mesh->jointsSampler->texture != nullptr) {
        command->samplers.push_back(mesh->jointsSampler);
    }

    for (const gl::Uniform& uniform : material->uniforms) {
        command->uniforms.push_back(&uniform);
    }

    for (const gl::Sampler& sampler : material->samplers) {
        command->samplers.push_back(&sampler);
    }

    if (m_scene.m_activeCamera != nullptr) {
        desc.distanceSortKey = m_scene.m_activeCamera->getDistance(mesh->getPosition());

        command->uniforms.push_back(&m_scene.m_activeCamera->getViewUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getProjectionUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getViewProjectionUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getCameraPositionUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getOrthographicScalingUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getViewportResolutionUniform());
    }

    if (m_scene.m_activeEnvironmentMap == nullptr) {
        return std::nullopt;
    }

    command->uniforms.push_back(&m_scene.m_environmentMapRotationUniform);
    command->uniforms.push_back(&m_scene.m_environmentMapIntensityUniform);
    command->uniforms.push_back(&m_scene.m_environmentMapBlurNormalizedUniform);
    command->uniforms.push_back(&m_scene.m_activeEnvironmentMap->getMipCount());

    if (m_scene.m_useIBL) {
        command->uniforms.push_back(&m_scene.m_activeEnvironmentMap->getSheenMipCount());
    }

    command->samplers.push_back(&m_scene.m_activeEnvironmentMap->getDiffuseSampler());
    command->samplers.push_back(&m_scene.m_activeEnvironmentMap->getSpecularSampler());
    command->samplers.push_back(&m_scene.m_activeEnvironmentMap->getSheenSampler());

    command->samplers.push_back(&m_scene.m_activeEnvironmentMap->getGGXLUTSampler());
    command->samplers.push_back(&m_scene.m_activeEnvironmentMap->getCharieLUTSampler());
    command->samplers.push_back(&m_scene.m_activeEnvironmentMap->getSheenLUTSampler());

    for (const auto& uniform : m_scene.m_lights.getUniforms()) {
        command->uniforms.push_back(&uniform);
    }

    command->uniforms.push_back(&m_scene.m_exposureUniform);

    return desc;
}
