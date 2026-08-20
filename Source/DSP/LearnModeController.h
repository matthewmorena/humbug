#pragma once

#include <cstdint>

#include "LearnBuffer.h"

class LearnModeController
{
public:
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
    }

    void reset() noexcept
    {
        learnBuffer.reset();

        analysisStartSample = 0;
    }

    void startLearn() noexcept
    {
        learnBuffer.start();

        if (learnBuffer.isCollecting())
        {
            analysisStartSample =
                processedSamples;
        }
    }

    void processBlock(
        const juce::AudioBuffer<float>& input
    ) noexcept
    {
        learnBuffer.push(
            input
        );

        processedSamples +=
            static_cast<std::uint64_t>(
                input.getNumSamples()
            );
    }

    bool isCollecting() const noexcept
    {
        return learnBuffer.isCollecting();
    }

    bool isReady() const noexcept
    {
        return learnBuffer.isReady();
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
        return learnBuffer.getBuffer();
    }

private:
    LearnBuffer learnBuffer;

    std::uint64_t processedSamples = 0;

    std::uint64_t analysisStartSample = 0;
};