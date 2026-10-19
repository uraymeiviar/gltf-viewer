#include <glad/glad.h>
#include "imgui/ElementInspector.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/MaterialInspector.h"
#include "imgui/AnimationPlayer.h"
#include "imgui/TextureUtils.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include "imgui/AppSettings.h"
#include "imgui/StdFileSystem.h"
#include "imageformats/imageformats.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/gl/Texture.h"
#include "gltf/GlTypes.h"
#include "gltf/AttributeType.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>

namespace ux3d::imgui {

struct ImagePreviewEntry {
    GLuint textureId = 0;
    int encodedMapID = -1;
};

static std::unordered_map<const ux3d::gltf::Image*, ImagePreviewEntry> s_previewCache;

void ElementInspector::clearPreviewCache()
{
    for (auto& pair : s_previewCache) {
        if (pair.second.textureId != 0) {
            glDeleteTextures(1, &pair.second.textureId);
        }
    }
    s_previewCache.clear();
}

struct ImageHeaderDetails {
    bool hasHeader = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t bitsPerChannel = 0;
    uint32_t totalBitDepth = 0;
    std::string format;
};

static ImageHeaderDetails parseImageHeader(const uint8_t* data, size_t size)
{
    ImageHeaderDetails d;
    if (!data || size < 8) return d;

    // 1. PNG: 89 50 4E 47 0D 0A 1A 0A
    if (size >= 26 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G' &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        if (data[12] == 'I' && data[13] == 'H' && data[14] == 'D' && data[15] == 'R') {
            d.hasHeader = true;
            d.format = "PNG";
            d.width = (uint32_t(data[16]) << 24) | (uint32_t(data[17]) << 16) | (uint32_t(data[18]) << 8) | uint32_t(data[19]);
            d.height = (uint32_t(data[20]) << 24) | (uint32_t(data[21]) << 16) | (uint32_t(data[22]) << 8) | uint32_t(data[23]);
            d.bitsPerChannel = data[24]; // 1, 2, 4, 8, 16
            uint8_t colorType = data[25];
            switch (colorType) {
            case 0: d.channels = 1; break; // Grayscale
            case 2: d.channels = 3; break; // RGB
            case 3: d.channels = 1; break; // Indexed
            case 4: d.channels = 2; break; // Grayscale + Alpha
            case 6: d.channels = 4; break; // RGBA
            default: d.channels = 3; break;
            }
            d.totalBitDepth = d.bitsPerChannel * d.channels;
            return d;
        }
    }

    // 2. DDS: 'D' 'D' 'S' ' '
    if (size >= 128 && data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ') {
        d.hasHeader = true;
        d.format = "DDS";
        d.height = uint32_t(data[12]) | (uint32_t(data[13]) << 8) | (uint32_t(data[14]) << 16) | (uint32_t(data[15]) << 24);
        d.width  = uint32_t(data[16]) | (uint32_t(data[17]) << 8) | (uint32_t(data[18]) << 16) | (uint32_t(data[19]) << 24);
        uint32_t pfFourCC = uint32_t(data[88]) | (uint32_t(data[89]) << 8) | (uint32_t(data[90]) << 16) | (uint32_t(data[91]) << 24);
        uint32_t rgbBitCount = uint32_t(data[92]) | (uint32_t(data[93]) << 8) | (uint32_t(data[94]) << 16) | (uint32_t(data[95]) << 24);
        if (pfFourCC == 0x30315844 && size >= 148) { // "DX10"
            uint32_t dxgiFormat = uint32_t(data[128]) | (uint32_t(data[129]) << 8) | (uint32_t(data[130]) << 16) | (uint32_t(data[131]) << 24);
            if (dxgiFormat == 10 || dxgiFormat == 11 || dxgiFormat == 12) {
                d.channels = 4; d.bitsPerChannel = 16; d.totalBitDepth = 64;
            } else if (dxgiFormat == 27 || dxgiFormat == 28 || dxgiFormat == 29) {
                d.channels = 4; d.bitsPerChannel = 8; d.totalBitDepth = 32;
            } else if (dxgiFormat >= 70 && dxgiFormat <= 84) {
                d.channels = 4; d.bitsPerChannel = 8; d.totalBitDepth = (dxgiFormat <= 72) ? 4 : 8;
            }
        } else if (rgbBitCount > 0) {
            d.totalBitDepth = rgbBitCount;
            if (rgbBitCount == 64) { d.channels = 4; d.bitsPerChannel = 16; }
            else if (rgbBitCount == 48) { d.channels = 3; d.bitsPerChannel = 16; }
            else if (rgbBitCount == 32) { d.channels = 4; d.bitsPerChannel = 8; }
            else if (rgbBitCount == 24) { d.channels = 3; d.bitsPerChannel = 8; }
            else if (rgbBitCount == 16) { d.channels = 2; d.bitsPerChannel = 8; }
        }
        return d;
    }

    return d;
}

static ImageHeaderDetails getImageHeaderDetails(const ux3d::gltf::Image* img)
{
    if (!img) return {};
    if (img->getEncodedBytes() != nullptr && img->getEncodedBytes()->size() >= 26) {
        return parseImageHeader(img->getEncodedBytes()->data(), img->getEncodedBytes()->size());
    }
    if (!img->getUri().empty()) {
        std::string modelPath = AppSettings::instance().getCurrentModelPath();
        if (!modelPath.empty()) {
            ux3d::filesystem::File mfile = StdFileSystem::instance().toFile(modelPath.c_str());
            ux3d::filesystem::Path imgPath = mfile.getParent() / img->getUri();
            ux3d::filesystem::File imgFile(imgPath);
            core::Vector<uint8_t> headerBytes;
            if (core::Succeeded(imgFile.load(headerBytes)) && headerBytes.size() >= 26) {
                return parseImageHeader(headerBytes.data(), headerBytes.size());
            }
        }
    }
    return {};
}

static void ensureImageDataDecoded(const ux3d::gltf::Image* img)
{
    if (!img) return;
    auto* mutableImg = const_cast<ux3d::gltf::Image*>(img);

    // If pixels are already loaded, we are good
    if (!mutableImg->getData().getPixels().empty() && mutableImg->getData().getWidth() > 0) {
        return;
    }

    // Try decoding from encoded bytes if available
    if (img->getEncodedBytes() != nullptr && !img->getEncodedBytes()->empty()) {
        ux3d::imageformats::decodeImage(*img->getEncodedBytes(), mutableImg->m_data);
        if (!mutableImg->getData().getPixels().empty() && mutableImg->getData().getWidth() > 0) {
            return;
        }
    }

    // Fallback: try loading from disk / VFS via base model directory
    std::string modelPath = AppSettings::instance().getCurrentModelPath();
    if (!modelPath.empty()) {
        ux3d::filesystem::File file = StdFileSystem::instance().toFile(modelPath.c_str());
        ux3d::filesystem::Dir gltfDir(file.getParent());
        mutableImg->loadDataToMemory(gltfDir);
    } else {
        mutableImg->loadDataToMemory(std::nullopt);
    }
}

static GLuint getOrCreateImagePreviewTexture(const ux3d::gltf::Image* img, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (!img) return 0;

    // 1. Check local cache
    auto cacheIt = s_previewCache.find(img);
    if (cacheIt != s_previewCache.end() && cacheIt->second.textureId != 0) {
        if (cacheIt->second.encodedMapID == img->encodedMapID) {
            return cacheIt->second.textureId;
        }
        // Stale texture, destroy it
        glDeleteTextures(1, &cacheIt->second.textureId);
        s_previewCache.erase(cacheIt);
    }

    // 2. Check if gltfLayer already has an uploaded OpenGL texture for this image
    if (gltfLayer != nullptr) {
        // Check sRGB first
        auto it = gltfLayer->m_textureMapping.map.find({ img, false });
        if (it != gltfLayer->m_textureMapping.map.end() && it->second != nullptr) {
            if (it->second->currentMapID == img->encodedMapID &&
                it->second->handle != ux3d::renderer::gl::invalid && it->second->handle != 0) {
                return it->second->handle;
            }
        }
        // Check linear
        it = gltfLayer->m_textureMapping.map.find({ img, true });
        if (it != gltfLayer->m_textureMapping.map.end() && it->second != nullptr) {
            if (it->second->currentMapID == img->encodedMapID &&
                it->second->handle != ux3d::renderer::gl::invalid && it->second->handle != 0) {
                return it->second->handle;
            }
        }
    }

    // 3. Create a dedicated preview OpenGL texture
    ensureImageDataDecoded(img);
    const auto& data = img->getData();
    if (data.getPixels().empty() || data.getWidth() == 0 || data.getHeight() == 0) {
        return 0;
    }

    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId == 0) return 0;

    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum format = GL_RGBA;
    GLint internalFormat = GL_RGBA8;
    uint32_t channels = data.getChannelCount();
    if (channels == 1) {
        format = GL_RED;
        internalFormat = GL_R8;
        GLint swizzleMask[4] = { GL_RED, GL_RED, GL_RED, GL_ONE };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
    } else if (channels == 2) {
        format = GL_RG;
        internalFormat = GL_RG8;
        GLint swizzleMask[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
    } else if (channels == 3) {
        format = GL_RGB;
        internalFormat = GL_RGB8;
    } else if (channels == 4) {
        format = GL_RGBA;
        internalFormat = GL_RGBA8;
    }

    GLenum type = GL_UNSIGNED_BYTE;
    if (data.getFormat() == ux3d::image::Format::FLOAT) {
        type = GL_FLOAT;
        if (channels == 1) internalFormat = GL_R32F;
        else if (channels == 2) internalFormat = GL_RG32F;
        else if (channels == 3) internalFormat = GL_RGB32F;
        else if (channels == 4) internalFormat = GL_RGBA32F;
    } else if (data.getChannelSize(ux3d::image::Channel::RED) == 16) {
        type = GL_UNSIGNED_SHORT;
        if (channels == 1) internalFormat = GL_R16;
        else if (channels == 2) internalFormat = GL_RG16;
        else if (channels == 3) internalFormat = GL_RGB16;
        else if (channels == 4) internalFormat = GL_RGBA16;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 static_cast<GLsizei>(data.getWidth()),
                 static_cast<GLsizei>(data.getHeight()),
                 0, format, type, data.getPixels().data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    ImagePreviewEntry entry;
    entry.textureId = texId;
    entry.encodedMapID = img->encodedMapID;
    s_previewCache[img] = entry;

    return texId;
}

static void renderImagePreview(const ux3d::gltf::Image* img, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (!img) return;

    ensureImageDataDecoded(img);

    GLuint texId = getOrCreateImagePreviewTexture(img, gltfLayer);

    const auto& data = img->getData();
    uint32_t imgW = data.getWidth();
    uint32_t imgH = data.getHeight();
    auto res = img->getResolution();
    if ((imgW == 0 || imgH == 0) && res.size() >= 2) {
        imgW = res[0];
        imgH = res[1];
    }

    if (imgW == 0 || imgH == 0) {
        ImGui::Spacing();
        ImGui::TextDisabled("Preview unavailable (0x0 resolution)");
        return;
    }

    float availWidth = ImGui::GetContentRegionAvail().x;
    if (availWidth < 20.0f) {
        return;
    }

    float aspect = static_cast<float>(imgW) / static_cast<float>(imgH);
    if (aspect <= 0.001f) {
        aspect = 1.0f;
    }

    float previewW = availWidth;
    float previewH = previewW / aspect;

    // Safety clamp in case of extreme height
    if (previewH > 2500.0f) {
        previewH = 2500.0f;
    }
    if (previewH < 16.0f) {
        previewH = 16.0f;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Image Preview (%u x %u):", imgW, imgH);
    ImGui::Spacing();

    if (texId != 0) {
        ImGui::Image(static_cast<ImTextureID>(texId),
                     ImVec2(previewW, previewH),
                     ImVec2(0.0f, 0.0f),
                     ImVec2(1.0f, 1.0f));

        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(rMin, rMax, IM_COL32(70, 70, 78, 255));

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", getImageDisplayName(img).c_str());
            ImGui::Separator();
            ImGui::Text("Dimensions: %u x %u px", imgW, imgH);
            ImGui::Text("Aspect Ratio: %.2f:1", aspect);
            auto hInfo = getImageHeaderDetails(img);
            uint32_t chs = (hInfo.hasHeader && hInfo.channels > 0) ? hInfo.channels : data.getChannelCount();
            uint32_t bpc = (hInfo.hasHeader && hInfo.bitsPerChannel > 0) ? hInfo.bitsPerChannel : data.getChannelSize(ux3d::image::Channel::RED);
            uint32_t totBits = (hInfo.hasHeader && hInfo.totalBitDepth > 0) ? hInfo.totalBitDepth : (bpc * (chs > 0 ? chs : 1));
            if (chs > 0) {
                ImGui::Text("Channels: %u (%s)", chs, (chs == 1) ? "Grayscale" : (chs == 2) ? "RG" : (chs == 3) ? "RGB" : (chs == 4) ? "RGBA" : "");
            }
            if (bpc > 0) {
                ImGui::Text("Bit Depth: %u-bit per channel (%u-bit total)", bpc, totBits);
            }
            ImGui::EndTooltip();
        }
    } else {
        ImGui::TextDisabled("Preview texture unavailable");
    }
}

void ElementInspector::render(SelectionState& selection,
                              ux3d::gltf::Gltf& gltfModel,
                              bool& isSceneDirty,
                              AnimationPlayer* animationPlayer,
                              ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (selection.type == SelectionType::None) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an element from the Hierarchy or Resources panel.");
        return;
    }

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
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            node->setName(nameBuf);
            isSceneDirty = true;
        }

        ImGui::Separator();

        // Transform (TRS)
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushFont(getMonoFont());
            // Translation
            auto t = node->getTranslationOrDefault();
            float trans[3] = { static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z) };
            if (ImGui::DragFloat3("Translation", trans, 0.05f)) {
                node->setTranslation(ux3d::math::dvec3(trans[0], trans[1], trans[2]));
                isSceneDirty = true;
            }

            // Rotation (Euler angles in degrees)
            auto r = node->getRotationOrDefault();
            glm::quat q(static_cast<float>(r.w), static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z));
            glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
            float rot[3] = { euler.x, euler.y, euler.z };
            if (ImGui::DragFloat3("Rotation (deg)", rot, 0.5f)) {
                glm::quat newQ = glm::quat(glm::radians(glm::vec3(rot[0], rot[1], rot[2])));
                node->setRotation(ux3d::math::dquat(newQ.w, newQ.x, newQ.y, newQ.z));
                isSceneDirty = true;
            }

            // Scale
            auto s = node->getScaleOrDefault();
            float scale[3] = { static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z) };
            if (ImGui::DragFloat3("Scale", scale, 0.02f, 0.001f, 100.0f)) {
                node->setScale(ux3d::math::dvec3(scale[0], scale[1], scale[2]));
                isSceneDirty = true;
            }
            ImGui::PopFont();

            if (ImGui::Button("Reset Transform")) {
                node->setTranslation(ux3d::math::dvec3(0.0));
                node->setRotation(ux3d::math::dquat(1.0, 0.0, 0.0, 0.0));
                node->setScale(ux3d::math::dvec3(1.0));
                isSceneDirty = true;
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

        // Node Vendor Extensions (e.g. ASOBO_gizmo_object, ASOBO_collision_object, ASOBO_macro_light)
        if (!selection.selectedNode->getUnknownExtensions().empty()) {
            std::string nodeExtHeader = "Node Extensions (MSFS) (" + std::to_string(selection.selectedNode->getUnknownExtensions().size()) + ")";
            if (ImGui::CollapsingHeader(nodeExtHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ExtensionInspector::instance().renderExtensions(selection.selectedNode, gltfModel, isSceneDirty);
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

        MaterialInspector::render(selection.selectedMaterial, gltfModel, isSceneDirty);
    }

    // -----------------------------------------------------------------
    // Mesh Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
        auto* mesh = selection.selectedMesh;
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
                ExtensionInspector::instance().renderExtensions(mesh, gltfModel, isSceneDirty);
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
                            isSceneDirty = true;
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
                        ExtensionInspector::instance().renderExtensions(&prim, gltfModel, isSceneDirty);
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
            isSceneDirty = true;
        }

        if (isPerspective && cam->getCameraPerspective().has_value()) {
            auto& p = cam->getCameraPerspective().value();
            ImGui::PushFont(getMonoFont());
            float fov = static_cast<float>(glm::degrees(p.getYFov()));
            if (ImGui::SliderFloat("Vertical FoV (deg)", &fov, 10.0f, 120.0f, "%.1f")) {
                p.setYFov(glm::radians(fov));
                isSceneDirty = true;
            }
            float znear = static_cast<float>(p.getZNear());
            if (ImGui::DragFloat("Near Plane", &znear, 0.01f, 0.001f, 100.0f)) {
                p.setZNear(znear);
                isSceneDirty = true;
            }
            float zfar = p.getZFar().has_value() ? static_cast<float>(p.getZFar().value()) : 1000.0f;
            if (ImGui::DragFloat("Far Plane", &zfar, 1.0f, 1.0f, 10000.0f)) {
                p.setZFar(zfar);
                isSceneDirty = true;
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
            isSceneDirty = true;
        }

        ImGui::PushFont(getMonoFont());
        // Intensity
        float intensity = static_cast<float>(light->getIntensityOrDefault());
        if (ImGui::DragFloat("Intensity", &intensity, 0.5f, 0.0f, 100000.0f, "%.1f")) {
            light->setIntensity(intensity);
            isSceneDirty = true;
        }

        // Range
        float range = static_cast<float>(light->getRangeOrDefault());
        if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 1000.0f, "%.1f")) {
            light->setRange(range);
            isSceneDirty = true;
        }
        ImGui::PopFont();
    }

    // -----------------------------------------------------------------
    // Texture / Image Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Texture && selection.selectedTexture != nullptr) {
        auto* tex = selection.selectedTexture;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", getTextureDisplayName(tex).c_str());
        ImGui::Separator();
        ImGui::Text("glTF Texture ID: %u", tex->m_ID);
        if (tex->hasName()) {
            ImGui::Text("glTF Name: %s", tex->getName().c_str());
        }
        if (tex->getSource() != nullptr) {
            auto* src = tex->getSource();
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

            if (ImGui::Button(ICON_MDI_FILE_IMAGE_OUTLINE " Inspect Source Image")) {
                selection.selectImage(tex->getSource());
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
        if (tex->getSource() != nullptr) {
            renderImagePreview(tex->getSource(), gltfLayer);
        }
    } else if (selection.type == SelectionType::Image && selection.selectedImage != nullptr) {
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
        if (byteSize > 0) {
            ImGui::Text("Data Size: %zu bytes", byteSize);
        }

        ensureImageDataDecoded(img);
        const auto& data = img->getData();
        auto headerInfo = getImageHeaderDetails(img);

        uint32_t width = data.getWidth();
        uint32_t height = data.getHeight();
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

        renderImagePreview(img, gltfLayer);
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
                            ExtensionInspector::instance().renderExtensions(&ch, gltfModel, isSceneDirty);
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
                ExtensionInspector::instance().renderExtensions(anim, gltfModel, isSceneDirty);
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
}

} // namespace ux3d::imgui
