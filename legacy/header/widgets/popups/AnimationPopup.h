#pragma once

#include "core/Vector.h"
#include "interfaces/RenderSettingInteractorOutput.h"
#include "model/GltfBinding.h"
#include "widgets/popups/Popup.h"

class QCheckBox;
class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace ux3d::editor {
class BoolEntry;
class RenderView;

class AnimationPopup : public Popup, public model::IGltfPresentation, public RenderSettingInteractorOutput {
    Q_OBJECT
public:
    AnimationPopup(RenderView* _parent, QPoint _pos);
    ~AnimationPopup();
    void presentAnimationSelectionList(const core::Vector<std::tuple<const gltf::Animation*, bool, bool>> _animationTuples) final;
    void presentSkinningEnabled(bool _enable) final;
    void presentMorphingEnabled(bool _enable) final;

private:
    QScrollArea* m_scrollArea = nullptr;
    QCheckBox* m_skinning = nullptr;
    QCheckBox* m_morphing = nullptr;
    core::Vector<QCheckBox*> m_boolEntries;
    model::GltfBinding m_gltf;
    QCheckBox* m_selectAll = nullptr;
    QLabel* m_pExplanationText = nullptr;
    QLabel* m_pAnimationsLabel = nullptr;
    QLabel* m_pSettingsLabel = nullptr;

    QVBoxLayout* m_scrollLayout = nullptr;
};
}
