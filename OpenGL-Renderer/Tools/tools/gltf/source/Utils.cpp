// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Utils.h"
#include "gltf/Accessor.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/GlTypes.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"

using namespace ux3d;
using namespace ux3d::gltf;

double utils::getFittingCameraZoom(const Extends& _Extends, double _yFov, double _AspectRatio)
{
    double xFov = _yFov * _AspectRatio;

    double maxAxisLength = math::max(_Extends.max.x - _Extends.min.x, _Extends.max.y - _Extends.min.y);
    double yZoom = maxAxisLength / 2 / math::tan(_yFov / 2);
    double xZoom = maxAxisLength / 2 / math::tan(xFov / 2);

    return math::max(yZoom, xZoom);
}

ClippingPlanes utils::getFittingClippingPlanes(const Extends& _Extends, double _Zoom)
{
    double zAxisLength = _Extends.max.z - _Extends.min.z;
    double zNear = _Zoom - (zAxisLength * 0.6);
    double zFar = _Zoom + (zAxisLength * 0.6);

    return { zNear, zFar };
}

// from https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/gltf_utils.js
Extends utils::getExtends(const core::Vector<Node*>& _nodes, const Gltf* _pGltf, const math::dmat4& _rootTransform)
{
    NodeTransforms nodeTransforms;
    fillNodeTransforms(_nodes, nodeTransforms, _rootTransform);

    math::dvec3 min(std::numeric_limits<double>::max());
    math::dvec3 max(std::numeric_limits<double>::lowest());

    for (auto& [pNode, transform] : nodeTransforms) {
        const Mesh* pMesh = pNode->getMesh();
        // if node has no mesh attached but has a light or camera consider it in the extends calculation
        if (pMesh == nullptr) {
            if (pNode->getCamera() != nullptr || pNode->getLight() != nullptr || isNodeUsedAsJointInSkin(_pGltf, pNode)) {
                auto nodePos = transform * math::vec4(0, 0, 0, 1);
                min = math::min(min, { nodePos.x, nodePos.y, nodePos.z });
                max = math::max(max, { nodePos.x, nodePos.y, nodePos.z });
            }
            continue;
        }

        for (const Primitive& primitive : pMesh->getPrimitives()) {
            for (const Attribute& attribute : primitive.getAttributes()) {
                constexpr auto name = "POSITION";
                if (attribute.getName() != name) {
                    continue;
                }

                const Accessor* pAccessoor = attribute.getAccessor();
                if (pAccessoor == nullptr) {
                    continue;
                }
                if (pAccessoor->getMin().size() != 3 || pAccessoor->getMax().size() != 3) {
                    continue;
                }

                Extends accessorExtends = getAccessorExtends(*pAccessoor, transform);

                min.x = math::min(min.x, accessorExtends.min.x);
                min.y = math::min(min.y, accessorExtends.min.y);
                min.z = math::min(min.z, accessorExtends.min.z);

                max.x = math::max(max.x, accessorExtends.max.x);
                max.y = math::max(max.y, accessorExtends.max.y);
                max.z = math::max(max.z, accessorExtends.max.z);
            }
        }
    }

    if (min == math::dvec3(std::numeric_limits<double>::max()) && max == math::dvec3(std::numeric_limits<double>::lowest())) {
        min = math::dvec3(-5);
        max = math::dvec3(5);
    }

    math::dvec3 center = (min + max) / 2.0;
    return { min, max, center };
}

// from https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/gltf_utils.js
Extends utils::getAccessorExtends(const Accessor& _Accessor, const math::dmat4& _Transform)
{
    math::dvec3 boxMin(_Accessor.getMin()[0], _Accessor.getMin()[1], _Accessor.getMin()[2]);
    math::dvec3 boxMax(_Accessor.getMax()[0], _Accessor.getMax()[1], _Accessor.getMax()[2]);

    if (_Accessor.isNormalized()) {
        double minValueFromComponentType = fabs(GlTypes::minimumValueFromComponentType(_Accessor.getComponentType()));
        double maxValueFromComponentType = fabs(GlTypes::maximumValueFromComponentType(_Accessor.getComponentType()));
        if (minValueFromComponentType != 0) {
            boxMin /= minValueFromComponentType;
        } else {
            boxMin /= maxValueFromComponentType;
        }
        if (maxValueFromComponentType != 0) {
            boxMax /= maxValueFromComponentType;
        }
    }

    boxMin = _Transform * math::dvec4(boxMin, 1);
    boxMax = _Transform * math::dvec4(boxMax, 1);

    math::dvec3 center = (boxMin + boxMax) / 2.0;
    math::dvec3 centerToSurface = boxMax - center;
    double radius = math::length(centerToSurface);

    math::dvec3 min(center.x - radius, center.y - radius, center.z - radius);
    math::dvec3 max(center.x + radius, center.y + radius, center.z + radius);

    return { min, max, center };
}

void utils::fillNodeTransforms(const core::Vector<Node*>& _Nodes, NodeTransforms& _NodeTransforms, const math::dmat4& _Root)
{
    for (const Node* node : _Nodes) {
        math::dmat4 transform = _Root * node->getTransformMatrix();
        if (nullptr == _NodeTransforms.emplace_back(node, transform)) {
            core::logError("Out of memory!");
            return;
        }

        fillNodeTransforms(node->getChildren(), _NodeTransforms, transform);
    }
}

bool utils::isNodeUsedAsJointInSkin(const Gltf* _pGltf, const Node* _pNode)
{
    const core::LinkedList<Skin>& skins = _pGltf->getSkins();
    for (auto& skin : skins) {
        if (core::contains(skin.getJoints(), _pNode)) {
            return true;
        }
    }
    return false;
}

void utils::searchAndReplace(std::string& str, const std::string& search, const std::string& replace)
{
    if (search.empty()) {
        return;
    }
    size_t currentPosition = 0;
    while ((currentPosition = str.find(search, currentPosition)) != std::string::npos) {
        str.replace(currentPosition, search.length(), replace);
        currentPosition += replace.length();
    }
}

bool utils::isURIUsed(const std::string& uri, const Gltf& gltf)
{
    for (auto& image : gltf.getImages()) {
        if (image.getUri().compare(uri) == 0) {
            return true;
        }
    }
    for (auto& buffer : gltf.getBuffers()) {
        if (buffer.getUri().compare(uri) == 0) {
            return true;
        }
    }
    for (auto& audioSource : gltf.getAudioSources()) {
        if (audioSource.hasUri() && audioSource.getUri().compare(uri) == 0) {
            return true;
        }
    }
    return false;
}

bool utils::hasURICollision(const ChildOfRoot& object, const GltfType type, const std::string& uri, const Gltf& gltf)
{
    // compare the ID individually for each type, because the ID is unique per type
    for (auto& image : gltf.getImages()) {
        if (type == GltfType::Image && image.m_ID == object.m_ID) {
            continue;
        }
        if (image.getUri().compare(uri) == 0) {
            return true;
        }
    }
    for (auto& buffer : gltf.getBuffers()) {
        if (type == GltfType::Buffer && buffer.m_ID == object.m_ID) {
            continue;
        }
        if (buffer.getUri().compare(uri) == 0) {
            return true;
        }
    }
    for (auto& audioSource : gltf.getAudioSources()) {
        if (type == GltfType::AudioSource && audioSource.m_ID == object.m_ID) {
            continue;
        }
        if (audioSource.getUri().compare(uri) == 0) {
            return true;
        }
    }
    return false;
}
