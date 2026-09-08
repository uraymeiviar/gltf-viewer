#include "gltf/ReferenceCollector.h"
#include "core/Algorithms.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/Gltf.h"
#include "gltf/GltfObject.h"

using namespace ux3d;
using namespace ux3d::gltf;

void ReferenceCollector::collectReferences(Accessor& _accessor)
{
    // Accessors are used in:
    // Meshes/Primitives: Attributes,indices
    // Skins/inverse bind matrices
    // Animations / samplers: input,output

    for (const auto& mesh : m_gltf.getMeshes()) {
        for (const auto& primitive : mesh.getPrimitives()) {
            if (primitive.getIndicesAccessor() == &_accessor) {
                addToGltfObjectList(&mesh);
            }

            for (const auto& attribute : primitive.getAttributes()) {
                if (attribute.getAccessor() == &_accessor) {
                    addToGltfObjectList(&primitive);
                }
            }
        }
    }

    for (const auto& animation : m_gltf.getAnimations()) {
        for (const auto& sampler : animation.getSamplers()) {
            if (sampler.getInputAccessor() == &_accessor) {
                addToGltfObjectList(&animation);
            }

            if (sampler.getOutputAccessor() == &_accessor) {
                addToGltfObjectList(&animation);
            }
        }
    }

    for (const auto& skin : m_gltf.getSkins()) {
        if (skin.getInverseBindMatricesAccessor() == &_accessor) {
            addToGltfObjectList(&skin);
        }
    }
}

core::Vector<const GltfObject*> ReferenceCollector::getCollectedReferences()
{
    return m_gltfObjectList;
}

void ReferenceCollector::resetReferences()
{
    m_gltfObjectList.clear();
}

void ReferenceCollector::addToGltfObjectList(const GltfObject* _element)
{
    if (_element == nullptr) {
        return;
    }

    if (core::contains(m_gltfObjectList, _element)) {
        return;
    }

    m_gltfObjectList.push_back(_element);
}
