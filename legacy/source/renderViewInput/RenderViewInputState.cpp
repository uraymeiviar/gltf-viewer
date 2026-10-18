#include "renderViewInput/RenderViewInputState.h"
#include "renderViewInput/RenderViewInput.h"
#include <QCursor>
#include <QGuiApplication>

using namespace ux3d::editor;

RenderViewBinding::RenderViewBinding(RenderViewInput* _input,
    std::vector<InputCombination>&& _activation)
    : m_renderViewInput(_input)
    , m_activation(std::move(_activation))
{
}

RenderViewInputState::RenderViewInputState(RenderViewInput* _input, InputRenderViewState _state,
    std::vector<InputCombination>&& _activation,
    std::vector<InputCombination>&& _deactivation)
    : RenderViewBinding(_input, std::move(_activation))
    , m_state(_state)
    , m_deactivation(std::move(_deactivation))
{
}

bool RenderViewBinding::check(const std::vector<InputCombination>& _bindings) const
{
    for (const auto& combination : _bindings) {
        if (check(combination)) {
            return true;
        }
    }

    return false;
}

bool RenderViewBinding::check(const InputCombination& _combination) const
{
    for (const auto& pair : _combination) {
        switch (pair.first) {
        case QEvent::Type::MouseButtonPress: {
            if (false == m_renderViewInput->mouseDown(pair.second)) {
                return false;
            }
            break;
        }
        case QEvent::Type::MouseButtonRelease: {
            if (m_renderViewInput->mouseDown(pair.second)) {
                return false;
            }
            break;
        }
        case QEvent::Type::KeyPress: {
            if (false == m_renderViewInput->keyDown(pair.second)) {
                return false;
            }
            break;
        }
        case QEvent::Type::KeyRelease: {
            if (m_renderViewInput->keyDown(pair.second)) {
                return false;
            }
            break;
        }
        default: {
            return false;
        }
        }
    }

    return true;
}

bool RenderViewBinding::checkActivate() const
{
    return check(m_activation);
}

bool RenderViewInputState::checkActivate() const
{
    return check(m_activation);
}

bool RenderViewInputState::checkDeactivate() const
{
    return check(m_deactivation);
}

InputRenderViewState RenderViewInputState::getStateName() const
{
    return m_state;
}
