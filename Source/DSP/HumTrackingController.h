#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

#include "LearnBuffer.h"

class HumTrackingController
{
public:
    static constexpr double trackingIntervalSeconds =
        1.0;

    enum class State
    {
        Inactive,
        Waiting,
        Collecting,
        ReadyForAnalysis,
        Analyzing,
        ModelReady
    };

    void prepare(
        double sampleRate,
        int numChannels
    )
    {
        jassert(sampleRate > 0.0);
        jassert(numChannels > 0);

        learnBuffer.prepare(
            sampleRate,
            numChannels
        );

        trackingIntervalSamples =
            static_cast<std::uint64_t>(
                std::round(
                    sampleRate
                    * trackingIntervalSeconds
                )
            );

        processedSamples = 0;
        analysisStartSample = 0;
        nextCaptureSample = 0;

        nextReferenceFrequencyHz =
            0.0;

        analysisReferenceFrequencyHz =
            0.0;

        state.store(
            State::Inactive
        );

        stopRequested.store(
            false,
            std::memory_order_release
        );
    }

    bool setReferenceFrequency(
        double newReferenceFrequencyHz
    ) noexcept
    {
        if (
            !std::isfinite(
                newReferenceFrequencyHz
            )
            || newReferenceFrequencyHz <= 0.0
        )
        {
            return false;
        }

        nextReferenceFrequencyHz =
            newReferenceFrequencyHz;

        return true;
    }

    bool startTracking() noexcept
    {
        if (
            state.load()
                != State::Inactive
            || !std::isfinite(
                nextReferenceFrequencyHz
            )
            || nextReferenceFrequencyHz <= 0.0
        )
        {
            return false;
        }

        nextCaptureSample =
            processedSamples
            + trackingIntervalSamples;

        state.store(
            State::Waiting
        );

        stopRequested.store(
            false,
            std::memory_order_release
        );

        return true;
    }

    void processBlock(
        const juce::AudioBuffer<float>& input
    ) noexcept
    {
        if (
            state.load() == State::Waiting
            && processedSamples
                >= nextCaptureSample
        )
        {
            learnBuffer.start();

            if (learnBuffer.isCollecting())
            {
                analysisStartSample =
                    processedSamples;

                analysisReferenceFrequencyHz =
                    nextReferenceFrequencyHz;

                nextCaptureSample =
                    analysisStartSample
                    + trackingIntervalSamples;

                state.store(
                    State::Collecting
                );
            }
        }

        if (
            state.load()
            == State::Collecting
        )
        {
            learnBuffer.push(
                input
            );

            if (learnBuffer.isReady())
            {
                state.store(
                    State::ReadyForAnalysis
                );
            }
        }

        processedSamples +=
            static_cast<std::uint64_t>(
                input.getNumSamples()
            );
    }

    double getAnalysisReferenceFrequency() const noexcept
    {
        jassert(
            state.load()
                == State::Analyzing
        );

        return analysisReferenceFrequencyHz;
    }

    bool isInactive() const noexcept
    {
        return state.load(
            std::memory_order_acquire
        ) == State::Inactive;
    }

    bool isWaiting() const noexcept
    {
        return state.load()
            == State::Waiting;
    }

    bool isCollecting() const noexcept
    {
        return state.load()
            == State::Collecting;
    }

    bool isReadyForAnalysis() const noexcept
    {
        return state.load()
            == State::ReadyForAnalysis;
    }

    bool tryBeginAnalysis() noexcept
    {
        auto expected =
            State::ReadyForAnalysis;

        return state.compare_exchange_strong(
            expected,
            State::Analyzing
        );
    }

    bool isAnalyzing() const noexcept
    {
        return state.load()
            == State::Analyzing;
    }

    void finishAnalysis() noexcept
    {
        jassert(
            state.load()
            == State::Analyzing
        );

        state.store(
            State::ModelReady
        );
    }

    void finishModelHandoff() noexcept
    {
        jassert(
            state.load()
            == State::ModelReady
        );

        if (
            stopRequested.exchange(
                false,
                std::memory_order_acq_rel
            )
        )
        {
            state.store(
                State::Inactive,
                std::memory_order_release
            );

            return;
        }

        state.store(
            State::Waiting,
            std::memory_order_release
        );
    }

    void requestStopTracking() noexcept
    {
        stopRequested.store(
            true,
            std::memory_order_release
        );

        auto expected =
            State::Waiting;

        if (
            state.compare_exchange_strong(
                expected,
                State::Inactive
            )
        )
        {
            stopRequested.store(
                false,
                std::memory_order_release
            );
        }
    }

    bool isModelReady() const noexcept
    {
        return state.load()
            == State::ModelReady;
    }

    void abortAnalysis() noexcept
    {
        jassert(
            state.load()
            == State::Analyzing
        );

        if (
            stopRequested.exchange(
                false,
                std::memory_order_acq_rel
            )
        )
        {
            state.store(
                State::Inactive,
                std::memory_order_release
            );

            return;
        }

        state.store(
            State::Waiting,
            std::memory_order_release
        );
    }

    const juce::AudioBuffer<float>&
    getAnalysisBuffer() const noexcept
    {
        jassert(
            state.load()
            == State::Analyzing
        );

        return learnBuffer.getBuffer();
    }

    std::uint64_t
    getCurrentSamplePosition() const noexcept
    {
        return processedSamples;
    }

    std::uint64_t
    getAnalysisStartSample() const noexcept
    {
        return analysisStartSample;
    }

private:
    LearnBuffer learnBuffer;

    std::atomic<State> state {
        State::Inactive
    };

    std::atomic<bool> stopRequested {
        false
    };

    std::uint64_t processedSamples = 0;
    std::uint64_t analysisStartSample = 0;

    std::uint64_t trackingIntervalSamples = 0;
    std::uint64_t nextCaptureSample = 0;

    double nextReferenceFrequencyHz =
        0.0;

    double analysisReferenceFrequencyHz =
        0.0;
};