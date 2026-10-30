// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Private to ElementInspector*.cpp: the image header details and the image helpers ElementInspector::render
// calls (ElementInspectorImages.cpp).

#pragma once

#include <glad/glad.h>
#include "imgui/ElementInspector.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/MaterialInspector.h"
#include "imgui/AnimationPlayer.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/TextureUtils.h"
#include "imgui/ResourceMemory.h"
#include "imgui/RuntimeTelemetry.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include "imgui/AppSettings.h"
#include "imgui/StdFileSystem.h"
#include "imgui/UiProbe.h"
#include "imageformats/imageformats.h"
#include "imageformats/ktx2.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/gl/Texture.h"
#include "gltf/GlTypes.h"
#include "gltf/AttributeType.h"
#include "gltf/extensions/AsoboExtensions.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>

namespace ux3d::imgui {

struct ImageHeaderDetails {
    bool hasHeader = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t bitsPerChannel = 0;
    uint32_t totalBitDepth = 0;
    std::string format;
    bool isKtx2 = false;
    uint32_t vkFormat = 0;
    uint32_t ktx2LevelCount = 0;
    uint64_t fileSize = 0;
    std::string vkFormatName;
};

ImageHeaderDetails getImageHeaderDetails(const ux3d::gltf::Image* img);

bool isPackagedNormalMap(const ux3d::gltf::Gltf& gltfModel, const ux3d::gltf::Image* img, bool ddsDeclared);

void ensureImageDataDecoded(const ux3d::gltf::Image* img);

void renderImagePreview(const ux3d::gltf::Image* img, ux3d::renderer::GltfRenderLayer* gltfLayer,
                        bool signedSquareNormal);

} // namespace ux3d::imgui
