// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "filesystem/Dir.h"
#include "filesystem/Path.h"
#include "model/TEventManager.h"

namespace ux3d::model {

class IEnvironmentMapPresentation {
public:
    virtual void presentEnvironmentMapName(const core::string& name) { }
    virtual void presentEnvironmentMapPath(const filesystem::Path& path) { }
    virtual void presentEnvironmentMapRotation(float _rotation) { }
    virtual void presentEnvironmentMapIntensity(float _value) { }
    virtual void presentEnvironmentMapBlurNormalized(float _blur) { }
};

class EnvironmentMapBinding : public TBinding<IEnvironmentMapPresentation> {
public:
    using TBinding::ID;

    EnvironmentMapBinding(const filesystem::Path& path, IEnvironmentMapPresentation* presentation = nullptr);
    ~EnvironmentMapBinding();
    void bind(IEnvironmentMapPresentation* _presentation) final;

    void refresh();

    bool isValid() const;
    core::string getName() const;

    void setEnvironmentMapRotation(float _rotation);
    float getEnvironmentMapRotation() const { return m_environmentMapRotation; }

    void setEnvironmentMapIntensity(float _value);
    float getEnvironmentMapIntensity() const { return m_environmentMapIntensity; }

    void setEnvironmentBlurNormalized(float _blur);
    float getEnvironmentBlurNormalized() const { return m_environmentBlurNormalized; }

    ID getID() const override;

    const filesystem::Dir getPath() const { return m_environmentMapPath; }

private:
    filesystem::Path m_environmentMapPath;
    float m_environmentMapRotation = 90;
    float m_environmentMapIntensity = 1;
    float m_environmentBlurNormalized = 0;
};

class EnvironmentMapEventManager : public TEventManager<EnvironmentMapEventManager, EnvironmentMapBinding, IEnvironmentMapPresentation> {
public:
    static EnvironmentMapEventManager* getInstance();
};

}
