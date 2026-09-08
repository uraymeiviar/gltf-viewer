#include "RxRunLoop.h"

#include <QMetaObject>
#include <QObject>
#include <QThread>
#include <QTimer>

ux3d::editor::RxRunLoop::RxRunLoop(QObject* parent)
    : QObject(parent)
{
    timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setTimerType(Qt::PreciseTimer);
    timer->connect(timer, &QTimer::timeout, [this] {
        while (!runLoop.empty() && runLoop.peek().when < runLoop.now()) {
            runLoop.dispatch();
        }
        if (!runLoop.empty()) {
            timer->start(remainingTimeUntil(runLoop.peek().when));
        }
    });

    runLoop.set_notify_earlier_wakeup([this](const TimePoint& when) { onEarlierWakeup(when); });
}

rxcpp::observe_on_one_worker ux3d::editor::RxRunLoop::worker() const
{
    return rxcpp::observe_on_run_loop(runLoop);
}

void ux3d::editor::RxRunLoop::onEarlierWakeup(const TimePoint& when)
{
    std::chrono::milliseconds msec = remainingTimeUntil(when);
    if (thread() == QThread::currentThread()) {
        auto remainingTime = timer->remainingTimeAsDuration();
        if (remainingTime < std::chrono::milliseconds::zero() || msec < remainingTime) {
            timer->start(msec);
        }
    } else {
        QMetaObject::invokeMethod(this, [this, when] { onEarlierWakeup(when); });
    }
}

std::chrono::milliseconds ux3d::editor::RxRunLoop::remainingTimeUntil(const TimePoint& when) const
{
    return std::max(std::chrono::ceil<std::chrono::milliseconds>(when - runLoop.now()), std::chrono::milliseconds::zero());
}
