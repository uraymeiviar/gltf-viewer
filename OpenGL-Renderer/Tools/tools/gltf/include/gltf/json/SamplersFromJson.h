#pragma once

#include "ChildOfRootFromJson.h"

namespace ux3d::gltf {
class SamplersFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonSampler) override;
};

}
