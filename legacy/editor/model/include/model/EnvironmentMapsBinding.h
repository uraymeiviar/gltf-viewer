// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "filesystem/Dir.h"
#include "filesystem/Path.h"
#include "model/TEventManager.h"

#include "model/EnvironmentMapBinding.h"

namespace ux3d::model {

class IEnvironmentMapsPresentation {
public:
    virtual void presentEnvironmentMaps(core::Vector<EnvironmentMapBinding>&& environmentMaps) { }
};

class EnvironmentMapsBinding : public TBinding<IEnvironmentMapsPresentation> {
public:
    using TBinding::ID;

    EnvironmentMapsBinding(const filesystem::Path& path, IEnvironmentMapsPresentation* presentation = nullptr);
    ~EnvironmentMapsBinding();

    ID getID() const override;

    core::Vector<EnvironmentMapBinding> getEnvironmentMaps() const;
    filesystem::Path getPath() const;

    void deleteEnvironmentMap(const EnvironmentMapBinding& _envMap);

    void refresh();

private:
    filesystem::Dir m_environmentMapsDir;
};

class EnvironmentMapsEventManager : public TEventManager<EnvironmentMapsEventManager, EnvironmentMapsBinding, IEnvironmentMapsPresentation> {
public:
    static EnvironmentMapsEventManager* getInstance();
};

}
