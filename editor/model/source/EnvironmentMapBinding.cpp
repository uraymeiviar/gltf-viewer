// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "model/EnvironmentMapBinding.h"

#include <algorithm>
#include <functional>

using namespace ux3d;
using namespace ux3d::model;

EnvironmentMapBinding::EnvironmentMapBinding(const filesystem::Path& _path, IEnvironmentMapPresentation* _presentation)
    : TBinding<IEnvironmentMapPresentation>(_presentation)
    , m_environmentMapPath(_path)
{
    EnvironmentMapEventManager::getInstance()->registerModel(*this);
}

EnvironmentMapBinding::~EnvironmentMapBinding()
{
    EnvironmentMapEventManager::getInstance()->unregisterModel(*this);
}

void EnvironmentMapBinding::bind(ux3d::model::IEnvironmentMapPresentation* _presentation)
{
    EnvironmentMapEventManager::getInstance()->unregisterModel(*this);
    m_presentation = _presentation;
    EnvironmentMapEventManager::getInstance()->registerModel(*this);
}

void EnvironmentMapBinding::refresh()
{
    EnvironmentMapEventManager::present(getID(), [&](auto& presenter) {
        presenter.presentEnvironmentMapName(m_environmentMapPath.getName());
        presenter.presentEnvironmentMapPath(m_environmentMapPath);
        presenter.presentEnvironmentMapRotation(m_environmentMapRotation);
        presenter.presentEnvironmentMapIntensity(m_environmentMapIntensity);
        presenter.presentEnvironmentMapBlurNormalized(m_environmentBlurNormalized);
    });
}

void EnvironmentMapBinding::setEnvironmentMapRotation(float _rotation)
{
    m_environmentMapRotation = _rotation;
    EnvironmentMapEventManager::present(getID(), [&](auto& presenter) {
        presenter.presentEnvironmentMapRotation(m_environmentMapRotation);
    });
}

void EnvironmentMapBinding::setEnvironmentMapIntensity(float _value)
{
    m_environmentMapIntensity = _value;
    EnvironmentMapEventManager::present(getID(), [&](auto& presenter) {
        presenter.presentEnvironmentMapIntensity(m_environmentMapIntensity);
    });
}

void EnvironmentMapBinding::setEnvironmentBlurNormalized(float _blur)
{
    m_environmentBlurNormalized = _blur;
    EnvironmentMapEventManager::present(getID(), [&](auto& presenter) {
        presenter.presentEnvironmentMapBlurNormalized(m_environmentBlurNormalized);
    });
}

core::string EnvironmentMapBinding::getName() const
{
    return m_environmentMapPath.getName();
}

bool EnvironmentMapBinding::isValid() const
{

    if (m_environmentMapPath.isFile() == false) {
        return false;
    }

    filesystem::File environmentFile = m_environmentMapPath;

    if (environmentFile.getExtension() != ".hdr") {
        return false;
    }

    return true;
}

EnvironmentMapBinding::ID EnvironmentMapBinding::getID() const
{
    return std::hash<core::string> {}(m_environmentMapPath.toString());
}

EnvironmentMapEventManager* EnvironmentMapEventManager::getInstance()
{
    static EnvironmentMapEventManager instance;
    return &instance;
}
