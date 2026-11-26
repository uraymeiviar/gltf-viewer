// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Utils.h"
#include "gltf/Accessor.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/GlTypes.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/Material.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"
#include "gltf/extensions/AsoboExtensions.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

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

namespace {
// The 8 corners of the accessor's min/max box (normalized integers mapped to their float range), transformed.
void accessorCorners(const Accessor& _Accessor, const math::dmat4& _Transform, math::dvec3 (&_corners)[8])
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

    for (int i = 0; i < 8; ++i) {
        const math::dvec3 corner((i & 1) ? boxMax.x : boxMin.x, (i & 2) ? boxMax.y : boxMin.y, (i & 4) ? boxMax.z : boxMin.z);
        _corners[i] = math::dvec3(_Transform * math::dvec4(corner, 1));
    }
}
} // namespace

// from https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/gltf_utils.js
Extends utils::getExtends(const core::Vector<Node*>& _nodes, const Gltf* _pGltf, const math::dmat4& _rootTransform, bool _geometryOnly,
    std::vector<math::dvec3>* _points)
{
    NodeTransforms nodeTransforms;
    fillNodeTransforms(_nodes, nodeTransforms, _rootTransform);

    // KHR_node_visibility: a node the file hides (its own value or an ancestor's below _nodes, current values) does
    // not count, so the framing and the clipping planes ignore what is not rendered. When every node is hidden (Focus
    // on a hidden node selected in the hierarchy), the hidden nodes count after all, so the camera still frames it.
    std::unordered_set<const Node*> fileHidden;
    std::function<void(const Node*, bool)> collectHidden = [&](const Node* node, bool parentHidden) {
        const bool hidden = parentHidden || !node->isFileVisible();
        if (hidden) {
            fileHidden.insert(node);
        }
        for (const Node* child : node->getChildren()) {
            collectHidden(child, hidden);
        }
    };
    for (const Node* node : _nodes) {
        collectHidden(node, false);
    }

    math::dvec3 min(std::numeric_limits<double>::max());
    math::dvec3 max(std::numeric_limits<double>::lowest());

    // Geometry only: primitives of never-rendered materials (MSFS ASOBO_material_invisible collision meshes and
    // ASOBO_material_environment_occluder cabin volumes) do not count either, as for the A3.6 model base.
    std::unordered_map<const Material*, bool> neverRendered;
    const auto skipPrimitive = [&](const Primitive& primitive) {
        const Material* material = primitive.getMaterial();
        if (!_geometryOnly || material == nullptr) {
            return false;
        }
        if (const auto it = neverRendered.find(material); it != neverRendered.end()) {
            return it->second;
        }
        const auto extensions = asobo::parseExtensions(*_pGltf, *material, asobo::Scope::Material);
        bool skip = false;
        if (const auto* invisible = extensions.find("ASOBO_material_invisible")) {
            skip = invisible->boolean("enabled").value_or(true);
        }
        if (const auto* occluder = extensions.find("ASOBO_material_environment_occluder")) {
            skip = skip || occluder->boolean("enabled").value_or(true);
        }
        neverRendered.emplace(material, skip);
        return skip;
    };

    const auto accumulate = [&](bool includeHidden, bool includePoints) {
        for (auto& [pNode, transform] : nodeTransforms) {
            if (!includeHidden && fileHidden.count(pNode) != 0) {
                continue;
            }
            const Mesh* pMesh = pNode->getMesh();
            // if node has no mesh attached but has a light or camera consider it in the extends calculation
            if (pMesh == nullptr) {
                if (includePoints && (pNode->getCamera() != nullptr || pNode->getLight() != nullptr || isNodeUsedAsJointInSkin(_pGltf, pNode))) {
                    auto nodePos = transform * math::vec4(0, 0, 0, 1);
                    min = math::min(min, { nodePos.x, nodePos.y, nodePos.z });
                    max = math::max(max, { nodePos.x, nodePos.y, nodePos.z });
                    if (_points != nullptr) {
                        _points->emplace_back(nodePos.x, nodePos.y, nodePos.z);
                    }
                }
                continue;
            }

            for (const Primitive& primitive : pMesh->getPrimitives()) {
                if (skipPrimitive(primitive)) {
                    continue;
                }
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

                    const auto add = [&](const math::dmat4& instanceTransform) {
                        Extends accessorExtends = getAccessorExtends(*pAccessoor, instanceTransform);
                        if (_points != nullptr) {
                            math::dvec3 corners[8];
                            accessorCorners(*pAccessoor, instanceTransform, corners);
                            _points->insert(_points->end(), std::begin(corners), std::end(corners));
                        }

                        min.x = math::min(min.x, accessorExtends.min.x);
                        min.y = math::min(min.y, accessorExtends.min.y);
                        min.z = math::min(min.z, accessorExtends.min.z);

                        max.x = math::max(max.x, accessorExtends.max.x);
                        max.y = math::max(max.y, accessorExtends.max.y);
                        max.z = math::max(max.z, accessorExtends.max.z);
                    };
                    // EXT_mesh_gpu_instancing: every instance, like a child node with the instance's TRS.
                    if (pNode->getGpuInstances().empty()) {
                        add(transform);
                    } else {
                        for (const GpuInstance& instance : pNode->getGpuInstances()) {
                            add(transform * gpu_instancing::localMatrix(instance));
                        }
                    }
                }
            }
        }
    };
    const auto empty = [&] {
        return min == math::dvec3(std::numeric_limits<double>::max()) && max == math::dvec3(std::numeric_limits<double>::lowest());
    };
    accumulate(false, !_geometryOnly);
    if (!fileHidden.empty() && empty()) {
        accumulate(true, !_geometryOnly);
    }
    // Geometry only, but there is none: frame the cameras, lights and joints after all.
    if (_geometryOnly && empty()) {
        accumulate(false, true);
        if (!fileHidden.empty() && empty()) {
            accumulate(true, true);
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
    // The axis-aligned box of all 8 transformed corners. (The sample viewer's version returned a cube around the
    // bounding sphere of the min/max corners, up to sqrt(3) times too large per axis and far larger for long thin
    // meshes, so framing and Focus kept the model small.)
    math::dvec3 corners[8];
    accessorCorners(_Accessor, _Transform, corners);
    math::dvec3 min = corners[0];
    math::dvec3 max = corners[0];
    for (const math::dvec3& corner : corners) {
        min = math::min(min, corner);
        max = math::max(max, corner);
    }
    return { min, max, (min + max) / 2.0 };
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
