// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/TexturesToJson.h"
#include "gltf/IndexLookup.h"
#include "gltf/Texture.h"
#include "gltf/extensions/BasisU.h"
#include "gltf/extensions/WebP.h"
#include "gltf/extensions/TextureDds.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

void TexturesToJson::convert(const Texture& _texture, nlohmann::json& _jsonTexture, const JsonPath& _jsonPath) const
{
    convertSampler(_texture, _jsonTexture);
    convertSource(_texture, _jsonTexture);
    convertBase(_texture, _jsonTexture);

    convertExtensionBasisu(_texture, _jsonTexture);
    convertExtensionWebP(_texture, _jsonTexture);
    convertExtensionTextureDds(_texture, _jsonTexture);
}

void TexturesToJson::convertSampler(const Texture& _texture, nlohmann::json& _jsonTexture) const
{
    if (_texture.getSampler() == nullptr) {
        return;
    }

    _jsonTexture["sampler"] = m_indices[_texture.getSampler()];
}

void TexturesToJson::convertSource(const Texture& _texture, nlohmann::json& _jsonTexture) const
{

    if (_texture.getSource() == nullptr) {
        return;
    }

    _jsonTexture["source"] = m_indices[_texture.getSource()];
}

void TexturesToJson::convertExtensionBasisu(const Texture& _texture, nlohmann::json& _jsonTexture) const
{
    const std::optional<BasisU> basisU = _texture.getBasisU();
    if (!basisU.has_value()) {
        return;
    }

    nlohmann::json jsonBasisU = nlohmann::json::object();
    if (basisU->getSource() != nullptr) {
        jsonBasisU["source"] = m_indices[basisU->getSource()];
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(basisU.value(), jsonBasisU);
    gltfObjectToJson.convertUnknownExtensions(basisU.value(), jsonBasisU);

    _jsonTexture["extensions"]["KHR_texture_basisu"] = jsonBasisU;
}

void TexturesToJson::convertExtensionWebP(const Texture& _texture, nlohmann::json& _jsonTexture) const
{
    const std::optional<WebP> webP = _texture.getWebP();
    if (!webP.has_value()) {
        return;
    }

    nlohmann::json jsonWebP = nlohmann::json::object();
    if (webP->getSource() != nullptr) {
        jsonWebP["source"] = m_indices[webP->getSource()];
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(webP.value(), jsonWebP);
    gltfObjectToJson.convertUnknownExtensions(webP.value(), jsonWebP);

    _jsonTexture["extensions"]["EXT_texture_webp"] = jsonWebP;
}

void TexturesToJson::convertExtensionTextureDds(const Texture& _texture, nlohmann::json& _jsonTexture) const
{
    const std::optional<TextureDds> textureDds = _texture.getTextureDds();
    if (!textureDds.has_value()) {
        return;
    }

    nlohmann::json jsonDds = nlohmann::json::object();
    if (textureDds->getSource() != nullptr) {
        jsonDds["source"] = m_indices[textureDds->getSource()];
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(textureDds.value(), jsonDds);
    gltfObjectToJson.convertUnknownExtensions(textureDds.value(), jsonDds);

    _jsonTexture["extensions"]["MSFT_texture_dds"] = jsonDds;
}
