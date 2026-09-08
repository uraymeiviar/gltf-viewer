#include "helpers/GltfObjectSelector.h"
#include "core/Vector.h"
#include "gltf/ChildCollector.h"
#include "gltf/Gltf.h"

using namespace ux3d;
using namespace ux3d::editor;

GltfObjectSelector::GltfObjectSelector(const gltf::Gltf& _gltf, GltfObjectSelectionScheme _selection)
    : m_gltf(_gltf)
    , m_selection(_selection)
{
}

void GltfObjectSelector::gather(core::Vector<const gltf::ChildOfRoot*>& _outCandidates) const
{
    bool checkAnimations = false;

    if (m_selection == GltfObjectSelectionScheme::ScenesAndUsedAnimations) {
        checkAnimations = true;
        for (const auto& scene : m_gltf.getScenes()) {
            _outCandidates.emplace_back(&scene);
        }
    } else if (m_selection == GltfObjectSelectionScheme::DefaultSceneAndUsedAnimations) {
        checkAnimations = true;
        if (m_gltf.getDefaultScene() != nullptr) {
            _outCandidates.emplace_back(m_gltf.getDefaultScene());
        }
    } else if (m_selection == GltfObjectSelectionScheme::Skins) {
        for (auto& skin : m_gltf.getSkins()) {
            _outCandidates.emplace_back(&skin);
        }
    }

    gltf::ChildCollector sceneRefsCollector(_outCandidates);
    sceneRefsCollector.collect(_outCandidates);

    if (m_gltf.hasMaterialVariants()) {
        for (auto& materialVariant : m_gltf.getMaterialVariants()) {
            _outCandidates.emplace_back(&materialVariant);
        }
    }

    if (checkAnimations) {
        for (const auto& animation : m_gltf.getAnimations()) {
            core::Vector<const gltf::ChildOfRoot*> animationRefs;
            gltf::ChildCollector animationRefsCollector({ &animation });
            animationRefsCollector.collect(animationRefs);

            for (const auto* ref : animationRefs) {
                if (core::contains(_outCandidates, ref)) {
                    _outCandidates.append(animationRefs);
                    break;
                }
            }
        }
    }
}
