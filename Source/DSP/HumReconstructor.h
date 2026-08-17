#pragma once

#include "HumEstimator.h"
#include "Oscillator.h"

#include <array>
#include <cstddef>
#include <cmath>

class HumReconstructor
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;

        for (auto& oscillator : oscillators)
            oscillator.prepare(sampleRate);
    }

    void setModel(
        const HumEstimator::Result& model,
        std::size_t sampleOffset = 0
    ) noexcept
    {
        const auto elapsedSeconds =
            static_cast<double>(sampleOffset)
            / sampleRate;

        for (
            std::size_t harmonicIndex = 0;
            harmonicIndex < HumEstimator::maxHarmonics;
            ++harmonicIndex
        )
        {
            const auto& harmonic =
                model[harmonicIndex];

            amplitudes[harmonicIndex] =
                harmonic.amplitude;

            oscillators[harmonicIndex]
                .setFrequency(
                    harmonic.frequencyHz
                );

            auto phase =
                harmonic.phase
                + harmonic.frequencyHz
                    * elapsedSeconds;

            phase =
                std::fmod(
                    phase,
                    1.0
                );

            oscillators[harmonicIndex]
                .setPhase(
                    phase
                );
        }
    }

    void reset() noexcept
    {
        for (auto& oscillator : oscillators)
            oscillator.reset();
    }

    float processSample() noexcept
    {
        float output = 0.0f;

        for (
            std::size_t harmonicIndex = 0;
            harmonicIndex < HumEstimator::maxHarmonics;
            ++harmonicIndex
        )
        {
            output +=
                amplitudes[harmonicIndex]
                * oscillators[harmonicIndex]
                    .processSample();
        }

        return output;
    }

private:
    double sampleRate = 44100.0;

    std::array<
        Oscillator,
        HumEstimator::maxHarmonics
    > oscillators;

    std::array<
        float,
        HumEstimator::maxHarmonics
    > amplitudes {};
};