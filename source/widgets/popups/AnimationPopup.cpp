#include "widgets/popups/AnimationPopup.h"
#include "helpers/Name.h"
#include "interactors/GltfOperations.h"
#include "interactors/RenderSettingInteractor.h"
#include "model/ProjectManager.h"
#include "widgets/RenderView.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>

using namespace ux3d;
using namespace ux3d::editor;

AnimationPopup::AnimationPopup(RenderView* parent, QPoint position)
    : Popup(parent, position, true, false)
    , m_gltf(this)
{
    RenderSettingInteractor::getInstance()->registerOutput(this);
    setObjectName("AnimationPopup");

    m_entryLayout->setSpacing(5);

    m_pAnimationsLabel = new QLabel(QString("Animations"), this);
    m_pAnimationsLabel->setObjectName("AnimationPopup_Animations");
    m_entryLayout->addWidget(m_pAnimationsLabel);

    m_selectAll = new QCheckBox("Select all non-conflicting", this);
    m_entryLayout->addWidget(m_selectAll);
    connect(m_selectAll, &QCheckBox::toggled, this, [this](bool _checked) {
        m_gltf.selectAllPossibleAnimations(_checked);
        model::ProjectManager::getInstance()->animationVisualizationChanged();
    });

    m_scrollArea = new QScrollArea(this);
    m_entryLayout->addWidget(m_scrollArea);
    m_scrollArea->setWidgetResizable(true);

    QWidget* scrollWidget = new QWidget(this);
    m_scrollArea->setWidget(scrollWidget);

    m_scrollLayout = new QVBoxLayout(scrollWidget);
    m_scrollLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollLayout->setSpacing(5);

    m_gltf.refresh();

    m_pExplanationText = new QLabel(QString("You can only select non-conflicting animations."), this);
    m_pExplanationText->setObjectName("AnimationPopup_Explanation");
    m_entryLayout->addWidget(m_pExplanationText);

    QFrame* line = new QFrame(this);
    line->setObjectName("AnimationPopup_Line");
    line->setFrameShape(QFrame::HLine);
    m_entryLayout->addWidget(line);

    m_pSettingsLabel = new QLabel(QString("Settings"), this);
    m_pSettingsLabel->setObjectName("AnimationPopup_Settings");
    m_entryLayout->addWidget(m_pSettingsLabel);

    m_skinning = new QCheckBox("Skinning", this);
    bool hasSkinning = model::gltf_operations::hasSkinning();
    m_skinning->setEnabled(hasSkinning);
    m_skinning->setChecked(!hasSkinning ? hasSkinning : parent->getIsSkinningEnabled());
    m_entryLayout->addWidget(m_skinning);
    connect(m_skinning, &QCheckBox::toggled, this, [](bool checked) {
        RenderSettingInteractor::getInstance()->setSkinningEnabled(checked);
    });

    m_morphing = new QCheckBox("Morphing", this);
    bool hasMorphing = model::gltf_operations::hasMorphing();
    m_morphing->setEnabled(hasMorphing);
    m_morphing->setChecked(!hasMorphing ? hasMorphing : parent->getIsMorphingEnabled());
    m_entryLayout->addWidget(m_morphing);
    connect(m_morphing, &QCheckBox::toggled, this, [](int checked) {
        RenderSettingInteractor::getInstance()->setMorphingEnabled(checked);
    });
    //unset FixedWidth
    setFixedWidth(QWIDGETSIZE_MAX);
}

AnimationPopup::~AnimationPopup()
{
    RenderSettingInteractor::getInstance()->unregisterOutput(this);
}

void AnimationPopup::presentAnimationSelectionList(const core::Vector<std::tuple<const gltf::Animation*, bool, bool>> _animationTuples)
{
    if (m_boolEntries.empty() || _animationTuples.size() != m_boolEntries.size()) {
        for (QCheckBox* boolEntry : m_boolEntries) {
            boolEntry->deleteLater();
        }
        m_boolEntries.clear();

        int height = 0;

        for (auto& animationTuple : _animationTuples) {
            QCheckBox* animationEntry = new QCheckBox(QString::fromStdString(Name(std::get<0>(animationTuple))), this);
            animationEntry->setChecked(std::get<1>(animationTuple));
            animationEntry->setEnabled(std::get<2>(animationTuple));
            m_scrollLayout->addWidget(animationEntry);
            connect(animationEntry, &QCheckBox::toggled, this, [this](bool _changed) {
                for (int i = 0; i < m_boolEntries.size(); ++i) {
                    if (m_boolEntries[i] == QObject::sender()) {
                        m_gltf.setAnimationPlaying(i, _changed);
                        model::ProjectManager::getInstance()->animationVisualizationChanged();
                        break;
                    }
                }
            });
            m_boolEntries.push_back(animationEntry);
            height += animationEntry->sizeHint().height();
        }

        // Hack to fix popup size if only a few elements should be shown
        if (_animationTuples.size() < 3) {
            m_scrollArea->setFixedHeight(height);
        } else {
            m_scrollArea->setFixedHeight(QWIDGETSIZE_MAX);
        }
    } else {
        //The list itself should not change while the Popup is open
        for (int i = 0; i < m_boolEntries.size(); ++i) {
            QCheckBox* entry = m_boolEntries[i];
            auto& tuple = _animationTuples[i];
            entry->setChecked(std::get<1>(tuple));
            entry->setEnabled(std::get<2>(tuple));
        }
    }
    bool selectAll = true;
    for (QCheckBox* checkbox : m_boolEntries) {
        if (checkbox->isEnabled() && !checkbox->isChecked()) {
            selectAll = false;
            break;
        }
    }
    m_selectAll->blockSignals(true);
    m_selectAll->setChecked(selectAll);
    m_selectAll->blockSignals(false);
}

void AnimationPopup::presentSkinningEnabled(bool _enable)
{
    m_skinning->blockSignals(true);
    m_skinning->setChecked(_enable);
    m_skinning->blockSignals(false);
}

void AnimationPopup::presentMorphingEnabled(bool _enable)
{
    m_morphing->blockSignals(true);
    m_morphing->setChecked(_enable);
    m_morphing->blockSignals(false);
}
