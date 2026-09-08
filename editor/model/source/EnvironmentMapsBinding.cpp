// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "model/EnvironmentMapsBinding.h"

#include "filesystem/PathResult.h"

#include <functional>

using namespace ux3d;
using namespace ux3d::model;

EnvironmentMapsBinding::EnvironmentMapsBinding(const filesystem::Path& _path, IEnvironmentMapsPresentation* _presentation)
    : TBinding<IEnvironmentMapsPresentation>(_presentation)
    , m_environmentMapsDir(_path)
{
    EnvironmentMapsEventManager::getInstance()->registerModel(*this);
}

EnvironmentMapsBinding::~EnvironmentMapsBinding()
{
    EnvironmentMapsEventManager::getInstance()->unregisterModel(*this);
}

EnvironmentMapsBinding::ID EnvironmentMapsBinding::getID() const
{
    return std::hash<core::string> {}(m_environmentMapsDir.toString());
}

filesystem::Path EnvironmentMapsBinding::getPath() const
{
    return m_environmentMapsDir;
}

core::Vector<EnvironmentMapBinding> EnvironmentMapsBinding::getEnvironmentMaps() const
{
    core::Vector<EnvironmentMapBinding> envMaps;

    auto contents = m_environmentMapsDir.listContents();
    for (auto envmapPath : contents) {
        if (envmapPath.isFile()) {
            EnvironmentMapBinding envMap(envmapPath);
            if (envMap.isValid()) {
                envMaps.emplace_back(std::move(envMap));
            }

            continue;
        }

        // old environments might be stored in subdirectories
        filesystem::Dir envmapDir = envmapPath;
        for (auto potentialEnvMapPath : envmapDir.listContents()) {
            EnvironmentMapBinding envMap(potentialEnvMapPath);
            if (envMap.isValid()) {
                envMaps.emplace_back(std::move(envMap));
            }
        }
    }
    return envMaps;
}

void EnvironmentMapsBinding::deleteEnvironmentMap(const EnvironmentMapBinding& _envMap)
{
    const filesystem::Dir envMapPath = _envMap.getPath();
    if (core::Failed(envMapPath.remove())) {
        return;
    }
    refresh();
}

void EnvironmentMapsBinding::refresh()
{
    EnvironmentMapsEventManager::present(getID(), [=](auto& presenter) {
        presenter.presentEnvironmentMaps(std::move(this->getEnvironmentMaps()));
    });
}

EnvironmentMapsEventManager* EnvironmentMapsEventManager::getInstance()
{
    static EnvironmentMapsEventManager instance;
    return &instance;
}
