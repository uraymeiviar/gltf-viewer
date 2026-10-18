#pragma once

#include <QObject>

#include <rxcpp/rx.hpp>

#include <chrono>

class QTimer;

namespace ux3d::editor {

/// A run loop to schedule observers on a `QThread`.
/// Use it by calling `observable.observe_on(rxRunLoop.worker())`.
class RxRunLoop : public QObject {
    using TimePoint = rxcpp::schedulers::run_loop::clock_type::time_point;

public:
    RxRunLoop(QObject* parent = nullptr);

    rxcpp::observe_on_one_worker worker() const;

private:
    void onEarlierWakeup(const TimePoint& when);
    std::chrono::milliseconds remainingTimeUntil(const TimePoint& when) const;

    rxcpp::schedulers::run_loop runLoop;
    QTimer* timer = nullptr;
};

}
