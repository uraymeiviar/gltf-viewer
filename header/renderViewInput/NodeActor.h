#pragma once

#include <cstdint>
#include <optional>

#include "math/VectorTypes.h"
#include "model/NodeBinding.h"

namespace ux3d::gltf {
class Node;
}

namespace ux3d::editor {
class GLTFRendererWidget;

class NodeActor {
public:
    NodeActor(const GLTFRendererWidget& widget, gltf::Node* node = nullptr);

    void setNode(gltf::Node* node);

    void translateX(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void translateY(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void translateZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void translateXY(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void translateYZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void translateXZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);

    void scaleX(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void scaleY(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void scaleZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void scaleXY(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void scaleYZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void scaleXZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void scaleXYZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);

    void setIntialScale(int32_t posX, int32_t posY);
    void setIntialRotation(int32_t posX, int32_t posY);

    void rotateX(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void rotateY(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);
    void rotateZ(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY);

    // saves translation to mementos
    void saveTranslation();
    // saves rotation to mementos
    void saveRotation();
    // saves scale to mementos
    void saveScale();

private:
    void translate(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY, math::bvec3 axes);
    void scale(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY, math::bvec3 axes);
    void rotate(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY, math::bvec3 axis);

    // projections from clip space to basic movement tools
    math::vec3 computePlaneOffset(const math::vec2& clipPosStart, const math::vec2& clipPosEnd, const math::vec3& normal) const;
    math::vec3 computeAxisOffset(const math::vec2& clipPosStart, const math::vec2& clipPosEnd, const math::vec3& axis) const;
    math::quat computeRotationOffset(const math::vec2& clipPosStart, const math::vec2& clipPosEnd, const math::vec3& rotationOrigin, const math::vec3& rotationAxis) const;

    // helpers

    math::mat4 getNodeTransform() const;
    math::mat3x3 getNodeRotation() const;
    math::quat getNodeRotationQuat() const;

    math::mat4 getParentTransform() const;
    math::mat3x3 getParentRotation() const;
    math::quat getParentRotationQuat() const;

    // geometry helpers

    // conversions to clip space
    std::tuple<math::vec2, math::vec2> cursorToClipSpace(int32_t lastX, int32_t lastY, int32_t deltaX, int32_t deltaY) const;
    math::vec2 widgetToClipSpace(const math::vec2& widgetPos) const;

    // conversions from clip space
    std::tuple<math::vec4, math::vec3> clipPosToWorldRay(const math::vec2& clipPos) const;
    math::vec2 worldToClipPos(const math::vec3& worldPos) const;

    // line to line
    std::tuple<math::vec3, math::vec3> closestPointOfApproach(const math::vec3& P0, const math::vec3& u, const math::vec3& Q0, const math::vec3& v) const;
    float linearIndependence(const math::vec3& lineDir1, const math::vec3& lineDir2) const;

    // line to plane
    math::vec3 planeIntersect(const math::vec3& lineStart, const math::vec3& lineDir, const math::vec3& planeOrigin, const math::vec3& planeNormal) const;

    // point to plane
    float distanceToPlane(const math::vec3& position, const math::vec3& planeOrigin, const math::vec3& planeNormal) const;

private:
    const GLTFRendererWidget& m_widget;
    gltf::Node* m_node;
    math::vec3 m_initialNodeScale = math::vec3(1.0f);
    math::mat3 m_initialNodeRotation = math::mat3();
    math::mat4 m_initialWorldTransform = math::mat4(1.0f);
    math::vec2 m_initialDistance = math::vec2(1.0f);
    std::optional<model::NodeBinding> m_nodeBinding;
};
}
