#pragma once

#include <optional>
#include <string>
#include <unordered_set>

namespace ux3d::editor {

struct GltfOpenUseCase {
    std::string path;
};

struct GltfCreate {
    bool createNewScene = false;
};
}
