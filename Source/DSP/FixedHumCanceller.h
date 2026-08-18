#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "FundamentalFrequencyDetector.h"
#include "HumEstimator.h"
#include "HumReconstructor.h"

#include <cstddef>

class FixedHumCanceller
{
public:
    struct LearnResult
    {
        double frequencyHz = 0.0;

        bool valid = false;
        bool humDetected = false;
    };

    void prepare(
        double newSampleRate
    ) noexcept
    {
        sampleRate = newSampleRate;

        reconstructor.prepare(
            sampleRate
        );

        reset();
    }

    void reset() noexcept
    {
        active = false;
        reconstructor.reset();
    }

    LearnResult learn(
        const juce::AudioBuffer<float>& buffer,
        int channel
    ) noexcept
    {
        active = false;

        const auto detection =
            detector.detect(
                buffer,
                channel,
                sampleRate
            );

        if (!detection.valid)
            return {};

        if (!detection.humDetected)
        {
            return {
                detection.frequencyHz,
                true,
                false
            };
        }

        const auto model =
            estimator.estimate(
                buffer,
                channel,
                sampleRate,
                detection.frequencyHz
            );

        reconstructor.setModel(
            model,
            static_cast<std::size_t>(
                buffer.getNumSamples()
            )
        );

        active = true;

        return {
            detection.frequencyHz,
            true,
            true
        };
    }

    float processSample(
        float inputSample
    ) noexcept
    {
        if (!active)
            return inputSample;

        return inputSample
            - reconstructor.processSample();
    }

    bool isActive() const noexcept
    {
        return active;
    }

private:
    double sampleRate = 44100.0;

    bool active = false;

    FundamentalFrequencyDetector detector;
    HumEstimator estimator;
    HumReconstructor reconstructor;
};