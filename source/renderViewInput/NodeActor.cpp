#include "renderViewInput/NodeActor.h"

#include "GltfPropertyPointer.h"
#include "widgets/GLTFRendererWidget.h"

#include <limits>

using namespace ux3d;
using namespace ux3d::editor;

NodeActor::NodeActor(const GLTFRendererWidget& _widget, gltf::Node* _node)
    : m_widget(_widget)
    , m_node(_node)
{
}

void NodeActor::setNode(gltf::Node* _node)
{
    if (m_node == _node) {
        return;
    }

    m_node = _node;
    if (m_node == nullptr) {
        m_nodeBinding.reset();
    } else {
        nlohmann::json::json_pointer nodePointer = gltfPointerToJsonPointer(_node, GltfPropertyType::Nodes);
        m_nodeBinding = model::NodeBinding(_node, nodePointer);
    }
}

void NodeActor::translateX(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    translate(_lastX, _lastY, _deltaX, _deltaY, { true, false, false });
}

void NodeActor::translateY(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    translate(_lastX, _lastY, _deltaX, _deltaY, { false, true, false });
}

void NodeActor::translateZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    translate(_lastX, _lastY, _deltaX, _deltaY, { false, false, true });
}

void NodeActor::translateXY(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    translate(_lastX, _lastY, _deltaX, _deltaY, { true, true, false });
}

void NodeActor::translateYZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    translate(_lastX, _lastY, _deltaX, _deltaY, { false, true, true });
}

void NodeActor::translateXZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    translate(_lastX, _lastY, _deltaX, _deltaY, { true, false, true });
}

void NodeActor::scaleX(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { true, false, false });
}

void NodeActor::scaleY(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { false, true, false });
}

void NodeActor::scaleZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { false, false, true });
}

void NodeActor::scaleXY(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { true, true, false });
}

void NodeActor::scaleYZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { false, true, true });
}

void NodeActor::scaleXZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { true, false, true });
}

void NodeActor::scaleXYZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    scale(_lastX, _lastY, _deltaX, _deltaY, { true, true, true });
}

void NodeActor::rotateX(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    rotate(_lastX, _lastY, _deltaX, _deltaY, { true, false, false });
}

void NodeActor::rotateY(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    rotate(_lastX, _lastY, _deltaX, _deltaY, { false, true, false });
}

void NodeActor::rotateZ(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY)
{
    rotate(_lastX, _lastY, _deltaX, _deltaY, { false, false, true });
}

void NodeActor::translate(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY, math::bvec3 _axes)
{
    // compute the offset of the delta in world space
    auto [startClipPos, endClipPos] = cursorToClipSpace(_lastX, _lastY, _deltaX, _deltaY);

    // compute translation along the axis / plane of the gizmo
    math::dvec3 translation;

    if (math::all(_axes)) // all axes, translate on the plane perpendicular to the view dir
    {
        auto camera = m_widget.getGltfRenderLayer()->getActiveCamera();
        math::vec3 normal = math::mat3(inverse(camera->getProjectionMatrix() * camera->getViewMatrix())) * math::vec3(0, 0, 1);
        translation = computePlaneOffset(startClipPos, endClipPos, normal);
    } else if ((_axes.x && _axes.y) || (_axes.x && _axes.z) || (_axes.y && _axes.z)) // is a plane
    {
        math::vec3 normal = math::vec3(1) - math::vec3(_axes);
        if (m_widget.getGizmoSpace() == GizmoSpace::Local) {
            // rotate normal to local space
            normal = getNodeTransform() * math::vec4(normal, 0.0f);
        }
        translation = computePlaneOffset(startClipPos, endClipPos, normal);
    } else if (math::any(_axes)) // is an axis
    {
        math::vec3 axis = math::vec3(_axes);
        if (m_widget.getGizmoSpace() == GizmoSpace::Local) {
            // rotate axis to local space
            axis = getNodeTransform() * math::vec4(axis, 0.0f);
        }

        // the gizmo is flipped (points into the negative direction)
        translation = computeAxisOffset(startClipPos, endClipPos, axis) * -1.0f;

    } else {
        throw std::invalid_argument("Axes argument in translate must have at least one true value");
    }

    // apply to the node
    if (m_nodeBinding.has_value()) {
        translation = math::inverse(getParentTransform()) * math::vec4(translation, 0.0);
        m_nodeBinding.value().setTranslation(m_node->getTranslationOrDefault() + translation);
    }
}

void NodeActor::setIntialScale(int32_t posX, int32_t posY)
{
    if (m_node == nullptr) {
        return;
    }
    m_initialNodeScale = m_node->getScaleOrDefault();
    math::vec2 cursorPos = widgetToClipSpace({ posX, posY });
    math::vec2 nodeClipPos = worldToClipPos(math::vec3(getNodeTransform()[3]));
    m_initialDistance = cursorPos - nodeClipPos;
    m_initialWorldTransform = getNodeTransform();
}

void NodeActor::setIntialRotation(int32_t posX, int32_t posY)
{
    if (m_node == nullptr) {
        return;
    }
    m_initialNodeRotation = getNodeRotation();
    math::vec2 cursorPos = widgetToClipSpace({ posX, posY });
    math::vec2 nodeClipPos = worldToClipPos(math::vec3(getNodeTransform()[3]));
    m_initialDistance = cursorPos - nodeClipPos;
    if (math::length(m_initialDistance) == 0) {
        m_initialDistance = { 0.0f, 1.0f };
    }
    m_initialDistance = math::normalize(m_initialDistance);
    m_initialWorldTransform = getNodeTransform();
    m_initialNodeScale = m_node->getScaleOrDefault();
}

void NodeActor::scale(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY, math::bvec3 _axes)
{
    if (!m_nodeBinding.has_value()) {
        return;
    }
    // compute the offset of the delta in world space
    auto [startClipPos, endClipPos] = cursorToClipSpace(_lastX, _lastY, _deltaX, _deltaY);

    if (math::all(_axes)) {
        // Project delta on diagional
        float scaling = math::dot(endClipPos - startClipPos, math::normalize(math::vec2(1.0f, 1.0f)));
        // Multiply with the initial scale
        scaling *= math::length(m_initialNodeScale);

        m_nodeBinding.value().setScale(m_node->getScaleOrDefault() + math::dvec3(scaling));
    } else {
        // Implemention similar to blender: distance from intital click to gizmo center is scale. Therefore, if you move the same distance away scale will be doubled.
        math::vec2 nodeClipPos = worldToClipPos(math::vec3(getNodeTransform()[3]));

        float initialDistance = math::length(m_initialDistance);
        if (initialDistance == 0) {
            return;
        }

        math::vec3 axes = math::vec3(_axes);

        float scaling = math::length(endClipPos - nodeClipPos) / initialDistance;

        math::vec3 scaleFactor = axes * scaling;

        // If scaled in global space we need to rotate the scaling axes and scale according to the ratios of the vector components
        // e.g. 0 0 1 -> 0.7 0 0.8
        if (m_widget.getGizmoSpace() == GizmoSpace::Global) {
            axes = getNodeRotation() * axes;
            scaleFactor = math::vec3(1.0f) + (scaling - 1) * math::abs(axes);
        }

        // If mouse cursor is on the other side of the selected gizmo axis use negative scaling
        if (math::dot(endClipPos - nodeClipPos, m_initialDistance) < 0) {
            scaleFactor *= -1;
        }

        // since we multiply with the inital scaling all non scaled elements needs to be 1
        for (int i = 0; i < 3; ++i) {
            if (scaleFactor[i] == 0) {
                scaleFactor[i] = 1;
            }
        }

        m_nodeBinding.value().setScale(m_initialNodeScale * scaleFactor);
    }
}

void NodeActor::rotate(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY, math::bvec3 _axis)
{
    const math::vec3 gizmoOrigin = getNodeTransform()[3];
    auto [startClipPos, endClipPos] = cursorToClipSpace(_lastX, _lastY, _deltaX, _deltaY);

    math::dquat rotation;
    if (math::all(_axis)) {
        // rotate on the plane perpendicular to the view dir
        auto camera = m_widget.getGltfRenderLayer()->getActiveCamera();
        math::vec3 normal = math::mat3(inverse(camera->getProjectionMatrix() * camera->getViewMatrix())) * math::vec3(0, 0, 1);
        rotation = computeRotationOffset(startClipPos, endClipPos, gizmoOrigin, normal);
    } else if (math::any(_axis) && 1 == math::compAdd(math::ivec3(_axis))) // is an axis (with exactly one component true)
    {
        // Rotation similar to blender: Use a circle in clipspace where the center is the gizmo center. Map the angle from clipspace to world/local space
        math::vec2 nodeClipPos = worldToClipPos(gizmoOrigin);
        math::vec2 currentDir = math::normalize(endClipPos - nodeClipPos);
        float angle = math::orientedAngle(m_initialDistance, currentDir);

        // axis in local space
        math::vec3 rotationAxis = math::vec3(_axis);

        auto* camera = m_widget.getGltfRenderLayer()->getActiveCamera();
        math::vec3 viewDir;

        // If camera is perspective we need to get the viewing direction from the viewProj matrix.
        // Otherwise if you look in the same direction as the rotation axis the rotation might be in the wrong direction.
        if (camera->getType() == renderer::Camera::Type::Perspective) {
            auto viewProj = camera->getProjectionMatrix() * camera->getViewMatrix();
            constexpr auto identity = math::vec4(0, 0, 1, 0);
            viewDir = -math::normalize(math::vec3(identity * viewProj));
        } else {
            viewDir = camera->getView();
        }

        // Check from which side the camera is looking at the rotation axis and correct for it
        if (m_widget.getGizmoSpace() == GizmoSpace::Global) {
            if (math::dot(rotationAxis, viewDir) <= 0) {
                angle *= -1.0f;
            }
        } else {
            // bring the axis in global space
            math::vec3 tmpRotationAxis = math::mat3(m_initialWorldTransform) * rotationAxis;
            if (math::dot(tmpRotationAxis, viewDir) <= 0) {
                angle *= -1.0f;
            }
        }

        math::mat3 rotationDelta = math::rotate(angle, rotationAxis);
        math::mat3 currentRotation = math::inverse(getParentRotation()) * m_initialNodeRotation;
        math::mat3 rotationMatrix;

        if (m_widget.getGizmoSpace() == GizmoSpace::Global) {
            math::mat3 parentTransform = getParentTransform();
            // In Global space we need to rotate the rotation axis with the inverse global rotation, since glm::rotate always works in local space
            rotationMatrix = math::inverse(parentTransform) * rotationDelta * parentTransform * currentRotation;
        } else {
            // The determinant of a orthogonal (rotation) matrix is exactly 1 or -1. It is positive if the scale is positive and
            // negative if there is negative scale in the transform matrix. (Also see how winding order is affected by a negative determinant)
            if (math::determinant(math::mat3(getParentTransform())) < 0.0) {
                rotationDelta = math::inverse(rotationDelta);
            }

            // If there is a negative scale on the same axis that is used for rotation, the rotation appears inverted in the viewport.
            // Correct this by inverting the rotation
            if (math::all(math::lessThanEqual(m_initialNodeScale * rotationAxis, math::vec3(0)))) {
                rotationDelta = math::inverse(rotationDelta);
            }

            // 🤨 not sure why the currentRotation needs to be on the left side here (in contrast to global rotation)
            rotationMatrix = currentRotation * rotationDelta;
        }

        rotation = math::toQuat(rotationMatrix);

    } else {
        throw std::invalid_argument("Axis argument in rotate may only have a single true value or be all true");
    }

    // apply to the node
    if (m_nodeBinding.has_value()) {
        m_nodeBinding.value().setRotation(rotation);
    }
}

math::vec3 NodeActor::computePlaneOffset(const math::vec2& _clipPosStart, const math::vec2& _clipPosEnd, const math::vec3& _normal) const
{
    const math::vec3 origin = getNodeTransform()[3];
    auto [startRayOrigin, startRayDir] = clipPosToWorldRay(_clipPosStart);
    auto [endRayOrigin, endRayDir] = clipPosToWorldRay(_clipPosEnd);

    // project the cursor positions to the plane of the gizmo
    const math::vec3 start = planeIntersect(startRayOrigin, startRayDir, origin, _normal);
    const math::vec3 end = planeIntersect(endRayOrigin, endRayDir, origin, _normal);

    // the end position is beyond the horizon -> don't move
    if (math::any(math::isinf(end)) || distanceToPlane(end, endRayOrigin, endRayDir) < 0.0f) {
        return math::vec3(0.0f);
    }

    // handle the case where we are coming back from the horizon
    if (math::any(math::isinf(start)) || distanceToPlane(start, startRayOrigin, startRayDir) < 0.0f) {
        return end - origin;
    }

    return end - start;
}

// https://nelari.us/post/gizmos/
math::vec3 NodeActor::computeAxisOffset(const math::vec2& _clipPosStart, const math::vec2& _clipPosEnd, const math::vec3& _gizmoDir) const
{
    math::vec4 gizmoOrigin = getNodeTransform()[3];

    const auto [startRayOrigin, startRayDir] = clipPosToWorldRay(_clipPosStart);
    const auto [endRayOrigin, endRayDir] = clipPosToWorldRay(_clipPosEnd);

    // check if the cursor ray is approaching parallelity with the gizmo axis and use the clipspace
    // movement of the cursor if it is
    const float orthogonality = linearIndependence(math::normalize(endRayDir), _gizmoDir);
    constexpr float orthogonalityCutoff = 0.1f;
    if (orthogonality < orthogonalityCutoff) {
        // project the gizmo axis to clip space (at the position of the gizmo in clip space)
        math::vec2 axisClipDir = worldToClipPos(math::vec3(gizmoOrigin) + _gizmoDir) - worldToClipPos(math::vec3(gizmoOrigin));

        // sanitize the axis clip dir for the case where the camera points directly along the gizmo axis
        if (math::length(axisClipDir) == 0.0f) {
            // up and right should correspond to movement along the gizmo axis
            axisClipDir = math::vec2(std::numeric_limits<float>::epsilon());
        }

        // clipspace movement of the cursor
        math::vec2 offsetClipDir = _clipPosEnd - _clipPosStart;

        // project the movement of the cursor in clip space to the axis of the gizmo in clip space
        float offsetAmount = math::dot(math::normalize(axisClipDir), offsetClipDir);
        // make offset inversely proportional to the size of the gizmo axis on the screen
        offsetAmount /= math::length(axisClipDir);
        // limit the offset to a maximum speed
        constexpr float offsetMax = 10.0f;
        offsetAmount = math::clamp(offsetAmount, -offsetMax, offsetMax);
        return _gizmoDir * -offsetAmount;
    }

    // compute an offset that corresponds to the projected movement of the cursor on the gizmo axis
    const auto [startRayIntersection, start] = closestPointOfApproach(math::vec3(startRayOrigin), startRayDir, math::vec3(gizmoOrigin), _gizmoDir);
    const auto [endRayIntersection, end] = closestPointOfApproach(math::vec3(endRayOrigin), endRayDir, math::vec3(gizmoOrigin), _gizmoDir);

    return end - start;
}

math::quat NodeActor::computeRotationOffset(const math::vec2& _clipPosStart, const math::vec2& _clipPosEnd, const math::vec3& _rotationOrigin, const math::vec3& _rotationAxis) const
{
    const auto [startRayOrigin, startRayDir] = clipPosToWorldRay(_clipPosStart);
    const auto [endRayOrigin, endRayDir] = clipPosToWorldRay(_clipPosEnd);

    // project the cursor positions to the plane of the gizmo
    const math::vec3 start = planeIntersect(startRayOrigin, startRayDir, _rotationOrigin, _rotationAxis);
    const math::vec3 end = planeIntersect(endRayOrigin, endRayDir, _rotationOrigin, _rotationAxis);

    // the start or end position is beyond the horizon -> don't rotate
    if (math::any(math::isinf(end)) || distanceToPlane(end, endRayOrigin, endRayDir) < 0.0f || math::any(math::isinf(start)) || distanceToPlane(start, startRayOrigin, startRayDir) < 0.0f) {
        // identity quaternion
        return math::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const math::vec3 startDir = math::normalize(start - _rotationOrigin);
    const math::vec3 endDir = math::normalize(end - _rotationOrigin);
    return math::rotation(startDir, endDir);
}

// Helpers

math::mat4 NodeActor::getNodeTransform() const
{
    return m_widget.getGltfRenderLayer()->getWorldTransform(m_node);
}

math::mat3x3 NodeActor::getNodeRotation() const
{
    return m_widget.getGltfRenderLayer()->getWorldRotation(m_node);
}

math::quat NodeActor::getNodeRotationQuat() const
{
    return m_widget.getGltfRenderLayer()->getWorldRotationQuat(m_node);
}

math::mat4 NodeActor::getParentTransform() const
{
    return m_widget.getGltfRenderLayer()->getParentWorldTransform(m_node);
}

math::mat3x3 NodeActor::getParentRotation() const
{
    return m_widget.getGltfRenderLayer()->getParentWorldRotation(m_node);
}

math::quat NodeActor::getParentRotationQuat() const
{
    return m_widget.getGltfRenderLayer()->getParentWorldRotationQuat(m_node);
}

// Geometry Helpers

// cursorToClipSpace generates 2D clip space positions {x, y} with a marginalized z from cursor input data
std::tuple<math::vec2, math::vec2> NodeActor::cursorToClipSpace(int32_t _lastX, int32_t _lastY, int32_t _deltaX, int32_t _deltaY) const
{
    // compute the bounds of the delta in clip space
    math::vec2 startClipPos = widgetToClipSpace({ _lastX - _deltaX, _lastY - _deltaY });
    math::vec2 endClipPos = widgetToClipSpace({ _lastX, _lastY });
    return { startClipPos, endClipPos };
}

// widgetToClipSpace generates 2D clip space positions {x, y} with a marginalized z component.
math::vec2 NodeActor::widgetToClipSpace(const math::vec2& _widgetPos) const
{
    const float scalingFactor = m_widget.getDevicePixelRatio();
    const math::vec2 viewportSize = { m_widget.getWidth(), m_widget.getHeight() };

    math::vec2 clip = _widgetPos * scalingFactor; // apply window scaling factor to widget coordinates
    clip.y = viewportSize.y - clip.y; // flip y
    clip /= viewportSize; // bring to [0, 1] domain
    return clip * 2.0f - 1.0f; // bring tp [-1,1] domain
}

std::tuple<math::vec4, math::vec3> NodeActor::clipPosToWorldRay(const math::vec2& _clipPos) const
{
    auto camera = m_widget.getGltfRenderLayer()->getActiveCamera();
    math::mat4 viewProj = camera->getProjectionMatrix() * camera->getViewMatrix();
    math::mat4 invViewProj = inverse(viewProj);

    math::vec4 rayStart = invViewProj * math::vec4(_clipPos.x, _clipPos.y, -1.f, 1.f);
    rayStart /= rayStart.w;
    math::vec4 rayEnd = invViewProj * math::vec4(_clipPos.x, _clipPos.y, 1.f, 1.f);
    rayEnd /= rayEnd.w;
    math::vec3 ray = rayEnd - rayStart;

    return { rayStart, ray };
}

math::vec2 NodeActor::worldToClipPos(const math::vec3& _worldPos) const
{
    auto camera = m_widget.getGltfRenderLayer()->getActiveCamera();
    math::mat4 viewProj = camera->getProjectionMatrix() * camera->getViewMatrix();

    math::vec4 clipPos = viewProj * math::vec4(_worldPos, 1.0);
    clipPos /= clipPos.w;
    return math::vec2(clipPos);
}

// closestPointOfApproach computes the point of closest approach for two lines and returns a tuple {determinant, t_1, t_2}
// http://geomalgorithms.com/a07-_distance.html
std::tuple<math::vec3, math::vec3> NodeActor::closestPointOfApproach(const math::vec3& P0, const math::vec3& u, const math::vec3& Q0, const math::vec3& v) const
{
    const auto P = [&](float t) { return P0 + u * t; };
    const auto Q = [&](float t) { return Q0 + v * t; };

    // system of linear equations
    const math::dmat2 A = {
        math::dot(u, u), math::dot(u, v),
        math::dot(u, v), math::dot(v, v)
    };
    const math::dvec2 b = {
        math::dot(u, Q0 - P0),
        math::dot(v, Q0 - P0)
    };

    // the linear system only has a solution if the lines are not parallel. Check for linear independence first.
    math::vec2 x = math::inverse(A) * b;
    return { P(x[0]), Q(x[1]) };
}

float NodeActor::linearIndependence(const math::vec3& u, const math::vec3& v) const
{
    // see NodeActor::closestPointOfApproach
    const math::dmat2 A = {
        math::dot(u, u), math::dot(u, v),
        math::dot(u, v), math::dot(v, v)
    };

    return math::determinant(A);
}

math::vec3 NodeActor::planeIntersect(const math::vec3& lineStart, const math::vec3& lineDir, const math::vec3& planeOrigin, const math::vec3& planeNormal) const
{
    // intersect the ray with the plane
    float intersectionDistance = math::dot(math::vec3(planeOrigin - lineStart), planeNormal) / math::dot(lineDir, planeNormal);

    // construct the intersection point
    return lineStart + lineDir * intersectionDistance;
}

float NodeActor::distanceToPlane(const math::vec3& position, const math::vec3& planeOrigin, const math::vec3& planeNormal) const
{
    return math::dot(math::vec3(position - planeOrigin), math::normalize(planeNormal));
}

// saves translation to mementos
void NodeActor::saveTranslation()
{
    m_nodeBinding.value().saveTranslation();
}
// saves rotation to mementos
void NodeActor::saveRotation()
{
    m_nodeBinding.value().saveRotation();
}
// saves scale to mementos
void NodeActor::saveScale()
{
    m_nodeBinding.value().saveScale();
}
