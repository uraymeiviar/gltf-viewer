// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/SamplersToJson.h"
#include "gltf/Sampler.h"

void ux3d::gltf::SamplersToJson::convert(const Sampler& _sampler, nlohmann::json& _jsonSampler, const JsonPath& _jsonPath) const
{
    convertBase(_sampler, _jsonSampler);
    convertMagFilter(_sampler, _jsonSampler);
    convertMinFilter(_sampler, _jsonSampler);
    convertWrapS(_sampler, _jsonSampler);
    convertWrapT(_sampler, _jsonSampler);
}

void ux3d::gltf::SamplersToJson::convertMagFilter(const Sampler& _sampler, nlohmann::json& _jsonSampler) const
{
    std::optional<TextureFilterType> textureFilterOptional = _sampler.getMagFilter();
    if (textureFilterOptional.has_value()) {
        _jsonSampler["magFilter"] = static_cast<int32_t>(textureFilterOptional.value());
    }
}

void ux3d::gltf::SamplersToJson::convertMinFilter(const Sampler& _sampler, nlohmann::json& _jsonSampler) const
{
    std::optional<TextureFilterType> textureFilterOptional = _sampler.getMinFilter();
    if (textureFilterOptional.has_value()) {
        _jsonSampler["minFilter"] = static_cast<int32_t>(textureFilterOptional.value());
    }
}

void ux3d::gltf::SamplersToJson::convertWrapS(const Sampler& _sampler, nlohmann::json& _jsonSampler) const
{
    if (_sampler.hasWrapS()) {
        _jsonSampler["wrapS"] = static_cast<int32_t>(_sampler.getWrapSOrDefault());
    }
}

void ux3d::gltf::SamplersToJson::convertWrapT(const Sampler& _sampler, nlohmann::json& _jsonSampler) const
{
    if (_sampler.hasWrapT()) {
        _jsonSampler["wrapT"] = static_cast<int32_t>(_sampler.getWrapTOrDefault());
    }
}
