// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once

#include "filesystem/Path.h"
#include "model/TEventManager.h"

namespace ux3d::model {
class IIblSamplerTaskPresentation {
public:
    virtual void presentIblSamplerTaskIsDone(const bool isDone) { }
};

class IblSamplerTaskBinding : public TBinding<IIblSamplerTaskPresentation> {
public:
    using TBinding::ID;
    IblSamplerTaskBinding(uint32_t taskHandle, IIblSamplerTaskPresentation* presentation = nullptr);
    IblSamplerTaskBinding& operator=(IblSamplerTaskBinding&& _other);
    ~IblSamplerTaskBinding();
    void bind(IIblSamplerTaskPresentation* presentation) final;

    void refresh();

    ID getID() const override;

private:
    uint32_t m_taskHandle;
};

class IblSamplerTaskEventManager : public TEventManager<IblSamplerTaskEventManager, IblSamplerTaskBinding, IIblSamplerTaskPresentation> {
public:
    static IblSamplerTaskEventManager* getInstance();
};
}
