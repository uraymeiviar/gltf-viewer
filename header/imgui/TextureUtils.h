// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Texture.h"
#include "gltf/Image.h"
#include "gltf/Sampler.h"
#include <string>

namespace ux3d::imgui {

/**
 * Extracts a clean, human-readable file name from an image URI (e.g. "textures/albedo.png" -> "albedo.png").
 */
inline std::string getFilenameFromUri(const std::string& uriStr) {
    if (uriStr.empty() || uriStr.rfind("data:", 0) == 0) {
        return "";
    }
    size_t lastSlash = uriStr.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? uriStr.substr(lastSlash + 1) : uriStr;
    size_t qmark = filename.find_first_of("?#");
    if (qmark != std::string::npos) {
        filename = filename.substr(0, qmark);
    }
    return filename;
}

/**
 * Returns a primary display name for a glTF Image, prioritizing file name / URI, then name, then ID.
 * e.g. "fuselage_albedo.png [Image 0]" or "CockpitDiffuse [Image 1]"
 */
inline std::string getImageDisplayName(const ux3d::gltf::Image* img) {
    if (!img) {
        return "(None)";
    }

    std::string filename;
    bool isDataUri = false;
    if (!img->getUri().empty()) {
        std::string uriStr = img->getUri().c_str();
        if (uriStr.rfind("data:", 0) == 0) {
            isDataUri = true;
        } else {
            filename = getFilenameFromUri(uriStr);
        }
    }

    if (!filename.empty()) {
        if (img->hasName() && !img->getName().empty() && img->getName().c_str() != filename) {
            return filename + " (" + std::string(img->getName().c_str()) + ") [Image " + std::to_string(img->m_ID) + "]";
        }
        return filename + " [Image " + std::to_string(img->m_ID) + "]";
    }

    if (img->hasName() && !img->getName().empty()) {
        return std::string(img->getName().c_str()) + " [Image " + std::to_string(img->m_ID) + "]";
    }

    if (isDataUri) {
        return "Embedded Data [Image " + std::to_string(img->m_ID) + "]";
    }

    return "Image " + std::to_string(img->m_ID);
}

/**
 * Returns a primary display name for a glTF Texture, prioritizing texture/image file name,
 * with the glTF texture ID as a secondary reference.
 * e.g. "fuselage_albedo.png [Texture 2]" or "CockpitBaseColor [Texture 0]"
 * `source` is the image the renderer picks for this texture (see
 * GltfRenderLayer::pickImageForTexture); pass nullptr to use the core source.
 */
inline std::string getTextureDisplayName(const ux3d::gltf::Texture* tex,
                                         const ux3d::gltf::Image* source) {
    if (!tex) {
        return "(None)";
    }

    // 1. Check if the underlying image has a URI / file name
    std::string filename;
    bool isDataUri = false;
    if (source != nullptr && !source->getUri().empty()) {
        std::string uriStr = source->getUri().c_str();
        if (uriStr.rfind("data:", 0) == 0) {
            isDataUri = true;
        } else {
            filename = getFilenameFromUri(uriStr);
        }
    }

    // 2. Check if the underlying image has an explicit name
    std::string imageName;
    if (source != nullptr && source->hasName() && !source->getName().empty()) {
        imageName = source->getName().c_str();
    }

    // 3. Check if the texture has an explicit name
    std::string texName;
    if (tex->hasName() && !tex->getName().empty()) {
        texName = tex->getName().c_str();
    }

    // Format: Primary Reference + [Texture ID]
    // If filename is present:
    if (!filename.empty()) {
        if (!texName.empty() && texName != filename) {
            return filename + " (" + texName + ") [Texture " + std::to_string(tex->m_ID) + "]";
        }
        if (!imageName.empty() && imageName != filename) {
            return filename + " (" + imageName + ") [Texture " + std::to_string(tex->m_ID) + "]";
        }
        return filename + " [Texture " + std::to_string(tex->m_ID) + "]";
    }

    // If no URI filename, but texture has a name
    if (!texName.empty()) {
        if (!imageName.empty() && imageName != texName) {
            return texName + " (" + imageName + ") [Texture " + std::to_string(tex->m_ID) + "]";
        }
        return texName + " [Texture " + std::to_string(tex->m_ID) + "]";
    }

    // If only image name exists (e.g. embedded GLB bufferView or named image)
    if (!imageName.empty()) {
        return imageName + " [Texture " + std::to_string(tex->m_ID) + "]";
    }

    if (isDataUri) {
        return "Embedded Image [Texture " + std::to_string(tex->m_ID) + "]";
    }

    // If source image exists with bufferView but no name or URI:
    if (source != nullptr) {
        return "Image " + std::to_string(source->m_ID) + " [Texture " + std::to_string(tex->m_ID) + "]";
    }

    // Ultimate fallback
    return "Texture " + std::to_string(tex->m_ID);
}

inline std::string getTextureDisplayName(const ux3d::gltf::Texture* tex) {
    return getTextureDisplayName(tex, tex != nullptr ? tex->getSource() : nullptr);
}

} // namespace ux3d::imgui
