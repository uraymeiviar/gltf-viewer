#include "gltf/AnimationType.h"

using namespace ux3d;
using namespace ux3d::gltf;

std::string gltf::AnimationTypeToString(AnimationType _type)
{
    switch (_type) {
    case AnimationType::Translation:
        return "Translation";
    case AnimationType::Rotation:
        return "Rotation";
    default:
        return "";
    }
}

AnimationType gltf::stringToAnimationType(const std::string& _text)
{
    if (_text == "Translation") {
        return AnimationType::Translation;
    } else if (_text == "Rotation") {
        return AnimationType::Rotation;
    }

    return AnimationType::Translation;
}
