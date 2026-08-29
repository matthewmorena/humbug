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

        state.store(
            State::Inactive
        );
    }

    bool startTracking() noexcept
    {
        if (
            state.load()
            != State::Inactive
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

    bool isInactive() const noexcept
    {
        return state.load()
            == State::Inactive;
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

        state.store(
            State::Waiting
        );
    }

    bool isModelReady() const noexcept
    {
        return state.load()
            == State::ModelReady;
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

    std::uint64_t processedSamples = 0;
    std::uint64_t analysisStartSample = 0;

    std::uint64_t trackingIntervalSamples = 0;
    std::uint64_t nextCaptureSample = 0;
};