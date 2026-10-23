// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝, David Labode

#include "renderer/CommandMapping.h"
#include "renderer/Camera.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"
#include "renderer/Command.h"
#include "renderer/CommandGenerator.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/gl/ShaderCache.h"

#include <iterator>
#include <utility>

using namespace ux3d;
using namespace ux3d::renderer;

namespace {
uint32_t renderSettingsBits(const RenderSettings& rs);
}

CommandMapping::CommandMapping(gl::GL* gl, CommandGenerator& scene, gl::ShaderCache& _shaderCache)
    : m_gl(gl)
    , m_shaderCache(_shaderCache)
    , m_scene(scene)
{
}

size_t CommandMapping::CommandKeyHash::operator()(const CommandKey& key) const
{
    uint64_t h = reinterpret_cast<uintptr_t>(key.mesh) * 0x9E3779B97F4A7C15ULL;
    h ^= reinterpret_cast<uintptr_t>(key.material) + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    h ^= reinterpret_cast<uintptr_t>(key.camera) + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    return static_cast<size_t>(h);
}

CommandMapping::CommandSignature CommandMapping::makeSignature(const Mesh& mesh, const Material& material) const
{
    CommandSignature sig;
    sig.renderPass = material.renderPass;
    sig.environment = m_scene.m_activeEnvironmentMap;
    const auto& lightUniforms = m_scene.m_lights.getUniforms();
    sig.lightUniforms = lightUniforms.data();
    sig.lightUniformCount = lightUniforms.size();
    sig.meshUniforms = mesh.uniforms.data();
    sig.meshUniformCount = mesh.uniforms.size();
    sig.meshBlocks = mesh.uniformBlocks.data();
    sig.meshBlockCount = mesh.uniformBlocks.size();
    sig.materialUniforms = material.uniforms.data();
    sig.materialUniformCount = material.uniforms.size();
    sig.materialSamplers = material.samplers.data();
    sig.materialSamplerCount = material.samplers.size();
    sig.morphTexture = mesh.getMorphTargetSampler().texture;
    sig.jointsSampler = mesh.jointsSampler;
    sig.jointsTexture = mesh.jointsSampler != nullptr ? mesh.jointsSampler->texture : nullptr;
    sig.indexBuffer = mesh.indexBuffer;
    sig.indexCount = mesh.indexCount;
    sig.indexByteOffset = mesh.indexByteOffset;
    sig.vertexCount = mesh.vertexCount;
    sig.meshDefines = mesh.defines.contentHash();
    sig.materialDefines = material.defines.contentHash();
    sig.indexFormat = static_cast<uint32_t>(mesh.indexFormat);
    // Same non-string program inputs as makeProgramKey; the shader names are
    // compared separately without copying.
    sig.renderSettings = renderSettingsBits(material.renderSettings);
    sig.toneMapping = static_cast<uint32_t>(m_scene.m_toneMapping);
    sig.lightCount = static_cast<uint32_t>(m_scene.m_lights.getLightCount());
    sig.useIBL = m_scene.m_useIBL;
    sig.usePunctualLights = m_scene.m_usePunctualLights;
    sig.culling = material.isDoubleSided ? 1u : 0u;
    sig.winding = static_cast<uint32_t>(mesh.frontFace);
    sig.polygonMode = static_cast<uint32_t>(mesh.polygonMode);
    sig.topology = static_cast<uint32_t>(mesh.primitiveTopology);
    return sig;
}

void CommandMapping::update(const core::LinkedList<MeshMaterialTuple>& drawables, std::vector<CmdDesc*>& commands)
{
    ++m_updateSerial;
    commands.clear();
    if (commands.capacity() < drawables.size()) {
        commands.reserve(drawables.size());
    }
    const Camera* camera = m_scene.m_activeCamera;
    for (const auto& meshMat : drawables) {
        const auto [mesh, material] = meshMat;
        if (mesh == nullptr || material == nullptr) {
            continue;
        }
        const CommandKey key { mesh, material, camera };
        CommandSignature signature = makeSignature(*mesh, *material);
        auto it = m_commandCache.find(key);
        if (m_commandCacheEnabled && it != m_commandCache.end() && it->second.signature == signature
            && it->second.vertexShader == mesh->vertexShader && it->second.fragmentShader == material->fragmentShader) {
            ++m_commandCacheHits;
            CmdDesc& desc = *it->second.desc;
            desc.distanceSortKey = camera != nullptr ? camera->getDistance(mesh->getPosition()) : 0.0f;
            desc.command.vertexLayout = m_scene.getVertexLayoutCache().getOrCreate(mesh, desc.command.program);
            it->second.lastUse = m_updateSerial;
            commands.push_back(&desc);
            continue;
        }
        ++m_commandCacheMisses;
        auto command = makeCommand(meshMat);
        if (!command) {
            continue;
        }
        CommandEntry& entry = m_commandCache[key];
        entry.signature = signature;
        entry.vertexShader = mesh->vertexShader;
        entry.fragmentShader = material->fragmentShader;
        entry.desc = std::make_unique<CmdDesc>(std::move(*command));
        entry.lastUse = m_updateSerial;
        commands.push_back(entry.desc.get());
    }

    // Drop entries whose drawables have not appeared for a while (transmission
    // duplicates are recreated every createScene, so their keys do not repeat).
    constexpr uint64_t kEvictAfterUpdates = 64;
    if (m_updateSerial % kEvictAfterUpdates == 0) {
        for (auto entry = m_commandCache.begin(); entry != m_commandCache.end();) {
            if (m_updateSerial - entry->second.lastUse > kEvictAfterUpdates) {
                entry = m_commandCache.erase(entry);
            } else {
                ++entry;
            }
        }
    }
}

namespace {
uint32_t renderSettingsBits(const RenderSettings& rs)
{
    const bool flags[] = { rs.enableSkinning, rs.enableMorphing, rs.enableSpecularGlossiness, rs.enableUnlit,
                           rs.enableClearCoat, rs.enableSheen, rs.enableTransmission, rs.enableVolume,
                           rs.enableIridescence, rs.enableIOR, rs.enableSpecular, rs.enableEmissiveStrength };
    uint32_t bits = 0;
    for (size_t i = 0; i < std::size(flags); ++i) {
        bits |= (flags[i] ? 1u : 0u) << i;
    }
    return bits;
}
}

CommandMapping::ProgramKey CommandMapping::makeProgramKey(const Mesh& mesh, const Material& material) const
{
    ProgramKey key;
    key.meshDefines = mesh.defines.contentHash();
    key.materialDefines = material.defines.contentHash();
    key.vertexShader = mesh.vertexShader;
    key.fragmentShader = material.fragmentShader;
    key.renderSettings = renderSettingsBits(material.renderSettings);
    key.toneMapping = static_cast<uint32_t>(m_scene.m_toneMapping);
    key.lightCount = static_cast<uint32_t>(m_scene.m_lights.getLightCount());
    key.useIBL = m_scene.m_useIBL;
    key.usePunctualLights = m_scene.m_usePunctualLights;
    return key;
}

size_t CommandMapping::ProgramKeyHash::operator()(const ProgramKey& key) const
{
    uint64_t h = key.meshDefines * 0x9E3779B97F4A7C15ULL;
    h ^= key.materialDefines + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    h ^= std::hash<std::string>()(key.vertexShader) + (h << 6) + (h >> 2);
    h ^= std::hash<std::string>()(key.fragmentShader) + (h << 6) + (h >> 2);
    h ^= (static_cast<uint64_t>(key.renderSettings) << 32 | key.toneMapping << 16 | key.lightCount << 2
          | (key.useIBL ? 2u : 0u) | (key.usePunctualLights ? 1u : 0u)) + (h << 6) + (h >> 2);
    return static_cast<size_t>(h);
}

gl::Program* CommandMapping::selectProgram(const Mesh& mesh, const Material& material)
{
    if (!m_programCacheEnabled) {
        ++m_programCacheMisses;
        return buildProgram(mesh, material);
    }
    ProgramKey key = makeProgramKey(mesh, material);
    auto it = m_programCache.find(key);
    if (it != m_programCache.end()) {
        ++m_programCacheHits;
        return it->second;
    }
    ++m_programCacheMisses;
    gl::Program* program = buildProgram(mesh, material);
    if (program != nullptr) {
        m_programCache.emplace(std::move(key), program); // programs live as long as the shader cache
    }
    return program;
}

gl::Program* CommandMapping::buildProgram(const Mesh& meshRef, const Material& materialRef)
{
    const Mesh* mesh = &meshRef;
    const Material* material = &materialRef;

    Defines fragmentDefines;
    fragmentDefines.reserve(64);
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
        return nullptr;
    }

    return m_shaderCache.getProgram(vertexShaderID, fragmentShaderID);
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

    // These lists have knowable upper bounds from the mesh, material, camera,
    // environment and light set. Reserving once avoids repeated reallocation
    // while the per-drawable command is assembled.
    command->uniformBlocks.reserve(mesh->uniformBlocks.size());
    command->uniforms.reserve(
        mesh->uniforms.size() + material->uniforms.size() +
        (m_scene.m_activeCamera != nullptr ? 6u : 0u) + 5u +
        (m_scene.m_useIBL ? 1u : 0u) +
        m_scene.m_lights.getUniforms().size() + 1u);
    command->samplers.reserve(material->samplers.size() + 2u + 6u);

    // Compile vertex and fragment shaders, and pass proper defines to both.
    // Program selection is cached on its exact inputs (P4.6).
    gl::Program* program = selectProgram(*mesh, *material);
    if (program == nullptr) {
        return std::nullopt;
    }

    command->program = program;
    command->vertexLayout = m_scene.getVertexLayoutCache().getOrCreate(mesh, program);

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
