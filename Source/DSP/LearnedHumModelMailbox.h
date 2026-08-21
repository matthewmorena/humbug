#pragma once

#include "PendingLearnResult.h"

#include <atomic>
#include <cstdint>

class LearnedHumModelMailbox
{
private:
    enum class State : std::uint8_t
    {
        Empty,
        Writing,
        Ready,
        Reading
    };

    static_assert(
        std::atomic<State>::is_always_lock_free,
        "Mailbox state must be lock-free"
    );

public:
    bool publish(
        const PendingLearnResult& result
    ) noexcept
    {
        auto expected =
            State::Empty;

        if (
            !state.compare_exchange_strong(
                expected,
                State::Writing
            )
        )
        {
            return false;
        }

        storage =
            result;

        state.store(
            State::Ready,
            std::memory_order_release
        );

        return true;
    }

    bool tryConsume(
        PendingLearnResult& result
    ) noexcept
    {
        auto expected =
            State::Ready;

        if (
            !state.compare_exchange_strong(
                expected,
                State::Reading,
                std::memory_order_acquire
            )
        )
        {
            return false;
        }

        result =
            storage;

        state.store(
            State::Empty,
            std::memory_order_release
        );

        return true;
    }

    bool hasPendingResult() const noexcept
    {
        return state.load(
            std::memory_order_acquire
        ) == State::Ready;
    }

private:
    PendingLearnResult storage {};

    std::atomic<State> state {
        State::Empty
    };
};