// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Flag.h"
#include "core/String.h"
#include "core/Vector.h"
#include <cstdint>

namespace ux3d::gltf {
enum class Extension : uint32_t {
    None = 0,
    PunctualLights = 1u << 0u,
    PbrSpecularGlossinessMaterial = 1u << 1u,
    UnlitMaterial = 1u << 2u,
    TextureBasisU = 1u << 3u,
    TextureTransform = 1u << 4u,
    Clearcoat = 1u << 5u,
    Specular = 1u << 6u,
    DracoMeshCompression = 1u << 7u,
    MeshQuantization = 1u << 8u,
    Sheen = 1u << 9u,
    Ior = 1u << 10u,
    Transmission = 1u << 11u,
    MaterialsVariants = 1u << 12u,
    XMP = 1u << 13u,
    Volume = 1u << 14u,
    Iridescence = 1u << 15u,
    EmissiveStrength = 1u << 16u,
    Audio = 1u << 17u,
    AnimationPointer = 1u << 18u,
    WebP = 1u << 19u,
    TextureDDS = 1u << 20u,
};

using Extensions = core::Flag<Extension>;

core::Vector<Extension> getAllExtensions();
core::Vector<core::string> getAllExtensionNames();
core::string extensionToString(Extension _extension);
Extension stringToExtension(core::string& _extensionStr);
bool extensionStringKnown(core::string& _extensionStr);
bool isExtensionRequired(Extension _extension);

}
