// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Element Inspector image support: the preview texture cache, image header parsing (PNG / JPEG / KTX2 / DDS
// details), normal-map detection, on-demand decoding and the image preview widget.

#include "ElementInspectorImages.h"

namespace ux3d::imgui {

struct ImagePreviewEntry {
    GLuint textureId = 0;
    int encodedMapID = -1;
    bool signedSquare = false;
};

// One image can be previewed both as a regular texture and as a normal map
// (signed-square decode). Keep one entry per interpretation so material slots
// showing the same image in different roles do not recreate it every frame.
struct ImagePreviewCacheEntry {
    ImagePreviewEntry regular;
    ImagePreviewEntry signedSquare;

    ImagePreviewEntry& get(bool signedSquareNormal)
    {
        return signedSquareNormal ? signedSquare : regular;
    }
};

static std::unordered_map<const ux3d::gltf::Image*, ImagePreviewCacheEntry> s_previewCache;

void ElementInspector::clearPreviewCache()
{
    for (auto& pair : s_previewCache) {
        if (pair.second.regular.textureId != 0) {
            glDeleteTextures(1, &pair.second.regular.textureId);
        }
        if (pair.second.signedSquare.textureId != 0) {
            glDeleteTextures(1, &pair.second.signedSquare.textureId);
        }
    }
    s_previewCache.clear();
}

static ImageHeaderDetails makeKtx2HeaderDetails(const ux3d::imageformats::KTX2ImageInfo& info, size_t fileSize)
{
    ImageHeaderDetails d;
    d.hasHeader = true;
    d.format = "KTX2";
    d.width = info.width;
    d.height = info.height;
    d.isKtx2 = true;
    d.vkFormat = static_cast<uint32_t>(info.format);
    d.ktx2LevelCount = info.levelCount;
    d.fileSize = fileSize;
    d.vkFormatName = ux3d::imageformats::getKTX2FormatName(info.format);
    return d;
}

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

ImageHeaderDetails getImageHeaderDetails(const ux3d::gltf::Image* img)
{
    if (!img || img->payloadOmitted()) return {};
    if (img->getEncodedBytes() != nullptr && img->getEncodedBytes()->size() >= 26) {
        ux3d::imageformats::KTX2ImageInfo ktx2Info;
        if (ux3d::imageformats::getKTX2ImageInfo(*img->getEncodedBytes(), ktx2Info)) {
            return makeKtx2HeaderDetails(ktx2Info, img->getEncodedBytes()->size());
        }
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
                ux3d::imageformats::KTX2ImageInfo ktx2Info;
                if (ux3d::imageformats::getKTX2ImageInfo(headerBytes, ktx2Info)) {
                    return makeKtx2HeaderDetails(ktx2Info, headerBytes.size());
                }
                return parseImageHeader(headerBytes.data(), headerBytes.size());
            }
        }
    }
    return {};
}

// A1.12: a declared file's normal maps use the packaged signed-square storage convention.
// The Inspector shows the decoded normal map only for images that actually serve as one;
// an unused file named *_NORM is still treated as a normal map so its preview is useful.
static bool isNormalMapTexture(const ux3d::gltf::Gltf& gltfModel, const ux3d::gltf::Texture* texture)
{
    if (texture == nullptr) return false;
    for (const auto& material : gltfModel.getMaterials()) {
        const auto& normal = material.getNormalTextureInfo();
        if (normal.has_value() && normal->getTexture() == texture) {
            return true;
        }
        const auto& clearcoat = material.getClearcoat();
        if (clearcoat.has_value() && clearcoat->getClearcoatNormalTexture().has_value()
            && clearcoat->getClearcoatNormalTexture()->getTexture() == texture) {
            return true;
        }
    }
    return false;
}

static bool looksLikeNormalMapName(const ux3d::gltf::Image* img)
{
    if (img == nullptr) return false;
    std::string name = img->hasName() ? std::string(img->getName().c_str()) : std::string();
    name += " " + std::string(img->getUri().c_str());
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return name.find("_NORM") != std::string::npos || name.find("NORMAL") != std::string::npos;
}

bool isPackagedNormalMap(const ux3d::gltf::Gltf& gltfModel, const ux3d::gltf::Image* img, bool ddsDeclared)
{
    for (const auto& texture : gltfModel.getTextures()) {
        if (ux3d::renderer::GltfRenderLayer::pickImageForTexture(&texture, ddsDeclared) != img) {
            continue;
        }
        if (isNormalMapTexture(gltfModel, &texture)) {
            return true;
        }
    }
    return looksLikeNormalMapName(img);
}

void ensureImageDataDecoded(const ux3d::gltf::Image* img)
{
    if (!img || img->payloadOmitted()) return;
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

static GLuint createPreviewTextureFromRgba(uint32_t width, uint32_t height, const core::Vector<uint8_t>& rgba)
{
    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId == 0) {
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texId;
}

static GLuint getOrCreateImagePreviewTexture(const ux3d::gltf::Image* img,
                                             ux3d::renderer::GltfRenderLayer* gltfLayer,
                                             bool signedSquareNormal)
{
    if (!img || img->payloadOmitted()) return 0;

    // 1. Check local cache
    auto cacheIt = s_previewCache.find(img);
    if (cacheIt != s_previewCache.end()) {
        ImagePreviewEntry& cached = cacheIt->second.get(signedSquareNormal);
        if (cached.textureId != 0 && cached.encodedMapID == img->encodedMapID) {
            return cached.textureId;
        }
        if (cached.textureId != 0) {
            // Stale texture, destroy it
            glDeleteTextures(1, &cached.textureId);
            cached = ImagePreviewEntry();
        }
    }

    // 2. Block-compressed KTX2 cannot be shown by uploading its bytes as pixels. Decode a
    // small mip level on the CPU. BC5 SNORM is previewed as a normal map (reconstructed Z);
    // a declared file's normal map is signed-square decoded first.
    const core::Vector<uint8_t>* encoded = img->getEncodedBytes();
    ux3d::imageformats::KTX2ImageInfo ktx2Info;
    const bool blockCompressedKtx2 = encoded != nullptr
        && ux3d::imageformats::getKTX2ImageInfo(*encoded, ktx2Info)
        && ktx2Info.blockCompressed;
    if (blockCompressedKtx2) {
        core::Vector<uint8_t> previewRgba;
        uint32_t previewWidth = 0;
        uint32_t previewHeight = 0;
        if (ux3d::imageformats::decodeKTX2Preview(*encoded, 1024u, previewRgba, previewWidth, previewHeight,
                                                  signedSquareNormal)) {
            GLuint texId = createPreviewTextureFromRgba(previewWidth, previewHeight, previewRgba);
            if (texId != 0) {
                ImagePreviewEntry entry;
                entry.textureId = texId;
                entry.encodedMapID = img->encodedMapID;
                entry.signedSquare = signedSquareNormal;
                s_previewCache[img].get(signedSquareNormal) = entry;
                return texId;
            }
        }
        // If the blocks have no CPU preview, still prefer an already-uploaded GPU texture.
    }

    // 3. Check if gltfLayer already has an uploaded OpenGL texture for this image
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

    // A compressed KTX2 image that neither decoded to a preview nor has a GPU upload has no
    // pixel layout that glTexImage2D could upload.
    if (blockCompressedKtx2) {
        return 0;
    }

    // 4. Create a dedicated preview OpenGL texture
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
    entry.signedSquare = signedSquareNormal;
    s_previewCache[img].get(signedSquareNormal) = entry;

    return texId;
}

unsigned int ElementInspector::getImagePreviewTextureId(const ux3d::gltf::Image* image,
                                                        ux3d::renderer::GltfRenderLayer* gltfLayer,
                                                        bool signedSquareNormal)
{
    return getOrCreateImagePreviewTexture(image, gltfLayer, signedSquareNormal);
}

void renderImagePreview(const ux3d::gltf::Image* img, ux3d::renderer::GltfRenderLayer* gltfLayer,
                        bool signedSquareNormal)
{
    if (!img) return;
    if (img->payloadOmitted()) {
        ImGui::TextDisabled("Image intentionally omitted (opened without textures)");
        return;
    }

    ensureImageDataDecoded(img);

    GLuint texId = getOrCreateImagePreviewTexture(img, gltfLayer, signedSquareNormal);

    const auto& data = img->getData();
    auto headerInfo = getImageHeaderDetails(img);
    uint32_t imgW = data.getWidth();
    uint32_t imgH = data.getHeight();
    if (headerInfo.isKtx2 && headerInfo.width > 0 && headerInfo.height > 0) {
        imgW = headerInfo.width;
        imgH = headerInfo.height;
    }
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
    if (signedSquareNormal && headerInfo.isKtx2
        && headerInfo.vkFormat == static_cast<uint32_t>(ux3d::gpu::Format::BC5_SNORM_BLOCK)) {
        ImGui::TextDisabled("decoded: signed square");
    }
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
            if (headerInfo.isKtx2) {
                ImGui::Text("KTX2: %s (vkFormat %u), %u mips, %llu bytes",
                            headerInfo.vkFormatName.c_str(), headerInfo.vkFormat,
                            headerInfo.ktx2LevelCount, static_cast<unsigned long long>(headerInfo.fileSize));
            } else {
                uint32_t chs = (headerInfo.hasHeader && headerInfo.channels > 0) ? headerInfo.channels : data.getChannelCount();
                uint32_t bpc = (headerInfo.hasHeader && headerInfo.bitsPerChannel > 0) ? headerInfo.bitsPerChannel : data.getChannelSize(ux3d::image::Channel::RED);
                uint32_t totBits = (headerInfo.hasHeader && headerInfo.totalBitDepth > 0) ? headerInfo.totalBitDepth : (bpc * (chs > 0 ? chs : 1));
                if (chs > 0) {
                    ImGui::Text("Channels: %u (%s)", chs, (chs == 1) ? "Grayscale" : (chs == 2) ? "RG" : (chs == 3) ? "RGB" : (chs == 4) ? "RGBA" : "");
                }
                if (bpc > 0) {
                    ImGui::Text("Bit Depth: %u-bit per channel (%u-bit total)", bpc, totBits);
                }
            }
            ImGui::EndTooltip();
        }
    } else {
        ImGui::TextDisabled("Preview texture unavailable");
    }
}

} // namespace ux3d::imgui
