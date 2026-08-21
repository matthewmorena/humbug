#pragma once

#include <atomic>
#include <cstdint>

#include "LearnBuffer.h"

class LearnModeController
{
public:
    enum class State
    {
        Idle,
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
        learnBuffer.prepare(
            sampleRate,
            numChannels
        );

        processedSamples = 0;
        analysisStartSample = 0;

        state.store(
            State::Idle
        );
    }

    bool startLearn() noexcept
    {
        if (
            state.load()
            != State::Idle
        )
        {
            return false;
        }

        learnBuffer.start();

        if (!learnBuffer.isCollecting())
        {
            return false;
        }

        analysisStartSample =
            processedSamples;

        state.store(
            State::Collecting
        );

        return true;
    }

    void processBlock(
        const juce::AudioBuffer<float>& input
    ) noexcept
    {
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

    bool tryBeginAnalysis() noexcept
    {
        auto expected =
            State::ReadyForAnalysis;

        return state.compare_exchange_strong(
            expected,
            State::Analyzing
        );
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
            State::Idle
        );
    }

    void abortAnalysis() noexcept
    {
        jassert(
            state.load()
            == State::Analyzing
        );

        state.store(
            State::Idle
        );
    }

    bool isModelReady() const noexcept
    {
        return state.load()
            == State::ModelReady;
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

    bool isAnalyzing() const noexcept
    {
        return state.load()
            == State::Analyzing;
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

    const juce::AudioBuffer<float>&
    getAnalysisBuffer() const noexcept
    {
        jassert(
            state.load()
            == State::Analyzing
        );

        return learnBuffer.getBuffer();
    }

private:
    LearnBuffer learnBuffer;

    std::atomic<State> state {
        State::Idle
    };

    std::uint64_t processedSamples = 0;

    std::uint64_t analysisStartSample = 0;
};