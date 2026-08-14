#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

class HumEstimator
{
public:
    static constexpr std::size_t maxHarmonics = 8;

    struct HarmonicEstimate
    {
        double frequencyHz = 0.0;
        float amplitude = 0.0f;

        // Normalized phase:
        // 0.0 =   0 degrees
        // 0.25 =  90 degrees
        // 0.5 =  180 degrees
        // 0.75 = 270 degrees
        double phase = 0.0;
    };

    using Result =
        std::array<HarmonicEstimate, maxHarmonics>;

    Result estimate(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        double fundamentalFrequencyHz
    ) const noexcept
    {
        Result result {};

        if (
            channel < 0
            || channel >= buffer.getNumChannels()
            || buffer.getNumSamples() <= 0
            || sampleRate <= 0.0
            || fundamentalFrequencyHz <= 0.0
        )
        {
            return result;
        }

        const auto* samples =
            buffer.getReadPointer(channel);

        const auto numSamples =
            buffer.getNumSamples();

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        for (
            std::size_t harmonicIndex = 0;
            harmonicIndex < maxHarmonics;
            ++harmonicIndex
        )
        {
            const auto harmonicNumber =
                static_cast<double>(
                    harmonicIndex + 1
                );

            const auto frequencyHz =
                fundamentalFrequencyHz
                * harmonicNumber;

            auto& harmonic =
                result[harmonicIndex];

            harmonic.frequencyHz =
                frequencyHz;

            // Don't attempt to estimate frequencies
            // at or above Nyquist.
            if (frequencyHz >= sampleRate * 0.5)
                continue;

            const auto phaseIncrement =
                twoPi
                * frequencyHz
                / sampleRate;

            double sineCorrelation = 0.0;
            double cosineCorrelation = 0.0;

            double angle = 0.0;

            for (
                int sample = 0;
                sample < numSamples;
                ++sample
            )
            {
                const auto value =
                    static_cast<double>(
                        samples[sample]
                    );

                sineCorrelation +=
                    value * std::sin(angle);

                cosineCorrelation +=
                    value * std::cos(angle);

                angle += phaseIncrement;
            }

            const auto scale =
                2.0
                / static_cast<double>(
                    numSamples
                );

            const auto sineCoefficient =
                sineCorrelation * scale;

            const auto cosineCoefficient =
                cosineCorrelation * scale;

            const auto amplitude =
                std::sqrt(
                    sineCoefficient
                        * sineCoefficient
                    + cosineCoefficient
                        * cosineCoefficient
                );

            auto phaseRadians =
                std::atan2(
                    cosineCoefficient,
                    sineCoefficient
                );

            if (phaseRadians < 0.0)
                phaseRadians += twoPi;

            harmonic.amplitude =
                static_cast<float>(
                    amplitude
                );

            harmonic.phase =
                phaseRadians / twoPi;
        }

        return result;
    }
};