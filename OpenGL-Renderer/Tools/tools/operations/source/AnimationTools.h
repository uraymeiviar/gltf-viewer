#pragma once
#include "datastorage/DataStorage.h"
#include <gtc/quaternion.hpp>
#include <limits>
#include <nlohmann/json.hpp>
#include <vector>

namespace ux3d::operations::animation {

nlohmann::json optimizeAnimation(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& dataStorage);
nlohmann::json optimizeSampler(nlohmann::json::json_pointer path, nlohmann::json& state, datastorage::DataStorage& dataStorage);

// this magic number epsilon seems to give good results when comparing keyframe data
static constexpr float epsilon = std::numeric_limits<float>::epsilon();

bool isRotationOver90(glm::quat a, glm::quat b);

// math helper functions, we use fast approximations, because we are not interested in detailed results
float approxAtan(float z);
float approxAtan2(float x, float y);
}
