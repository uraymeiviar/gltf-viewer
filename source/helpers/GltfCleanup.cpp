#include "helpers/GltfCleanup.h"
#include "core/Algorithms.h"
#include "core/Logger.h"
#include "gltf/Gltf.h"
#include "helpers/GltfObjectSelector.h"
#include <QMessageBox>
#include <QPushButton>

using namespace ux3d;
using namespace ux3d::editor;

bool GltfCleanup::deleteAllUnused(gltf::Gltf& _gltf)
{
    core::Vector<const gltf::ChildOfRoot*> usedObjects;

    GltfObjectSelector selector(_gltf, GltfObjectSelectionScheme::ScenesAndUsedAnimations);
    selector.gather(usedObjects);

    core::Vector<gltf::ChildOfRoot*> unusedObjects;

    // we don't remove:
    // * Scenes: we don't consider scenes because they are the base set of what we consider 'used'
    // * Buffers/BufferViews: we don't consider the binary data because
    //   a) the user doesn't know about it and when saving a file, it's automatically pruned
    //   b) they are needed for undo if Accessors or Images have to be restored
    collectUnused(usedObjects, _gltf.getNodes(), unusedObjects);
    collectUnused(usedObjects, _gltf.getCameras(), unusedObjects);
    collectUnused(usedObjects, _gltf.getLights(), unusedObjects);
    collectUnused(usedObjects, _gltf.getMeshes(), unusedObjects);
    collectUnused(usedObjects, _gltf.getAccessors(), unusedObjects);
    collectUnused(usedObjects, _gltf.getImages(), unusedObjects);
    collectUnused(usedObjects, _gltf.getMaterials(), unusedObjects);
    collectUnused(usedObjects, _gltf.getTextures(), unusedObjects);
    collectUnused(usedObjects, _gltf.getSamplers(), unusedObjects);
    collectUnused(usedObjects, _gltf.getAnimations(), unusedObjects);
    collectUnused(usedObjects, _gltf.getSkins(), unusedObjects);
    collectUnused(usedObjects, _gltf.getMaterialVariants(), unusedObjects);
    collectUnused(usedObjects, _gltf.getAudioEmitters(), unusedObjects);
    collectUnused(usedObjects, _gltf.getAudioSources(), unusedObjects);

    if (unusedObjects.empty()) {
        QMessageBox::information(
            nullptr,
            "Clean Up",
            "Every object is in use by at least one scene, nothing to delete.");

        return false;
    }

    QMessageBox messageBox;
    messageBox.setWindowTitle("Clean Up");

    QString text = unusedObjects.size() == 1
        ? "One unused object is about to be deleted."
        : QString::number(unusedObjects.size()) + " unused objects are about to be deleted.";
    messageBox.setText(text);
    messageBox.setIcon(QMessageBox::Icon::Warning);
    QPushButton* deleteButton = new QPushButton("Delete", &messageBox);
    messageBox.addButton(deleteButton, QMessageBox::ButtonRole::YesRole);
    messageBox.setStandardButtons(QMessageBox::Cancel);
    messageBox.setDefaultButton(deleteButton);
    messageBox.exec();

    if (messageBox.clickedButton() == deleteButton) {
        for (gltf::ChildOfRoot* unused : unusedObjects) {
            _gltf.deleteElement(*unused);
        }

        return true;
    }

    return false;
}

template <typename T>
void GltfCleanup::collectUnused(
    const core::Vector<const gltf::ChildOfRoot*>& _usedObjects,
    const core::LinkedList<T>& _candidates,
    core::Vector<gltf::ChildOfRoot*>& _outUnusedObjects)
{
    if (false == _outUnusedObjects.reserve(_outUnusedObjects.size() + _candidates.size())) {
        core::logError("Allocation failure in GltfCleanup::matchAndDelete");
        return;
    }

    for (T& candidate : _candidates) {
        if (false == core::contains(_usedObjects, &candidate)) {
            _outUnusedObjects.emplace_back(&candidate);
        }
    }
}
