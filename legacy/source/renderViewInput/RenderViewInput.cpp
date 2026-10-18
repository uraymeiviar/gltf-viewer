#include "renderViewInput/RenderViewInput.h"
#include "Actions.h"
#include "SelectionManager.h"
#include "enums/InputStateEnum.h"
#include "enums/UserEvents.h"
#include "model/ProjectManager.h"
#include "renderViewInput/CameraActor.h"
#include "renderViewInput/GizmoEvent.h"
#include "renderer/Camera.h"
#include "renderer/editor/GizmoMapping.h"
#include "widgets/GLTFRendererWidget.h"
#include <QGuiApplication>
#include <QMouseEvent>

using namespace ux3d;
using namespace ux3d::editor;

RenderViewInput::RenderViewInput(GLTFRendererWidget* _widget, CameraActor* _cameraActor)
    : QObject()
    , m_widget(_widget)
    , m_cameraActor(_cameraActor)
    , m_nodeActor(*_widget)
    , m_zoom(this,
          InputRenderViewState::Zoom,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::RightButton }, { QEvent::KeyPress, Qt::Key_Alt } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::RightButton } } })
    , m_zoomWheel(this,
          std::vector<InputCombination> { { { QEvent::Wheel, Qt::NoButton } } })
    , m_panning(this,
          InputRenderViewState::Panning,
          std::vector<InputCombination> {
              { { QEvent::MouseButtonPress, Qt::MiddleButton } },
              { { QEvent::MouseButtonPress, Qt::LeftButton }, { QEvent::KeyPress, Qt::Key_Control } } },
          std::vector<InputCombination> {
              { { QEvent::MouseButtonRelease, Qt::MiddleButton }, { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_orbit(this,
          InputRenderViewState::Orbit,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_orbitEdit(this,
          InputRenderViewState::Orbit,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton }, { QEvent::KeyPress, Qt::Key_Alt } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_flight(this,
          InputRenderViewState::Flight,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::RightButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::RightButton } } })
    , m_changeSpeed(this,
          std::vector<InputCombination> { { { QEvent::Wheel, Qt::NoButton } } })
    , m_sprint(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_Shift } } })
    , m_select(this,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } })
    , m_forward(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_W } } })
    , m_backward(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_S } } })
    , m_forwardPerspective(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_Up } } })
    , m_backwardPerspective(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_Down } } })
    , m_left(this,
          std::vector<InputCombination> {
              { { QEvent::KeyPress, Qt::Key_A } },
              { { QEvent::KeyPress, Qt::Key_Left } } })
    , m_right(this,
          std::vector<InputCombination> {
              { { QEvent::KeyPress, Qt::Key_D } },
              { { QEvent::KeyPress, Qt::Key_Right } } })
    , m_up(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_E } } })
    , m_down(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_Q } } })
    , m_upOrtho(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_Up } } })
    , m_downOrtho(this,
          std::vector<InputCombination> { { { QEvent::KeyPress, Qt::Key_Down } } })
    , m_translateX(this,
          InputRenderViewState::TranslateX,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton }, { QEvent::KeyPress, Qt::Key_X } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton }, { QEvent::KeyRelease, Qt::Key_X } } })
    , m_translateY(this,
          InputRenderViewState::TranslateY,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton }, { QEvent::KeyPress, Qt::Key_Y } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton }, { QEvent::KeyRelease, Qt::Key_Y } } })
    , m_translateZ(this,
          InputRenderViewState::TranslateZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton }, { QEvent::KeyPress, Qt::Key_Z } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton }, { QEvent::KeyRelease, Qt::Key_Z } } })
    , m_translateXY(this,
          InputRenderViewState::TranslateXY,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_translateYZ(this,
          InputRenderViewState::TranslateYZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_translateXZ(this,
          InputRenderViewState::TranslateXZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleX(this,
          InputRenderViewState::ScaleX,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleY(this,
          InputRenderViewState::ScaleY,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleZ(this,
          InputRenderViewState::ScaleZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleXY(this,
          InputRenderViewState::ScaleXY,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleYZ(this,
          InputRenderViewState::ScaleYZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleXZ(this,
          InputRenderViewState::ScaleXZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_scaleXYZ(this,
          InputRenderViewState::ScaleXYZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_rotateX(this,
          InputRenderViewState::RotateX,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_rotateY(this,
          InputRenderViewState::RotateY,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
    , m_rotateZ(this,
          InputRenderViewState::RotateZ,
          std::vector<InputCombination> { { { QEvent::MouseButtonPress, Qt::LeftButton } } },
          std::vector<InputCombination> { { { QEvent::MouseButtonRelease, Qt::LeftButton } } })
{
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &RenderViewInput::handleApplicationStateChange);
    connect(SelectionManager::getInstance(), &SelectionManager::selectObjects, this, [this](const std::vector<SelectionInfo>& _selection) {
        if (_selection.empty()) {
            return;
        }
        const SelectionInfo& selection = _selection[0];
        if (selection.type == GltfPropertyType::Nodes) {
            m_nodeActor.setNode(selection.pointer.unsafeGet<gltf::Node*>());
        }
    });
}

bool RenderViewInput::eventFilter(QObject* _object, QEvent* _event)
{
    if (m_renderViewState == RenderViewState::FixedCam || m_renderViewState == RenderViewState::NotInitialized || m_cameraActor == nullptr) {
        m_keysDown.clear();
        m_mouseDown.clear();
        return false;
    }
    // Check if event is relevant
    switch (_event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent* event = static_cast<QMouseEvent*>(_event);
        m_mouseDown[event->button()] = true;
        setupCursorPosition();
        break;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent* event = static_cast<QMouseEvent*>(_event);
        m_mouseDown[event->button()] = false;
        break;
    }
    case QEvent::TabletPress: {
        QTabletEvent* event = static_cast<QTabletEvent*>(_event);
        // check if key on pad
        if (event->pressure() == 0) {
            break;
        }
        m_mouseDown[event->button()] = true;
        break;
    }
    case QEvent::TabletMove: {
        QTabletEvent* event = static_cast<QTabletEvent*>(_event);
        // check if key on pad
        if (event->pressure() == 0) {
            break;
        }
        m_mouseDown[event->buttons()] = true;
        break;
    }
    case QEvent::TabletRelease: {
        QTabletEvent* event = static_cast<QTabletEvent*>(_event);
        setupCursorPosition();
        m_mouseDown[event->button()] = false;
        break;
    }
    case QEvent::Wheel: {
        QWheelEvent* event = static_cast<QWheelEvent*>(_event);
        const float wheelDelta = static_cast<float>(event->angleDelta().y());
        if (m_pState == &m_flight) {
            const float increment = 0.07f * math::sign(wheelDelta);
            m_cameraActor->changeCamSpeed(increment);
        } else {
            m_cameraActor->zoomCamera(wheelDelta);
        }
        return false;
    }
    case QEvent::KeyPress: {
        QKeyEvent* event = static_cast<QKeyEvent*>(_event);
        m_keysDown[event->key()] = true;
        break;
    }
    case QEvent::KeyRelease: {
        QKeyEvent* event = static_cast<QKeyEvent*>(_event);
        m_keysDown[event->key()] = false;
        break;
    }
    case QEvent::FocusOut: {
        m_keysDown.clear();
        m_mouseDown.clear();
        QGuiApplication::restoreOverrideCursor();
        return false;
    }
    case QEvent::FocusIn: {
        auto keyModifiers = QGuiApplication::queryKeyboardModifiers();
        m_keysDown[Qt::Key_Shift] = (keyModifiers & Qt::ShiftModifier);
        m_keysDown[Qt::Key_Control] = (keyModifiers & Qt::ControlModifier);
        m_keysDown[Qt::Key_Alt] = (keyModifiers & Qt::AltModifier);
        break;
    }
    case static_cast<QEvent::Type>(UserEvent::Gizmo): {
        break;
    }
    default: {
        return false;
    }
    }

    // Disable State
    if (m_pState != nullptr && m_pState->checkDeactivate()) {
        resetState();
    }

    // Set State
    if (m_pState == nullptr) {
        if (m_zoom.checkActivate()) {
            m_pState = &m_zoom;
            return false;
        }
        switch (m_renderViewState) {
        case RenderViewState::OrbitCam: {
            if (m_orbit.checkActivate()) {
                m_cameraActor->applyFreeCamOrientationToOrbitCam();
                m_pState = &m_orbit;
            }
            return false; // No selection
        }
        case RenderViewState::PerspectiveCam:
        case RenderViewState::OrthographicCam: {
            if (m_panning.checkActivate()) {
                m_cameraActor->applyFreeCamOrientationToOrbitCam();
                m_pState = &m_panning;
                return false;
            }
            if (m_orbitEdit.checkActivate()) {
                m_cameraActor->applyFreeCamOrientationToOrbitCam();
                m_pState = &m_orbitEdit;
                return false;
            }
            if (m_flight.checkActivate()) {
                m_cameraActor->applyOrbitCamOrientationToFreeCam();
                Actions::instance().setToolbarShortcutsEnabled(false);
                m_pState = &m_flight;
                return false;
            }
            if (m_translateX.checkActivate()) {
                m_pState = &m_translateX;
                return false;
            }
            if (_event->type() == static_cast<QEvent::Type>(UserEvent::Gizmo)) {
                handleGizmoSelection(static_cast<GizmoEvent*>(_event));
                return false;
            }
            if (m_select.checkActivate()) {
                // Selection
                m_widget->performPickingInNextFrame();
                return false;
            }
            break;
        }
        case RenderViewState::FixedCamEdit: {
            if (_event->type() == static_cast<QEvent::Type>(UserEvent::Gizmo)) {
                handleGizmoSelection(static_cast<GizmoEvent*>(_event));
                return false;
            }
            if (m_select.checkActivate()) {
                // Selection
                m_widget->performPickingInNextFrame();
                return false;
            }
            break;
        }
        default:
            break;
        }
    }

    return false;
}

void RenderViewInput::resetState()
{
    if (m_pState != nullptr && m_cameraActor != nullptr) {
        switch (m_pState->getStateName()) {
        case InputRenderViewState::Orbit: {
            m_cameraActor->applyOrbitCamOrientationToFreeCam();
            break;
        }
        case InputRenderViewState::Flight: {
            Actions::instance().setToolbarShortcutsEnabled(true);
            m_cameraActor->applyFreeCamOrientationToOrbitCam();
            break;
        }
        case InputRenderViewState::Panning: {
            m_cameraActor->applyFreeCamOrientationToOrbitCam();
            break;
        }
        case InputRenderViewState::TranslateX:
        case InputRenderViewState::TranslateY:
        case InputRenderViewState::TranslateZ:
        case InputRenderViewState::TranslateXY:
        case InputRenderViewState::TranslateYZ:
        case InputRenderViewState::TranslateXZ: {
            m_widget->setGizmoState({ renderer::GizmoTransformMode::DefaultTranslate, renderer::GizmoInteractionMode::Hover });
            m_nodeActor.saveTranslation();
            break;
        }
        case InputRenderViewState::ScaleX:
        case InputRenderViewState::ScaleY:
        case InputRenderViewState::ScaleZ:
        case InputRenderViewState::ScaleXY:
        case InputRenderViewState::ScaleYZ:
        case InputRenderViewState::ScaleXZ:
        case InputRenderViewState::ScaleXYZ: {
            m_widget->setGizmoState({ renderer::GizmoTransformMode::DefaultScale, renderer::GizmoInteractionMode::Hover });
            m_nodeActor.saveScale();
            break;
        }
        case InputRenderViewState::RotateX:
        case InputRenderViewState::RotateY:
        case InputRenderViewState::RotateZ: {
            m_widget->setGizmoState({ renderer::GizmoTransformMode::DefaultRotate, renderer::GizmoInteractionMode::Hover });
            m_nodeActor.saveRotation();
            break;
        }
        default:
            break;
        }
    }

    m_pState = nullptr;
}

void RenderViewInput::handleGizmoSelection(GizmoEvent* _event)
{
    renderer::GizmoState const& state = _event->getState();

    if (state.interactionMode != renderer::GizmoInteractionMode::Drag) {
        return;
    }

    switch (state.transformMode) {
    case renderer::GizmoTransformMode::TranslateX: {
        m_pState = &m_translateX;
        break;
    }
    case renderer::GizmoTransformMode::TranslateY: {
        m_pState = &m_translateY;
        break;
    }
    case renderer::GizmoTransformMode::TranslateZ: {
        m_pState = &m_translateZ;
        break;
    }
    case renderer::GizmoTransformMode::TranslatePlaneX: {
        m_pState = &m_translateYZ;
        break;
    }
    case renderer::GizmoTransformMode::TranslatePlaneY: {
        m_pState = &m_translateXZ;
        break;
    }
    case renderer::GizmoTransformMode::TranslatePlaneZ: {
        m_pState = &m_translateXY;
        break;
    }
    case renderer::GizmoTransformMode::ScaleX: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleX;
        break;
    }
    case renderer::GizmoTransformMode::ScaleY: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleY;
        break;
    }
    case renderer::GizmoTransformMode::ScaleZ: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleZ;
        break;
    }
    case renderer::GizmoTransformMode::ScalePlaneX: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleYZ;
        break;
    }
    case renderer::GizmoTransformMode::ScalePlaneY: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleXZ;
        break;
    }
    case renderer::GizmoTransformMode::ScalePlaneZ: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleXY;
        break;
    }
    case renderer::GizmoTransformMode::ScaleUniform: {
        m_nodeActor.setIntialScale(m_lastMouseX, m_lastMouseY);
        m_pState = &m_scaleXYZ;
        break;
    }
    case renderer::GizmoTransformMode::RotateX: {
        m_nodeActor.setIntialRotation(m_lastMouseX, m_lastMouseY);
        m_pState = &m_rotateX;
        break;
    }
    case renderer::GizmoTransformMode::RotateY: {
        m_nodeActor.setIntialRotation(m_lastMouseX, m_lastMouseY);
        m_pState = &m_rotateY;
        break;
    }
    case renderer::GizmoTransformMode::RotateZ: {
        m_nodeActor.setIntialRotation(m_lastMouseX, m_lastMouseY);
        m_pState = &m_rotateZ;
        break;
    }
    default:
        break;
    }
}

void RenderViewInput::transformCamera(float _deltaTime)
{
    if (m_pState == nullptr || m_cameraActor == nullptr) {
        return;
    }
    updateMouseDeltaResetCursor();

    switch (m_pState->getStateName()) {
    case InputRenderViewState::Orbit: {
        m_cameraActor->rotateCamera(m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::Flight: {
        math::vec3 velocity = getTranslationInput() * _deltaTime;
        m_cameraActor->handleFreeCamera(m_mouseDeltaX, m_mouseDeltaY, velocity);
        break;
    }
    case InputRenderViewState::Panning: {
        m_cameraActor->panCamera(-m_mouseDeltaX, -m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::Zoom: {
        float length = math::sqrt(math::pow(m_mouseDeltaX, 2) + math::pow(m_mouseDeltaY, 2));
        if (m_mouseDeltaY < 0) {
            length *= -1;
        }
        m_cameraActor->zoomCamera(length);
        break;
    }
    case InputRenderViewState::TranslateX: {
        m_nodeActor.translateX(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::TranslateY: {
        m_nodeActor.translateY(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::TranslateZ: {
        m_nodeActor.translateZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::TranslateXY: {
        m_nodeActor.translateXY(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::TranslateYZ: {
        m_nodeActor.translateYZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::TranslateXZ: {
        m_nodeActor.translateXZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::RotateX: {
        m_nodeActor.rotateX(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::RotateY: {
        m_nodeActor.rotateY(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::RotateZ: {
        m_nodeActor.rotateZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleX: {
        m_nodeActor.scaleX(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleY: {
        m_nodeActor.scaleY(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleZ: {
        m_nodeActor.scaleZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleXY: {
        m_nodeActor.scaleXY(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleYZ: {
        m_nodeActor.scaleYZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleXZ: {
        m_nodeActor.scaleXZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    case InputRenderViewState::ScaleXYZ: {
        m_nodeActor.scaleXYZ(m_lastMouseX, m_lastMouseY, m_mouseDeltaX, m_mouseDeltaY);
        break;
    }
    default:
        break;
    }
}

void RenderViewInput::setRenderViewState(RenderViewState _state)
{
    m_keysDown.clear();
    m_mouseDown.clear();
    m_renderViewState = _state;
    m_pState = nullptr;
}

void RenderViewInput::setInputToolState(InputToolState _state)
{
    if (_state == InputToolState::Selection) {
        m_widget->setGizmoVisibility(false);
    } else {
        renderer::GizmoTransformMode mode = renderer::GizmoTransformMode::DefaultTranslate;
        switch (_state) {
        case InputToolState::Rotation: {
            mode = renderer::GizmoTransformMode::DefaultRotate;
            break;
        }
        case InputToolState::Translation: {
            mode = renderer::GizmoTransformMode::DefaultTranslate;
            break;
        }
        case InputToolState::Scaling: {
            mode = renderer::GizmoTransformMode::DefaultScale;
            break;
        }
        default:
            break;
        }
        m_widget->setGizmoState({ mode, renderer::GizmoInteractionMode::Hover });
        m_widget->setGizmoVisibility(true);
    }
    m_keysDown.clear();
    m_mouseDown.clear();
    m_inputToolState = _state;
    m_pState = nullptr;
}

void RenderViewInput::setupCursorPosition()
{
    auto cursorPos = getCurrentMousePos();
    m_mouseOriginalX = cursorPos.x;
    m_mouseOriginalY = cursorPos.y;
    m_lastMouseX = cursorPos.x;
    m_lastMouseY = cursorPos.y;
}

void RenderViewInput::updateMouseDeltaResetCursor()
{
    if (m_useTabletMode) {
        return;
    }

    QPoint cursorPos = QCursor::pos();
    cursorPos = m_widget->mapFromGlobal(cursorPos);
    m_mouseDeltaX = cursorPos.x() - m_lastMouseX;
    m_mouseDeltaY = cursorPos.y() - m_lastMouseY;
    m_lastMouseX = cursorPos.x();
    m_lastMouseY = cursorPos.y();
}

std::pair<int32_t, int32_t> RenderViewInput::getLastMousePos() const
{
    return std::make_pair(m_lastMouseX, m_lastMouseY);
}

math::ivec2 RenderViewInput::getCurrentMousePos() const
{
    QPoint glob = m_widget->mapToGlobal(QPoint(m_mouseOriginalX, m_mouseOriginalY));
    QScreen* pScreen = QGuiApplication::screenAt(glob);
    QPoint cursorPos = QCursor::pos(pScreen);
    cursorPos = m_widget->mapFromGlobal(cursorPos);
    return { cursorPos.x(), cursorPos.y() };
}

math::vec3 RenderViewInput::getTranslationInput()
{
    const math::vec3 up(0.0, 1.0, 0.0);
    const math::vec3 forward(1.0, 0.0, 0.0);
    const math::vec3 right(0.0, 0.0, 1.0);

    bool isOrthograhic = m_renderViewState == RenderViewState::OrthographicCam;

    math::vec3 velocity(0);
    if (m_forward.checkActivate() || (m_forwardPerspective.checkActivate() && !isOrthograhic)) {
        velocity -= forward;
    }
    if (m_backward.checkActivate() || (m_backwardPerspective.checkActivate() && !isOrthograhic)) {
        velocity += forward;
    }
    if (m_left.checkActivate()) {
        velocity -= right;
    }
    if (m_right.checkActivate()) {
        velocity += right;
    }
    if (m_up.checkActivate() || (m_upOrtho.checkActivate() && isOrthograhic)) {
        velocity += up;
    }
    if (m_down.checkActivate() || (m_downOrtho.checkActivate() && isOrthograhic)) {
        velocity -= up;
    }
    if (m_sprint.checkActivate()) {
        velocity *= 2;
    }
    return velocity;
}

bool RenderViewInput::keyDown(int _keyCode) const
{
    auto it = m_keysDown.find(_keyCode);
    if (it != m_keysDown.end()) {
        // element found;
        return it->second;
    } else {
        return false;
    }
}

bool RenderViewInput::mouseDown(int _button) const
{
    auto it = m_mouseDown.find(_button);
    if (it != m_mouseDown.end()) {
        // element found;
        return it->second;
    } else {
        return false;
    }
}

void RenderViewInput::handleApplicationStateChange(Qt::ApplicationState _state)
{
    if (_state == Qt::ApplicationHidden || _state == Qt::ApplicationInactive) {
        resetState();
        QGuiApplication::restoreOverrideCursor();
        m_keysDown.clear();
        m_mouseDown.clear();
    }
}

void RenderViewInput::setLastTabletPos(QPoint _globalTabletPos)
{
    m_lastTabletInputPosX = _globalTabletPos.x();
    m_lastTabletInputPosY = _globalTabletPos.y();
}

void RenderViewInput::setMouseDelta(QPoint _globalTabletPos)
{
    QPoint last(m_lastTabletInputPosX, m_lastTabletInputPosY);
    QPoint diff = (last - _globalTabletPos) * 2;
    setLastTabletPos(_globalTabletPos);
    m_mouseDeltaX = diff.x();
    m_mouseDeltaY = diff.y();
}
