#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "FundamentalFrequencyDetector.h"
#include "HumEstimator.h"
#include "LearnedHumModel.h"

class HumAnalyzer
{
public:
    LearnedHumModel analyze(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate
    ) const noexcept
    {
        LearnedHumModel result {};

        const auto detection =
            detector.detect(
                buffer,
                channel,
                sampleRate
            );

        result.frequencyHz =
            detection.frequencyHz;

        result.valid =
            detection.valid;

        result.humDetected =
            detection.humDetected;

        if (
            !result.valid
            || !result.humDetected
        )
        {
            return result;
        }

        result.harmonics =
            estimator.estimate(
                buffer,
                channel,
                sampleRate,
                detection.frequencyHz
            );

        return result;
    }

private:
    FundamentalFrequencyDetector detector;
    HumEstimator estimator;
};