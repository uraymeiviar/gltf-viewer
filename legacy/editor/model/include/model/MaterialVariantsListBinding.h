#pragma once

#include "TBinding.h"
#include "TEventManager.h"

#include "core/String.h"
#include "core/Vector.h"
#include <optional>

namespace ux3d::gltf {
class MaterialVariant;
}

namespace ux3d::model {
class IMaterialVariantsListPresentation {
public:
    // simple notifier
    virtual void presentMaterialVariantsSelected(std::optional<uint32_t> _materialVariantID) { }
    virtual void presentMaterialVariantsSelected(int _id, core::Vector<const core::string> _materialVariantNames) { }
};

class MaterialVariantsListBinding : public TBinding<IMaterialVariantsListPresentation> {
public:
    using TBinding::ID;

    MaterialVariantsListBinding(IMaterialVariantsListPresentation* _presentation = nullptr);
    ~MaterialVariantsListBinding() override;

    ID getID() const override;
    void refresh() const;
};

class MaterialVariantsListEventManager : public TEventManager<MaterialVariantsListEventManager, MaterialVariantsListBinding, IMaterialVariantsListPresentation> {
public:
    static MaterialVariantsListEventManager* getInstance();
};
}
