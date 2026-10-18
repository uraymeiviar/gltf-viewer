// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#include "model/IblSamplerTaskCollectionBinding.h"

#include "model/tasks/IblSamplerTask.h"

using namespace ux3d;
using namespace ux3d::model;

IblSamplerTaskCollectionBinding::IblSamplerTaskCollectionBinding(IIblSamplerTaskCollectionPresentation* _presentation)
    : TBinding<IIblSamplerTaskCollectionPresentation>(_presentation)
{
    IblSamplerTaskCollectionEventManager::getInstance()->registerModel(*this);
}

IblSamplerTaskCollectionBinding::~IblSamplerTaskCollectionBinding()
{
    IblSamplerTaskCollectionEventManager::getInstance()->unregisterModel(*this);
}

void IblSamplerTaskCollectionBinding::bind(IIblSamplerTaskCollectionPresentation* _presentation)
{
    IblSamplerTaskCollectionEventManager::getInstance()->unregisterModel(*this);
    m_presentation = _presentation;
    IblSamplerTaskCollectionEventManager::getInstance()->registerModel(*this);
}

IblSamplerTaskCollectionBinding::ID IblSamplerTaskCollectionBinding::getID() const
{
    return IblSamplerTaskCollectionBinding::ID(0);
}

void IblSamplerTaskCollectionBinding::refresh()
{
    presentTasks();
}

void IblSamplerTaskCollectionBinding::presentTasks()
{
    // TODO: get tasks from factory and present them
    IblSamplerTaskCollectionEventManager::present(getID(), [=](auto& presenter) {
        presenter.presentSamplerTaskCollectionTask({});
    });
}

void IblSamplerTaskCollectionBinding::create(const filesystem::Path& _pathToHdr, const filesystem::Path& _pathToEnvironments)
{
    auto& factory = tasks::IblSamplerTaskFactory::getInstance();
    auto task = factory.create(_pathToHdr, _pathToEnvironments);

    // TODO: get tasks from factory and present them
    IblSamplerTaskCollectionEventManager::present(getID(), [=](IIblSamplerTaskCollectionPresentation& presenter) {
        auto taskBinding = IblSamplerTaskBinding(static_cast<uint32_t>(task->taskId));
        IblSamplerTaskEventManager::getInstance()->unregisterModel(taskBinding);
        presenter.presentTaskCreated(std::move(taskBinding));
    });

    presentTasks();
}

IblSamplerTaskCollectionEventManager* IblSamplerTaskCollectionEventManager::getInstance()
{
    static IblSamplerTaskCollectionEventManager instance;
    return &instance;
}
