#include "model/MaterialVariantsListBinding.h"
#include "model/ProjectManager.h"

using namespace ux3d;
using namespace ux3d::model;

MaterialVariantsListBinding::MaterialVariantsListBinding(IMaterialVariantsListPresentation* _presentation)
    : TBinding<IMaterialVariantsListPresentation>(_presentation)
{
    MaterialVariantsListEventManager::getInstance()->registerModel(*this);
}

MaterialVariantsListBinding::~MaterialVariantsListBinding()
{
    MaterialVariantsListEventManager::getInstance()->unregisterModel(*this);
}

void MaterialVariantsListBinding::refresh() const
{
    MaterialVariantsListEventManager::present(getID(), [&](IMaterialVariantsListPresentation& _presentation) {
        _presentation.presentMaterialVariantsSelected(ProjectManager::getInstance()->getSelectedMaterialVariant());
        _presentation.presentMaterialVariantsSelected(ProjectManager::getInstance()->getSelectedMaterialVariantIndex(),
            ProjectManager::getInstance()->getGltf().getMaterialVariantsNames());
    });
}

MaterialVariantsListEventManager* MaterialVariantsListEventManager::getInstance()
{
    static MaterialVariantsListEventManager instance;
    return &instance;
}

MaterialVariantsListBinding::ID MaterialVariantsListBinding::getID() const
{
    auto* pointer = &(ProjectManager::getInstance()->getGltf().getMaterialVariants());
    return reinterpret_cast<uint64_t>(pointer);
}
