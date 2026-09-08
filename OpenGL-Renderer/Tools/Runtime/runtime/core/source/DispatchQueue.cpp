#include "core/DispatchQueue.h"

using namespace ux3d;
using namespace ux3d::model;

DispatchQueue::DispatchQueue()
{
    this->thread = std::thread(&DispatchQueue::threadHandler, this);
}

DispatchQueue::~DispatchQueue()
{
    // Signal to dispatch threads that it's time to wrap up
    std::unique_lock<std::mutex> lock(this->tasksMutex);
    this->quit = true;
    this->conditionVariable.notify_all();
    lock.unlock();

    // Wait for threads to finish before we exit
    if (this->thread.joinable()) {
        this->thread.join();
    }
}

DispatchQueue::ResultPipes DispatchQueue::launch(Task&& task)
{
    std::unique_lock<std::mutex> lock(this->tasksMutex);

    const TaskId taskId = ++this->currentId;

    this->tasks.push({ taskId, task });
    this->conditionVariable.notify_one();

    auto taskException = this->exceptions->get_observable().filter([taskId](auto t) {
                                                               return std::get<TaskId>(t) == taskId;
                                                           })
                             .map([](auto x) {
                                 return std::get<std::string>(x);
                             });

    auto taskSuccess = this->successes->get_observable().filter([taskId](auto t) {
                                                            return std::get<TaskId>(t) == taskId;
                                                        })
                           .map([](auto x) {
                               return std::get<std::string>(x);
                           });

    auto taskCompletion = this->completions->get_observable().filter([taskId](TaskId completedId) {
                                                                 return taskId == completedId;
                                                             })
                              // Delay triggering the observable so that the output observables (error & success) don't get shut by a completion happening at the same time
                              .delay(std::chrono::milliseconds(100));

    return {
        .error = taskException
                     .take_until(taskCompletion),
        .success = taskSuccess
                       .take_until(taskCompletion)
    };
}

void DispatchQueue::threadHandler()
{
    std::unique_lock<std::mutex> lock(this->tasksMutex);

    this->runLoop = new rxcpp::schedulers::run_loop();
    auto worker = rxcpp::observe_on_run_loop(*runLoop);
    this->exceptions = new rxcpp::subjects::replay<std::tuple<TaskId, std::string>, rxcpp::observe_on_one_worker>(100, worker);
    this->successes = new rxcpp::subjects::replay<std::tuple<TaskId, std::string>, rxcpp::observe_on_one_worker>(100, worker);
    this->completions = new rxcpp::subjects::replay<TaskId, rxcpp::observe_on_one_worker>(100, worker);

    do {
        //Wait until we have data
        this->conditionVariable.wait(lock, [this] {
            return (this->tasks.size() || this->quit);
        });

        //after wait, we own the lock
        if (this->tasks.size()) {
            auto [taskId, task] = std::move(this->tasks.front());
            this->tasks.pop();

            //unlock now that we're done messing with the queue
            lock.unlock();

            try {
                auto message = task();
                this->successes->get_subscriber().on_next(std::make_tuple(taskId, message));
            } catch (const std::exception& error) {
                this->exceptions->get_subscriber().on_next(std::make_tuple(taskId, error.what()));
            }
            this->completions->get_subscriber().on_next(taskId);

            // Trigger the observables
            while (!this->runLoop->empty() && this->runLoop->peek().when <= this->runLoop->now()) {
                this->runLoop->dispatch();
            }

            lock.lock();
        }
    } while (!this->quit);

    delete this->completions;
    delete this->successes;
    delete this->exceptions;
    delete this->runLoop;
}
