// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝, David Labode

#include "renderer/CommandMapping.h"
#include "renderer/Camera.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"
#include "renderer/Command.h"
#include "renderer/CommandGenerator.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/gl/Program.h"
#include "renderer/gl/ShaderCache.h"
#include "renderer/gl/VertexLayout.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <utility>

using namespace ux3d;
using namespace ux3d::renderer;

namespace {
uint32_t renderSettingsBits(const RenderSettings& rs);

// A2.2: the forward fallback's depth bias is a physical distance, not a fixed number of
// depth units. Polygon offset units are multiples of the smallest resolvable window depth,
// whose metres-per-unit changes with the near/far planes, the projection and the fragment
// distance (it grows with z^2 under perspective). 2 mm is chosen from the E195 DECAL_LIT
// measurement: the local decal insets are at most ~0.9 mm, so this has margin, while a
// 2 cm occluder in front stays far outside it. The forward depth buffer is 24-bit.
float decalDepthBiasUnits(const Camera& camera, float distance)
{
    constexpr float kTargetBiasMeters = 0.002f;
    constexpr float kDepthUnits = 16777216.0f; // 2^24
    const float zNear = camera.getZNearPlane();
    const float zFar = camera.getZFarPlane();
    if (zNear <= 0.0f || zFar <= zNear) {
        return 32.0f;
    }
    float metersPerUnit;
    if (camera.getType() == Camera::Type::Orthographic) {
        metersPerUnit = (zFar - zNear) / kDepthUnits;
    } else {
        const float z = (std::max)(distance, zNear);
        metersPerUnit = (zFar - zNear) / (zFar * zNear) * z * z / kDepthUnits;
    }
    return (std::clamp)(kTargetBiasMeters / metersPerUnit, 1.0f, 100000.0f);
}
}

float CommandMapping::computeDistanceSortKey(const Mesh& mesh, const Material& material) const
{
    if (m_scene.m_activeCamera == nullptr) {
        return 0.0f;
    }
    const uint32_t passId = material.renderPass != nullptr ? material.renderPass->getId() : RenderPassType::MaxRenderPasses;
    const bool coveredTransparent = material.drawOrderActive && material.drawOrderClass != DrawOrderClass::None
        && (passId == RenderPassType::Transparent || passId == RenderPassType::Transparent2
            || passId == RenderPassType::DecalAccumulation);
    return m_scene.m_activeCamera->getDistance(coveredTransparent ? mesh.getBoundingCenter() : mesh.getPosition());
}

CommandMapping::CommandMapping(gl::GL* gl, CommandGenerator& scene, gl::ShaderCache& _shaderCache)
    : m_gl(gl)
    , m_shaderCache(_shaderCache)
    , m_scene(scene)
{
}

CommandMapping::~CommandMapping()
{
    clearInstanceGroups();
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
    // Re-filtering may reuse the EnvironmentMap address, but its sampler/uniform
    // storage has been replaced. Reject cached pointers from that earlier instance.
    sig.environmentGeneration = m_scene.m_activeEnvironmentMap ? m_scene.m_activeEnvironmentMap->generation() : 0;
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
    sig.baseVertex = static_cast<uint64_t>(mesh.baseVertex);
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
    sig.winding = static_cast<uint32_t>(mesh.effectiveFrontFace());
    sig.polygonMode = static_cast<uint32_t>(mesh.polygonMode);
    sig.topology = static_cast<uint32_t>(mesh.primitiveTopology);
    sig.drawOrderClass = static_cast<uint32_t>(material.drawOrderClass)
        | (material.decalBiasActive ? 0x100u : 0u)
        | (material.drawOrderActive ? 0x200u : 0u);
    sig.depthBias = material.depthBiasEnabled;
    sig.depthBiasSlope = material.depthBiasEnabled ? material.depthBiasSlope : 0.0f;
    sig.depthBiasUnits = material.depthBiasEnabled ? material.depthBiasUnits : 0.0f;
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
    // A2.2: refresh the physical depth bias of every decal command, cached or new. The
    // command itself is reused while its signature is unchanged, but the units depend on
    // the current near/far planes and view distance.
    const auto applyDecalDepthBias = [camera](CmdDesc& desc, const Mesh& mesh) {
        // A3.2: a material's own depth bias (shadow casters) is fixed, not the decal's physical bias.
        if (!desc.command.polygonOffsetEnabled || camera == nullptr
            || (desc.material != nullptr && desc.material->depthBiasEnabled)) {
            return;
        }
        desc.command.polygonOffsetUnits =
            -decalDepthBiasUnits(*camera, camera->getDistance(mesh.getPosition()));
    };
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
            desc.mesh = mesh;
            desc.material = material;
            desc.distanceSortKey = computeDistanceSortKey(*mesh, *material);
            applyDecalDepthBias(desc, *mesh);
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
        entry.desc->mesh = mesh;
        entry.desc->material = material;
        entry.desc->revision = ++m_nextRevision;
        applyDecalDepthBias(*entry.desc, *mesh);
        prepareInstancing(*entry.desc);
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
        for (auto group = m_instanceGroups.begin(); group != m_instanceGroups.end();) {
            if (m_updateSerial - group->second.lastUse > kEvictAfterUpdates) {
                releaseInstanceGroup(group->second);
                group = m_instanceGroups.erase(group);
            } else {
                ++group;
            }
        }
    }

    mergeInstances(commands);
}

namespace {
uint64_t mix(uint64_t h, uint64_t value)
{
    return h ^ (value + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2));
}

const gl::Uniform* findUniform(const Mesh& mesh, const char* name)
{
    for (const gl::Uniform& uniform : mesh.uniforms) {
        if (uniform.name == name) {
            return &uniform;
        }
    }
    return nullptr;
}

// Per-instance stream: u_ModelMatrix then u_NormalMatrix, 64 bytes each.
constexpr size_t kInstanceStride = 128;
}

size_t CommandMapping::InstanceKeyHash::operator()(const InstanceKey& key) const
{
    uint64_t h = reinterpret_cast<uintptr_t>(key.material) * 0x9E3779B97F4A7C15ULL;
    h = mix(h, reinterpret_cast<uintptr_t>(key.program));
    h = mix(h, reinterpret_cast<uintptr_t>(key.camera));
    h = mix(h, reinterpret_cast<uintptr_t>(key.indexBuffer));
    h = mix(h, key.attributes);
    h = mix(h, key.meshDefines);
    h = mix(h, static_cast<uint64_t>(key.indexByteOffset) ^ (static_cast<uint64_t>(key.primitiveCount) << 32));
    h = mix(h, static_cast<uint64_t>(key.primitiveOffset));
    if (key.baseVertex != 0) {
        h = mix(h, static_cast<uint64_t>(key.baseVertex));
    }
    h = mix(h, key.topology | key.indexFormat << 8 | key.culling << 16 | key.winding << 20 | static_cast<uint64_t>(key.polygonMode) << 32);
    return static_cast<size_t>(h);
}

// Everything checked here is part of the command-cache signature, so it holds until the
// command is rebuilt: the uniform pointers stay valid (the signature includes the mesh's
// uniform storage) and only the transforms they point at change.
void CommandMapping::prepareInstancing(CmdDesc& desc) const
{
    desc.instanceable = false;
    const Mesh* mesh = desc.mesh;
    const Material* material = desc.material;
    if (mesh == nullptr || material == nullptr || desc.pass == nullptr
        || (desc.pass->getId() != RenderPassType::Opaque && desc.pass->getId() != RenderPassType::DecalPrepass)
        || desc.command.tag != CommandTag::Draw || desc.command.program == nullptr || desc.command.instanceCount != 0
        || mesh->vertexShader != "primitive.vert" || !mesh->uniformBlocks.empty() || mesh->jointsSampler != nullptr
        || mesh->getMorphTargetSampler().texture != nullptr || mesh->defines.isDefined("USE_SKINNING")
        || mesh->defines.isDefined("USE_MORPHING") || material->defines.isDefined("MATERIAL_TRANSMISSION")) {
        return;
    }
    desc.modelUniform = findUniform(*mesh, "u_ModelMatrix");
    desc.normalUniform = findUniform(*mesh, "u_NormalMatrix");
    if (desc.modelUniform == nullptr || desc.normalUniform == nullptr) {
        return;
    }
    const Command& command = desc.command;
    InstanceKey& key = desc.instanceKey;
    key = InstanceKey();
    key.material = desc.material;
    key.program = command.program;
    key.indexBuffer = command.indexBuffer;
    uint64_t attributes = 0;
    for (const gl::Attribute& attribute : desc.mesh->attributes) {
        attributes = mix(attributes, std::hash<std::string>()(attribute.name));
        attributes = mix(attributes, reinterpret_cast<uintptr_t>(attribute.buffer));
        attributes = mix(attributes, static_cast<uint64_t>(attribute.offset) | static_cast<uint64_t>(attribute.stride) << 32);
        attributes = mix(attributes, static_cast<uint64_t>(attribute.dimension) | static_cast<uint64_t>(attribute.type) << 8
                                         | static_cast<uint64_t>(attribute.normalized) << 40);
    }
    key.attributes = attributes;
    key.meshDefines = desc.mesh->defines.contentHash();
    key.indexByteOffset = command.indexByteOffset;
    key.baseVertex = command.baseVertex;
    key.primitiveCount = command.primitiveCount;
    key.primitiveOffset = command.primitiveOffset;
    key.topology = static_cast<uint32_t>(command.primitiveTopology);
    key.indexFormat = static_cast<uint32_t>(command.indexFormat);
    key.culling = static_cast<uint32_t>(command.culling);
    key.winding = static_cast<uint32_t>(command.winding);
    key.polygonMode = static_cast<uint32_t>(command.polygonMode);
    desc.instanceable = true;
}

bool CommandMapping::buildInstanceGroup(InstanceGroup& group, const CmdDesc& source)
{
    releaseInstanceGroup(group);
    m_selectInstanced = true;
    auto command = makeCommand({ source.mesh, source.material });
    m_selectInstanced = false;
    if (!command) {
        return false;
    }
    const gl::Program* program = command->command.program;
    const GLint modelLocation = program->findAttributeLocation("a_instanceModelMatrix");
    const GLint normalLocation = program->findAttributeLocation("a_instanceNormalMatrix");
    if (modelLocation < 0) {
        return false;
    }
    group.layout = std::shared_ptr<gl::VertexLayout>(program->createVertexLayout(source.mesh->attributes, source.mesh->indexBuffer));
    m_gl->genBuffers(1, &group.buffer);
    m_gl->bindVertexArray(group.layout->handle);
    m_gl->bindBuffer(GL_ARRAY_BUFFER, group.buffer);
    const std::pair<GLint, size_t> streams[] = { { modelLocation, 0 }, { normalLocation, 64 } };
    for (const auto& [location, offset] : streams) {
        if (location < 0) {
            continue; // e.g. no normals: the normal matrix is unused
        }
        for (GLuint column = 0; column < 4; ++column) {
            const GLuint index = static_cast<GLuint>(location) + column;
            m_gl->enableVertexAttribArray(index);
            m_gl->vertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, kInstanceStride,
                                      reinterpret_cast<const void*>(offset + column * 16));
            m_gl->vertexAttribDivisor(index, 1);
        }
    }
    m_gl->bindVertexArray(0);
    group.desc = std::make_unique<CmdDesc>(std::move(*command));
    group.desc->command.vertexLayout = group.layout;
    group.desc->mesh = source.mesh;
    group.desc->material = source.material;
    group.sourceRevision = source.revision;
    group.data.clear();
    return true;
}

void CommandMapping::mergeInstances(std::vector<CmdDesc*>& commands)
{
    m_instancedDraws = 0;
    m_instancedMembers = 0;
    if (!m_instancingEnabled || commands.size() < 2) {
        return;
    }
    std::unordered_map<InstanceKey, std::vector<size_t>, InstanceKeyHash> buckets;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (commands[i]->instanceable) {
            InstanceKey key = commands[i]->instanceKey;
            key.camera = m_scene.m_activeCamera;
            buckets[key].push_back(i);
        }
    }
    std::vector<CmdDesc*> replacement(commands.size(), nullptr); // instanced draw at its first member's slot
    std::vector<bool> merged(commands.size(), false);
    bool any = false;
    std::vector<uint8_t> data;
    for (const auto& [key, members] : buckets) {
        if (members.size() < 2) {
            continue;
        }
        InstanceGroup& group = m_instanceGroups[key];
        const CmdDesc& first = *commands[members.front()];
        if (!group.desc || group.sourceRevision != first.revision) {
            if (!buildInstanceGroup(group, first)) {
                releaseInstanceGroup(group);
                m_instanceGroups.erase(key);
                continue;
            }
        }
        group.lastUse = m_updateSerial;
        data.resize(members.size() * kInstanceStride);
        float sortKey = first.distanceSortKey;
        for (size_t m = 0; m < members.size(); ++m) {
            const CmdDesc& member = *commands[members[m]];
            std::memcpy(data.data() + m * kInstanceStride, member.modelUniform->bytes(), 64);
            std::memcpy(data.data() + m * kInstanceStride + 64, member.normalUniform->bytes(), 64);
            sortKey = std::min(sortKey, member.distanceSortKey);
            merged[members[m]] = true;
        }
        if (group.data != data) {
            group.data = data;
            m_gl->bindBuffer(GL_ARRAY_BUFFER, group.buffer);
            m_gl->bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size()), data.data(), GL_DYNAMIC_DRAW);
        }
        group.desc->command.instanceCount = static_cast<GLsizei>(members.size());
        group.desc->distanceSortKey = sortKey;
        replacement[members.front()] = group.desc.get();
        ++m_instancedDraws;
        m_instancedMembers += members.size();
        any = true;
    }
    if (!any) {
        return;
    }
    size_t out = 0;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (replacement[i] != nullptr) {
            commands[out++] = replacement[i];
        } else if (!merged[i]) {
            commands[out++] = commands[i];
        }
    }
    commands.resize(out);
}

void CommandMapping::releaseInstanceGroup(InstanceGroup& group)
{
    if (group.buffer != 0) {
        m_gl->deleteBuffers(1, &group.buffer);
        group.buffer = 0;
    }
    group.layout.reset();
    group.desc.reset();
    group.data.clear();
}

void CommandMapping::clearInstanceGroups()
{
    for (auto& [key, group] : m_instanceGroups) {
        releaseInstanceGroup(group);
    }
    m_instanceGroups.clear();
}


namespace {
uint32_t renderSettingsBits(const RenderSettings& rs)
{
    const bool flags[] = { rs.enableSkinning, rs.enableMorphing, rs.enableSpecularGlossiness, rs.enableUnlit,
                           rs.enableClearCoat, rs.enableSheen, rs.enableTransmission, rs.enableVolume,
                           rs.enableIridescence, rs.enableIOR, rs.enableSpecular, rs.enableEmissiveStrength,
                           rs.enableAnisotropy };
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
    key.instanced = m_selectInstanced;
    return key;
}

size_t CommandMapping::ProgramKeyHash::operator()(const ProgramKey& key) const
{
    uint64_t h = key.meshDefines * 0x9E3779B97F4A7C15ULL;
    h ^= key.materialDefines + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    h ^= std::hash<std::string>()(key.vertexShader) + (h << 6) + (h >> 2);
    h ^= std::hash<std::string>()(key.fragmentShader) + (h << 6) + (h >> 2);
    h ^= (static_cast<uint64_t>(key.renderSettings) << 32 | key.toneMapping << 16 | key.lightCount << 2
          | (key.instanced ? 4u : 0u) | (key.useIBL ? 2u : 0u) | (key.usePunctualLights ? 1u : 0u)) + (h << 6) + (h >> 2);
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

    fragmentDefines.define("DEBUG_SSR", static_cast<int>(RenderMode::SSR));
    fragmentDefines.define("DEBUG_SSR_RAY", static_cast<int>(RenderMode::SSRRay));
    fragmentDefines.define("DEBUG_SHADOW_CASCADES", static_cast<int>(RenderMode::ShadowCascades));
    fragmentDefines.define("DEBUG_SHADOW_VISIBILITY", static_cast<int>(RenderMode::ShadowVisibility));
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
    if (!material->renderSettings.enableAnisotropy && !fragmentDefines.isDefined("ASOBO_ANISOTROPY")) {
        // KHR_materials_anisotropy display switch. The ASOBO mapping (ASOBO_ANISOTROPY) shares the
        // define but has its own per-viewport switch (EnvironmentBlock u_AsoboAnisotropyApplied).
        fragmentDefines.undefine("MATERIAL_ANISOTROPY");
        fragmentDefines.undefine("HAS_ANISOTROPY_MAP");
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
    GLuint vertexShaderID = gl::invalid;
    if (m_selectInstanced) {
        Defines vertexDefines = mesh->defines;
        vertexDefines.define("USE_INSTANCING");
        vertexShaderID = m_shaderCache.selectShader(mesh->vertexShader, vertexDefines);
    } else {
        vertexShaderID = m_shaderCache.selectShader(mesh->vertexShader, mesh->defines);
    }
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
    command->samplers.reserve(material->samplers.size() + 2u + 6u + 1u);

    // Compile vertex and fragment shaders, and pass proper defines to both.
    // Program selection is cached on its exact inputs (P4.6).
    gl::Program* program = selectProgram(*mesh, *material);
    if (program == nullptr) {
        return std::nullopt;
    }

    command->program = program;
    command->vertexLayout = m_scene.getVertexLayoutCache().getOrCreate(mesh, program);

    command->culling = material->isDoubleSided ? gl::Culling::None : gl::Culling::Back;
    // The single front-face decision for every pass that builds glTF draw commands.
    // It folds in the mirrored-node rule and the packaged clockwise storage convention.
    command->winding = mesh->effectiveFrontFace();
    command->polygonMode = mesh->polygonMode;
    command->primitiveTopology = mesh->primitiveTopology;
    // A2.2: decals are coplanar (or nearly) with the surface they modify, so pull their
    // depth towards the camera. This is the forward fallback (A1.4) only: the accumulation
    // pass has no depth buffer and performs its own tolerant test, where a polygon offset
    // would corrupt the gl_FragCoord.z that is compared with the prepass depth. The bias
    // follows the viewport's decal switch (decalBiasActive) and, in the fallback, its
    // draw-order switch (drawOrderActive). ASOBO off keeps the A1.4 baseline unchanged.
    const bool accumulationPass = material->renderPass != nullptr
        && material->renderPass->getId() == RenderPassType::DecalAccumulation;
    command->polygonOffsetEnabled = material->drawOrderClass == DrawOrderClass::Decal
        && (material->decalBiasActive || material->drawOrderActive) && !accumulationPass;
    if (command->polygonOffsetEnabled) {
        // A2.2 measured gap (E195 cockpit DECAL_LIT): the MCP text decals sit up to ~0.6 mm
        // behind their button faces and the button meshes up to ~0.3 mm in front. The bias
        // units are derived per frame from the projection and view distance (2 mm target);
        // the slope factor keeps the bias growing with the surface angle.
        command->polygonOffsetFactor = -1.0f;
        command->polygonOffsetUnits = -32.0f; // refreshed by update() before use
    }
    if (material->depthBiasEnabled) {
        // A3.2: the sun shadow caster pass pushes depth away from the sun (slope-scaled plus constant).
        command->polygonOffsetEnabled = true;
        command->polygonOffsetFactor = material->depthBiasSlope;
        command->polygonOffsetUnits = material->depthBiasUnits;
    }

    if (mesh->indexBuffer != nullptr) {
        command->indexBuffer = mesh->indexBuffer;
        command->primitiveCount = static_cast<GLsizei>(mesh->indexCount);
        command->indexFormat = mesh->indexFormat;
        command->indexByteOffset = static_cast<GLint>(mesh->indexByteOffset);
        command->baseVertex = mesh->baseVertex;
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
        desc.distanceSortKey = computeDistanceSortKey(*mesh, *material);

        command->uniforms.push_back(&m_scene.m_activeCamera->getViewUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getProjectionUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getViewProjectionUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getCameraPositionUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getOrthographicScalingUniform());
        command->uniforms.push_back(&m_scene.m_activeCamera->getViewportResolutionUniform());
    }

    // Low/early Auto deliberately skip HDR loading. Unlit/debug and punctual-only
    // draws still work without an environment; only IBL draws require its maps.
    if (m_scene.m_activeEnvironmentMap == nullptr && m_scene.m_useIBL) {
        return std::nullopt;
    }

    command->uniforms.push_back(&m_scene.m_environmentMapRotationUniform);
    command->uniforms.push_back(&m_scene.m_environmentMapIntensityUniform);
    command->uniforms.push_back(&m_scene.m_environmentMapBlurNormalizedUniform);
    if (m_scene.m_activeEnvironmentMap != nullptr) {
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
    }

    for (const auto& uniform : m_scene.m_lights.getUniforms()) {
        command->uniforms.push_back(&uniform);
    }

    command->uniforms.push_back(&m_scene.m_exposureUniform);
    command->uniforms.push_back(&m_scene.m_asoboGlassCopyValidUniform);
    if (material->defines.isDefined("ASOBO_WINDSHIELD_SSR")) {
        command->uniforms.push_back(&m_scene.m_ssrOptions);
        command->uniforms.push_back(&m_scene.m_ssrDepthFloat);
        command->uniforms.push_back(&m_scene.m_ssrFrame);
        command->samplers.push_back(m_scene.m_ssrDepthSampler.get());
    }

    // A2.2: a receiver program samples the six shared accumulation targets. The wrappers
    // and their sampler pointers are stable for the generator's lifetime; only the texture
    // handles change when the buffer is created, resized or released (zero when inactive).
    // They go last so a tight texture-unit limit drops the decal reads, not the IBL ones.
    if (material->defines.isDefined("ASOBO_DECAL_RECEIVER")) {
        for (const auto& sampler : m_scene.m_decalSamplers) {
            command->samplers.push_back(sampler.get());
        }
    }
    // A3.2: the sun shadow map, for the programs that include shadow.glsl (pbr.frag). Last, so every other
    // sampler keeps its texture unit (rendering without the sun is unchanged). The executor logs
    // "Using too many textures" if a program ever exceeds the unit limit with it.
    // A3.6: the ground plane (ground.frag) receives the same shadows.
    if (material->fragmentShader == "pbr.frag" || material->fragmentShader == "ground.frag") {
        command->samplers.push_back(m_scene.m_sunShadowSampler.get());
    }

    // P8.1a: resolve the program's bindings once instead of by name on every draw, and
    // keep only the uniforms the program uses (the others were never committed). Sampler
    // positions are texture units, so samplers keep theirs.
    command->uniformBindings.reserve(command->uniforms.size());
    size_t activeUniforms = 0;
    for (const gl::Uniform* uniform : command->uniforms) {
        const gl::UniformBinding* binding = uniform != nullptr ? program->findUniformBinding(uniform->name) : nullptr;
        if (binding != nullptr) {
            command->uniforms[activeUniforms++] = uniform;
            command->uniformBindings.push_back(binding);
        }
    }
    command->uniforms.resize(activeUniforms);
    command->samplerBindings.reserve(command->samplers.size());
    for (const gl::Sampler* sampler : command->samplers) {
        command->samplerBindings.push_back(sampler != nullptr ? program->findUniformBinding(sampler->name) : nullptr);
    }

    return desc;
}
