// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#include "model/IblSamplerTaskBinding.h"
#include "model/tasks/IblSamplerTask.h"

using namespace ux3d;
using namespace ux3d::model;

IblSamplerTaskBinding::IblSamplerTaskBinding(uint32_t _taskHandle, IIblSamplerTaskPresentation* _presentation)
    : TBinding<IIblSamplerTaskPresentation>(_presentation)
    , m_taskHandle(_taskHandle)
{
    IblSamplerTaskEventManager::getInstance()->registerModel(*this);
}

IblSamplerTaskBinding& IblSamplerTaskBinding::operator=(IblSamplerTaskBinding&& _other)
{
    this->m_taskHandle = _other.m_taskHandle;
    this->bind(_other.m_presentation);
    _other.m_taskHandle = 0;
    _other.bind(nullptr);
    return *this;
}

IblSamplerTaskBinding::~IblSamplerTaskBinding()
{
    IblSamplerTaskEventManager::getInstance()->unregisterModel(*this);
}

void IblSamplerTaskBinding::bind(IIblSamplerTaskPresentation* _presentation)
{
    IblSamplerTaskEventManager::getInstance()->unregisterModel(*this);
    m_presentation = _presentation;
    IblSamplerTaskEventManager::getInstance()->registerModel(*this);
}

IblSamplerTaskBinding::ID IblSamplerTaskBinding::getID() const
{
    return m_taskHandle;
}

void IblSamplerTaskBinding::refresh()
{
    IblSamplerTaskEventManager::present(getID(), [=](auto& presenter) {
        auto task = tasks::IblSamplerTaskFactory::getInstance().find(m_taskHandle);
        if (task != std::nullopt && task.has_value() && task.value() != nullptr) {
            presenter.presentIblSamplerTaskIsDone(task.value()->isDone());
        } else {
            core::logWarning("Binding to unknown task used.");
            presenter.presentIblSamplerTaskIsDone(true);
        }
    });
}

IblSamplerTaskEventManager* IblSamplerTaskEventManager::getInstance()
{
    static IblSamplerTaskEventManager instance;
    return &instance;
}
