// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once

#include <algorithm>
#include <future>
#include <list>
#include <mutex>
#include <optional>
#include <string>

namespace ux3d::model::tasks {
using TaskId = uint32_t;
constexpr TaskId InvalidTaskId = 0u;

template <class T>
class TaskFactory {
public:
    TaskFactory() = default;
    TaskFactory(const TaskFactory&) = delete;
    TaskFactory(TaskFactory&&) = delete;

    template <class... Args>
    T* create(Args&&... args);

    std::optional<T*> find(TaskId taskId);

protected:
    std::list<T> m_tasks;
};

template <class T>
template <class... Args>
T* TaskFactory<T>::create(Args&&... args)
{
    static_assert(std::is_constructible_v<T, Args...>, "Invalid Arguments to create. Must match constructor of the Task");
    return &m_tasks.emplace_back(std::forward<Args>(args)...);
}

template <class T>
std::optional<T*> TaskFactory<T>::find(TaskId _taskId)
{
    auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [&](const T& task) {
        return task.taskId == _taskId;
    });

    if (it == m_tasks.end()) {
        return std::nullopt;
    }

    return &(*it);
}

template <class T>
class Task {
public:
    float getProgress();
    std::string getProgressDescription();
    bool isDone();

protected:
    Task(TaskId hashedArgumentsOrUid);
    void setProgress(float progress);
    void setProgressDescription(const std::string& progressDescription);
    void setDone();

public:
    const TaskId taskId;
    std::future<T> future;

private:
    std::mutex m_progressMutex;
    float m_progress = 0.0;
    std::mutex m_progressDescriptionMutex;
    std::string m_progressDescription = "Not Running";
    std::mutex m_isDoneMutex;
    bool m_isDone = false;
};

template <class T>
inline float Task<T>::getProgress()
{
    std::lock_guard<std::mutex> lock(m_progressMutex);
    return m_progress;
}

template <class T>
inline std::string Task<T>::getProgressDescription()
{
    std::lock_guard<std::mutex> lock(m_progressDescriptionMutex);
    return m_progressDescription;
}

template <class T>
inline bool Task<T>::isDone()
{
    std::lock_guard<std::mutex> lock(m_isDoneMutex);
    return m_isDone;
}

template <class T>
inline Task<T>::Task(TaskId _taskId)
    : taskId(_taskId)
{
}

template <class T>
inline void Task<T>::setProgress(float progress)
{
    std::lock_guard<std::mutex> lock(m_progressMutex);
    m_progress = progress;
}

template <class T>
inline void Task<T>::setProgressDescription(const std::string& progressDescription)
{
    std::lock_guard<std::mutex> lock(m_progressDescriptionMutex);
    m_progressDescription = progressDescription;
}

template <class T>
inline void Task<T>::setDone()
{
    std::lock_guard<std::mutex> lock(m_isDoneMutex);
    m_isDone = true;
}

}
