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
        const auto detection =
            detector.detect(
                buffer,
                channel,
                sampleRate
            );

        return createModel(
            buffer,
            channel,
            sampleRate,
            detection
        );
    }

    LearnedHumModel analyzeNear(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        double referenceFrequencyHz,
        double searchRadiusHz
    ) const noexcept
    {
        const auto detection =
            detector.detectNear(
                buffer,
                channel,
                sampleRate,
                referenceFrequencyHz,
                searchRadiusHz
            );

        return createModel(
            buffer,
            channel,
            sampleRate,
            detection
        );
    }

private:
    LearnedHumModel createModel(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        const FundamentalFrequencyDetector::Result&
            detection
    ) const noexcept
    {
        LearnedHumModel result {};

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

    FundamentalFrequencyDetector detector;
    HumEstimator estimator;
};