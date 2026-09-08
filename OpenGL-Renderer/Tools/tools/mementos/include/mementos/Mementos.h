// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <memory>
#include <vector>

#include <rxcpp/rx.hpp>

namespace ux3d::mementos {

template <typename T>
struct Instant {
    std::vector<std::shared_ptr<const T>> past;
    std::vector<std::shared_ptr<const T>> future;
};

/// Mementos is used to store a history of states and allow undo and redo operations on that history.
template <typename T>
class Mementos {
public:
    Mementos();
    ~Mementos();

    /// Commit an lvalue to the state history. Afterwards the head points to the inserted element.
    void commit(const T& value);

    /// Commit an rvalue to the state history.  Afterwards the head points to the inserted element.
    void commit(T&& value);

    /// Move the head back in the history. Adds the current head to the future (accessible by moveForward).
    void moveBack();

    /// Move the head forwad into the future. Adds the next element to the future to the top of the history.
    void moveForward();

    /// Clear both history and future. Resets mementos to the uninitialized state
    void clear();

    rxcpp::observable<Instant<T>> instant;
    rxcpp::observable<std::shared_ptr<const T>> headPointer;
    rxcpp::observable<T> head;
    rxcpp::observable<bool> canMoveBack;
    rxcpp::observable<bool> canMoveForward;
    rxcpp::observable<bool> pastless;
    rxcpp::observable<bool> futureless;
    rxcpp::observable<size_t> size;

    Instant<T> present;

private:
    rxcpp::subjects::subject<Instant<T>> subject;
};

template <typename T>
Mementos<T>::Mementos()
{
    instant = subject.get_observable().start_with(present);

    headPointer = instant
                      .filter([](const Instant<T>& instant) { return !instant.past.empty(); })
                      .map([](const Instant<T>& instant) { return instant.past.back(); });
    head = headPointer.map([](const std::shared_ptr<const T>& pointer) { return *pointer; });
    canMoveBack = instant.map([](const Instant<T>& instant) { return !instant.past.empty(); }).distinct_until_changed();
    canMoveForward = instant.map([](const Instant<T>& instant) { return !instant.future.empty(); }).distinct_until_changed();
    pastless = instant.map([](const Instant<T>& instant) { return instant.past.empty(); }).distinct_until_changed();
    futureless = instant.map([](const Instant<T>& instant) { return instant.future.empty(); }).distinct_until_changed();
    size = instant.map([](const Instant<T>& instant) { return instant.past.size(); }).distinct_until_changed();
}

template <typename T>
Mementos<T>::~Mementos()
{
    subject.get_subscriber().on_completed();
}

template <typename T>
void Mementos<T>::commit(const T& value)
{
    present.past.push_back(std::make_shared<T>(value));
    present.future.clear();
    subject.get_subscriber().on_next(present);
}

template <typename T>
void Mementos<T>::commit(T&& value)
{
    present.past.push_back(std::make_shared<T>(std::move(value)));
    present.future.clear();
    subject.get_subscriber().on_next(present);
}

template <typename T>
void Mementos<T>::moveBack()
{
    if (present.past.empty()) {
        return;
    }

    auto recent = std::move(present.past.back());
    present.past.pop_back();
    present.future.push_back(std::move(recent));

    subject.get_subscriber().on_next(present);
}

template <typename T>
void Mementos<T>::moveForward()
{
    if (present.future.empty()) {
        return;
    }

    auto imminent = std::move(present.future.back());
    present.future.pop_back();
    present.past.push_back(std::move(imminent));

    subject.get_subscriber().on_next(present);
}

template <typename T>
void Mementos<T>::clear()
{
    if (present.past.empty() && present.future.empty()) {
        return;
    }
    present.past.clear();
    present.future.clear();
    subject.get_subscriber().on_next(present);
}

}
