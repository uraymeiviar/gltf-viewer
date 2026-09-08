#pragma once

#include <utility>
#include <vector>

#include <QEvent>

#include "enums/InputStateEnum.h"

namespace ux3d::editor {
using InputCombination = std::vector<std::pair<QEvent::Type, int>>;

class RenderViewInput;

class RenderViewBinding {
public:
    RenderViewBinding(RenderViewInput* _input, std::vector<InputCombination>&& _activation);
    virtual bool checkActivate() const;

protected:
    std::vector<InputCombination> m_activation;
    RenderViewInput* m_renderViewInput;
    bool check(const std::vector<InputCombination>& _bindings) const;
    bool check(const InputCombination& _combination) const;
};

class RenderViewInputState : public RenderViewBinding {
public:
    RenderViewInputState(RenderViewInput* _input, InputRenderViewState _state,
        std::vector<InputCombination>&& _activation,
        std::vector<InputCombination>&& _deactivation);
    bool checkActivate() const override;
    bool checkDeactivate() const;
    InputRenderViewState getStateName() const;
    const InputRenderViewState m_state;

protected:
    std::vector<InputCombination> m_deactivation;
};
}
