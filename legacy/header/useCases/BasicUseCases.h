#pragma once

#include <string>

namespace ux3d::editor {
template <typename GenericUseCase>
struct ObjectUseCase {
    GenericUseCase* pGltfPointer = nullptr;
};

template <typename GenericUseCase>
struct GenericSelectUseCase : public ObjectUseCase<GenericUseCase> {
    bool clearInspectorBackStack = false;
};

template <typename GenericUseCase>
struct GenericSetNameUseCase : public ObjectUseCase<GenericUseCase> {
    std::string name = "";
};

}
