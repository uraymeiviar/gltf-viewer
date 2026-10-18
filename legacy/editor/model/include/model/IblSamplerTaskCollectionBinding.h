// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once

#include "model/IblSamplerTaskBinding.h"
#include "model/TEventManager.h"

#include <vector>

namespace ux3d::model {
class IIblSamplerTaskCollectionPresentation {
public:
    virtual void presentSamplerTaskCollectionTask(const std::vector<IblSamplerTaskBinding>&& tasks) { }
    virtual void presentTaskCreated(IblSamplerTaskBinding&& taskBinding) {};
};

class IblSamplerTaskCollectionBinding : public TBinding<IIblSamplerTaskCollectionPresentation> {
public:
    using TBinding::ID;
    IblSamplerTaskCollectionBinding(IIblSamplerTaskCollectionPresentation* presentation = nullptr);
    ~IblSamplerTaskCollectionBinding();
    void bind(IIblSamplerTaskCollectionPresentation* presentation) final;
    ID getID() const override;

    void refresh();
    void presentTasks();

    void create(const filesystem::Path& pathToHdr, const filesystem::Path& pathToEnvironments);
};

class IblSamplerTaskCollectionEventManager : public TEventManager<IblSamplerTaskCollectionEventManager, IblSamplerTaskCollectionBinding, IIblSamplerTaskCollectionPresentation> {
public:
    static IblSamplerTaskCollectionEventManager* getInstance();
};
}
