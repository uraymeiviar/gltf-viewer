// Copyright © 2023 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "nlohmann/json.hpp"
#include <functional>
#include <queue>
#include <rxcpp/rx.hpp>
#include <string>
#include <thread>

namespace ux3d::model {

/**
 * @brief DispatchQueue for tasks that can fail.
 * If the task throws an exception, the tasks observable
 * will receive a value containing the exception text,
 * otherwise the observable terminates
 */
class DispatchQueue {
public:
    // The function that should be executed on the dispatch queue.
    // Make sure to protect all shared resources accessed in this function,
    // as the task will be executed on a separate thread
    using Task = std::function<std::string(void)>;
    using TaskId = size_t;

    DispatchQueue();
    ~DispatchQueue();

    struct ResultPipes {
        rxcpp::observable<std::string> error;
        rxcpp::observable<std::string> success;
    };
    ResultPipes launch(Task&& task);

private:
    void threadHandler();

private:
    std::queue<std::tuple<TaskId, Task>> tasks;
    rxcpp::schedulers::run_loop* runLoop = nullptr;
    rxcpp::subjects::replay<std::tuple<TaskId, std::string>, rxcpp::observe_on_one_worker>* exceptions = nullptr;
    rxcpp::subjects::replay<std::tuple<TaskId, std::string>, rxcpp::observe_on_one_worker>* successes = nullptr;
    rxcpp::subjects::replay<TaskId, rxcpp::observe_on_one_worker>* completions = nullptr;
    std::thread thread;
    std::condition_variable conditionVariable;
    std::mutex tasksMutex;
    TaskId currentId = 1;
    bool quit = false;
};
}
