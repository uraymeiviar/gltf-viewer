#pragma once

#include "math/VectorTypes.h"
#include "renderViewInput/NodeActor.h"
#include "renderViewInput/RenderViewInputState.h"

#include <QObject>

#include <unordered_map>

namespace ux3d::renderer {
class Camera;
}

namespace ux3d::gltf {
class Node;
}

namespace ux3d::editor {
class CameraActor;
class GLTFRendererWidget;
class GizmoEvent;

class RenderViewInput : public QObject {
    Q_OBJECT
public:
    RenderViewInput(GLTFRendererWidget* _widget, CameraActor* _cameraActor);
    bool keyDown(int _keyCode) const;
    bool mouseDown(int _button) const;
    void setRenderViewState(RenderViewState _state);
    RenderViewState getRenderViewState() const { return m_renderViewState; }
    void setInputToolState(InputToolState _state);
    InputToolState getInputToolState() const { return m_inputToolState; }
    void transformCamera(float _deltaTime);

    void setCameraActor(CameraActor* _actor) { m_cameraActor = _actor; }
    CameraActor* getCameraActor() { return m_cameraActor; }
    const CameraActor* getCameraActor() const { return m_cameraActor; }

    NodeActor* getNodeActor() { return &m_nodeActor; }
    const NodeActor* getNodeActor() const { return &m_nodeActor; }

    std::pair<int32_t, int32_t> getLastMousePos() const;
    math::ivec2 getCurrentMousePos() const;

    void setUseTableMode(const bool _useTabletMode) { m_useTabletMode = _useTabletMode; }
    void setMouseDelta(QPoint _globalTabletPos);
    void setLastTabletPos(QPoint _globalTabletPos);

private:
    RenderViewState m_renderViewState = RenderViewState::NotInitialized;
    InputToolState m_inputToolState = InputToolState::Selection;
    RenderViewInputState* m_pState = nullptr;

    std::unordered_map<int, bool> m_keysDown;
    std::unordered_map<int, bool> m_mouseDown;

    GLTFRendererWidget* m_widget = nullptr;
    CameraActor* m_cameraActor;
    NodeActor m_nodeActor;

    RenderViewInputState m_zoom;
    RenderViewInputState m_panning;
    RenderViewInputState m_orbit;
    RenderViewInputState m_orbitEdit;
    RenderViewInputState m_flight;

    RenderViewInputState m_translateX;
    RenderViewInputState m_translateY;
    RenderViewInputState m_translateZ;
    RenderViewInputState m_translateXY;
    RenderViewInputState m_translateYZ;
    RenderViewInputState m_translateXZ;
    RenderViewInputState m_scaleX;
    RenderViewInputState m_scaleY;
    RenderViewInputState m_scaleZ;
    RenderViewInputState m_scaleXY;
    RenderViewInputState m_scaleYZ;
    RenderViewInputState m_scaleXZ;
    RenderViewInputState m_scaleXYZ;
    RenderViewInputState m_rotateX;
    RenderViewInputState m_rotateY;
    RenderViewInputState m_rotateZ;

    RenderViewBinding m_zoomWheel;
    RenderViewBinding m_changeSpeed;
    RenderViewBinding m_sprint;
    RenderViewBinding m_select;
    RenderViewBinding m_forward;
    RenderViewBinding m_backward;
    RenderViewBinding m_forwardPerspective;
    RenderViewBinding m_backwardPerspective;
    RenderViewBinding m_left;
    RenderViewBinding m_right;
    RenderViewBinding m_up;
    RenderViewBinding m_down;
    RenderViewBinding m_upOrtho;
    RenderViewBinding m_downOrtho;

    bool eventFilter(QObject* _object, QEvent* _event) override;
    void updateMouseDeltaResetCursor();
    void setupCursorPosition();
    math::vec3 getTranslationInput();
    void handleApplicationStateChange(Qt::ApplicationState _state);
    void resetState();
    void handleGizmoSelection(GizmoEvent* _event);

    int32_t m_lastMouseX = 0;
    int32_t m_lastMouseY = 0;
    int32_t m_mouseDeltaX = 0;
    int32_t m_mouseDeltaY = 0;
    int32_t m_mouseOriginalX = 0;
    int32_t m_mouseOriginalY = 0;
    int32_t m_lastTabletInputPosX = 0;
    int32_t m_lastTabletInputPosY = 0;

    bool m_useTabletMode = false;
};
}
