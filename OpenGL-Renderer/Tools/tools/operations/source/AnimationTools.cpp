#include "AnimationTools.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/component_wise.hpp>
#include <set>
#include <typeinfo>

using namespace ux3d;
using namespace ux3d::operations;

nlohmann::json animation::optimizeAnimation(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& dataStorage)
{
    nlohmann::json result = nlohmann::json::object();
    result["info"]["removed_keyframes"] = 0;
    result["info"]["removed_accessors"] = nlohmann::json::array();
    result["info"]["added_accessors"] = nlohmann::json::array();
    result["info"]["original_accessors"] = nlohmann::json::array();

    nlohmann::json::json_pointer path { params["path"] };
    for (int i = 0; i < state.at(path / "samplers").size(); i++) {
        nlohmann::json samplerResult = animation::optimizeSampler(path / "samplers" / i, state, dataStorage);

        result["info"]["removed_keyframes"] = result["info"]["removed_keyframes"].get<uint32_t>() + samplerResult["info"]["removed_keyframes"].get<uint32_t>();
        result["info"]["removed_accessors"].insert(result["info"]["removed_accessors"].end(), samplerResult["info"]["removed_accessors"].begin(), samplerResult["info"]["removed_accessors"].end());

        result["info"]["added_accessors"].insert(result["info"]["added_accessors"].end(), samplerResult["info"]["added_accessors"].begin(), samplerResult["info"]["added_accessors"].end());
        result["info"]["original_accessors"].insert(result["info"]["original_accessors"].end(), samplerResult["info"]["original_accessors"].begin(), samplerResult["info"]["original_accessors"].end());
    }
    // check if removed keyframes actually reduce overall size
    // in some cases with shared time accessors, overall size can be larger with removed keyframes but with additional time accessor

    auto getByteSize = [](nlohmann::json accessorArray, nlohmann::json& state) -> size_t {
        std::set<std::string> accessors;
        // deduplicate by using a set
        for (std::string path : accessorArray) {
            accessors.insert(path);
        }
        size_t sum = 0;
        for (std::string path : accessors) {
            sum += reactor::utils::getAccessorByteSize(nlohmann::json::json_pointer(path), state);
        }
        return sum;
    };

    auto accessorsBefore = result["info"]["original_accessors"];
    accessorsBefore.insert(accessorsBefore.end(), result["info"]["removed_accessors"].begin(), result["info"]["removed_accessors"].end());

    auto accessorsAfter = result["info"]["original_accessors"];
    accessorsAfter.insert(accessorsAfter.end(), result["info"]["added_accessors"].begin(), result["info"]["added_accessors"].end());

    auto sizeBefore = getByteSize(accessorsBefore, state);
    auto sizeAfter = getByteSize(accessorsAfter, state);
    result["info"]["byte_size_before"] = sizeBefore;
    result["info"]["byte_size_after"] = sizeAfter;

    if (sizeAfter > sizeBefore) {
        throw std::runtime_error("Unable to optimize animation");
    }

    return result;
}

nlohmann::json
animation::optimizeSampler(nlohmann::json::json_pointer path, nlohmann::json& state, datastorage::DataStorage& dataStorage)
{
    nlohmann::json result = nlohmann::json::object();
    result["info"]["removed_accessors"] = nlohmann::json::array();
    result["info"]["added_accessors"] = nlohmann::json::array();
    result["info"]["original_accessors"] = nlohmann::json::array();

    auto derefAccessor = [](std::string path, nlohmann::json& state) -> nlohmann::json::json_pointer {
        auto index = state.at(nlohmann::json::json_pointer(path)).get<uint64_t>();
        return nlohmann::json::json_pointer("/gltf/accessors") / index;
    };

    std::string interpolation = "LINEAR"; // default
    if (state.contains(path / "interpolation")) {
        interpolation = state.at(path / "interpolation").get<std::string>();
    }

    if (interpolation == "CUBICSPLINE") {
        result["warning"] = "Unable to handle CUBICSPLINE interpolation";
        return result;
    }

    auto timeAccessor = derefAccessor(path / "input", state);
    auto dataAccessor = derefAccessor(path / "output", state);

    reactor::gltf::Type dataAttributeType = state.at(dataAccessor / "type").get<reactor::gltf::Type>();
    reactor::gltf::ComponentType dataComponentType = state.at(dataAccessor / "componentType").get<reactor::gltf::ComponentType>();

    // get the keyframe data as floats

    std::vector<float> keyframeTimes = reactor::utils::getAccessorData<float>(timeAccessor, state, dataStorage);

    reactor::utils::AccessorTypeVariant keyframeData = reactor::utils::getAccessorData(dataAccessor, state, dataStorage);

    size_t originalKeyframeCount = keyframeTimes.size();

    // remove redundant keyframes

    auto remove_redundant_keyframe = [](reactor::utils::AccessorTypeVariant& keyframeData, std::vector<float>& keyframeTimes, size_t index, auto func) -> bool {
        if (std::holds_alternative<std::vector<float>>(keyframeData)) {
            auto& typedData = std::get<std::vector<float>>(keyframeData);
            if (func(typedData, keyframeTimes, index)) {
                typedData.erase(typedData.begin() + index);
                keyframeTimes.erase(keyframeTimes.begin() + index);
                return true;
            }
        }
        if (std::holds_alternative<std::vector<glm::vec2>>(keyframeData)) {
            auto& typedData = std::get<std::vector<glm::vec2>>(keyframeData);
            if (func(typedData, keyframeTimes, index)) {
                typedData.erase(typedData.begin() + index);
                keyframeTimes.erase(keyframeTimes.begin() + index);
                return true;
            }
        }
        if (std::holds_alternative<std::vector<glm::vec3>>(keyframeData)) {
            auto& typedData = std::get<std::vector<glm::vec3>>(keyframeData);
            if (func(typedData, keyframeTimes, index)) {
                typedData.erase(typedData.begin() + index);
                keyframeTimes.erase(keyframeTimes.begin() + index);
                return true;
            }
        }
        if (std::holds_alternative<std::vector<glm::vec4>>(keyframeData)) {
            auto& typedData = std::get<std::vector<glm::vec4>>(keyframeData);
            if (func(typedData, keyframeTimes, index)) {
                typedData.erase(typedData.begin() + index);
                keyframeTimes.erase(keyframeTimes.begin() + index);
                return true;
            }
        }
        return false;
    };

    auto step_compare = [](auto& keyframeData, std::vector<float>& keyframeTimes, size_t index) -> bool {
        if (index == 0) {
            return false;
        }
        if (index + 1 >= keyframeData.size()) { // keep last keyframe
            return false;
        }
        auto a = keyframeData[index];
        auto b = keyframeData[index - 1];

        bool isEqual = glm::length(a - b) <= epsilon;

        return isEqual;
    };

    // this is used for all linear interpolations except for rotations, so for translation, scale and weights
    auto linear_compare = [](auto& keyframeData, auto& keyframeTimes, size_t index) {
        if (index == 0) {
            return false;
        }
        if (index + 1 >= keyframeData.size()) {
            return false;
        }

        float previousTime = keyframeTimes[(index - 1)];
        float currentTime = keyframeTimes[index];
        float nextTime = keyframeTimes[(index + 1)];

        float timePreviousToCurrent = currentTime - previousTime;
        float timePreviousToNext = nextTime - previousTime;

        auto previousData = keyframeData[index - 1];
        auto currentData = keyframeData[index];
        auto nextData = keyframeData[index + 1];

        auto linearInterpolationData = glm::mix(previousData, nextData, timePreviousToCurrent / timePreviousToNext);

        bool isEqual = glm::length(linearInterpolationData - currentData) <= epsilon;

        return isEqual;
    };

    // this is only used for linear interpolation of rotations (quaternions) [vec4]
    auto quaternion_linear_compare = [](auto& keyframeData, auto& keyframeTimes, size_t index) {
        if (index == 0) {
            return false;
        }
        if (index + 1 >= keyframeData.size()) {
            return false;
        }

        // constructor glm::quat(...)  expects  (w, x, y, z)
        // we can do this as this function only gets vec4
        glm::vec4 v;
        v = *reinterpret_cast<glm::vec4*>(&keyframeData[index - 1]);
        glm::quat previousData = glm::quat(v.w, v.x, v.y, v.z);
        v = *reinterpret_cast<glm::vec4*>(&keyframeData[index]);
        glm::quat currentData = glm::quat(v.w, v.x, v.y, v.z);
        v = *reinterpret_cast<glm::vec4*>(&keyframeData[index + 1]);
        glm::quat nextData = glm::quat(v.w, v.x, v.y, v.z);

        float previousTime = keyframeTimes[(index - 1)];
        float currentTime = keyframeTimes[index];
        float nextTime = keyframeTimes[(index + 1)];

        float timePreviousToCurrent = currentTime - previousTime;
        float timePreviousToNext = nextTime - previousTime;
        glm::quat linearInterpolationData = glm::slerp(previousData, nextData, timePreviousToCurrent / timePreviousToNext);

        bool isEqual = glm::epsilonEqual<float>(linearInterpolationData.x, currentData.x, epsilon)
            && glm::epsilonEqual<float>(linearInterpolationData.y, currentData.y, epsilon)
            && glm::epsilonEqual<float>(linearInterpolationData.z, currentData.z, epsilon)
            && glm::epsilonEqual<float>(linearInterpolationData.w, currentData.w, epsilon);

        return isEqual && !isRotationOver90(previousData, currentData);
    };

    if (interpolation == "STEP") {
        for (int32_t i = 0; i < keyframeTimes.size(); i += 1) {
            if (remove_redundant_keyframe(keyframeData, keyframeTimes, i, step_compare)) {
                i--;
            }
        }
    }

    if (interpolation == "LINEAR") {
        if (std::holds_alternative<std::vector<glm::vec4>>(keyframeData)) {
            for (int32_t i = 0; i < keyframeTimes.size(); i += 1) {
                if (remove_redundant_keyframe(keyframeData, keyframeTimes, i, quaternion_linear_compare)) {
                    i--;
                }
            }
        } else {
            for (int32_t i = 0; i < keyframeTimes.size(); i += 1) {
                if (remove_redundant_keyframe(keyframeData, keyframeTimes, i, linear_compare)) {
                    i--;
                }
            }
        }
    }

    if (originalKeyframeCount == keyframeTimes.size()) {
        result["info"]["removed_keyframes"] = 0;
        result["info"]["original_accessors"].push_back(derefAccessor(path / "input", state).to_string());
        result["info"]["original_accessors"].push_back(derefAccessor(path / "output", state).to_string());
        return result;
    }
    result["info"]["removed_keyframes"] = originalKeyframeCount - keyframeTimes.size();

    // create new Accessors if the optimization was successful

    result["info"]["removed_accessors"].push_back(derefAccessor(path / "input", state).to_string());
    state.at(path / "input") = std::stoi(reactor::utils::createAccessorFromData(keyframeTimes, state, dataStorage).back());
    result["info"]["added_accessors"].push_back(derefAccessor(path / "input", state).to_string());

    auto createAccessorFromVariant = [](reactor::utils::AccessorTypeVariant dataVariant, nlohmann::json& state, datastorage::DataStorage& dataStorage) -> nlohmann::json::json_pointer {
        if (std::holds_alternative<std::vector<float>>(dataVariant)) {
            auto& typedData = std::get<std::vector<float>>(dataVariant);
            return reactor::utils::createAccessorFromData(typedData, state, dataStorage);
        }
        if (std::holds_alternative<std::vector<glm::vec2>>(dataVariant)) {
            auto& typedData = std::get<std::vector<glm::vec2>>(dataVariant);
            return reactor::utils::createAccessorFromData(typedData, state, dataStorage);
        }
        if (std::holds_alternative<std::vector<glm::vec3>>(dataVariant)) {
            auto& typedData = std::get<std::vector<glm::vec3>>(dataVariant);
            return reactor::utils::createAccessorFromData(typedData, state, dataStorage);
        }
        if (std::holds_alternative<std::vector<glm::vec4>>(dataVariant)) {
            auto& typedData = std::get<std::vector<glm::vec4>>(dataVariant);
            return reactor::utils::createAccessorFromData(typedData, state, dataStorage);
        }
        return nlohmann::json::json_pointer();
    };

    result["info"]["removed_accessors"].push_back(derefAccessor(path / "output", state).to_string());
    state.at(path / "output") = std::stoi(createAccessorFromVariant(keyframeData, state, dataStorage).back());
    result["info"]["added_accessors"].push_back(derefAccessor(path / "output", state).to_string());

    return result;
}

bool animation::isRotationOver90(glm::quat a, glm::quat b)
{
    glm::quat previousData = a;
    glm::quat currentData = b;

    glm::quat mix = glm::conjugate(previousData) * currentData;
    float angle = glm::degrees(2 * approxAtan2(glm::length(glm::vec3(mix.x, mix.y, mix.z)), mix.w));
    return glm::ceil(angle) >= 90.0f;
}

float animation::approxAtan(float z)
{
    const float magic_number1 = 0.97239411f;
    const float magic_number2 = -0.19194795f;
    return (magic_number1 + magic_number2 * z * z) * z;
}

float animation::approxAtan2(float y, float x)
{
    if (x != 0.0f) {
        if (fabsf(x) > fabsf(y)) {
            const float z = y / x;
            if (x > 0.0) {
                // atan2(y, x) = atan(y/x) if x > 0
                return approxAtan(z);
            } else if (y >= 0.0) {
                // atan2(y, x) = atan(y/x) + PI if x < 0, y >= 0
                return approxAtan(z) + glm::pi<float>();
            } else {
                // atan2(y, x) = atan(y/x) - PI if x < 0, y < 0
                return approxAtan(z) - glm::pi<float>();
            }
        } else // Use property atan(y/x) = PI/2 - atan(x/y) if |y/x| > 1.
        {
            const float z = x / y;
            if (y > 0.0) {
                // atan2(y, x) = PI/2 - atan(x/y) if |y/x| > 1, y > 0
                return -approxAtan(z) + glm::half_pi<float>();
            } else {
                // atan2(y, x) = -PI/2 - atan(x/y) if |y/x| > 1, y < 0
                return -approxAtan(z) - glm::half_pi<float>();
            }
        }
    } else {
        if (y > 0.0f) {
            // x = 0, y > 0
            return glm::half_pi<float>();
        } else if (y < 0.0f) {
            // x = 0, y < 0
            return -glm::half_pi<float>();
        }
    }

    // x, y = 0
    return 0.0f;
}
