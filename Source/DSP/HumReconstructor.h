#pragma once

#include "HumEstimator.h"
#include "Oscillator.h"

#include <array>
#include <cstddef>

class HumReconstructor
{
public:
    void prepare(double sampleRate) noexcept
    {
        for (auto& oscillator : oscillators)
            oscillator.prepare(sampleRate);
    }

    void setModel(
        const HumEstimator::Result& model
    ) noexcept
    {
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

            oscillators[harmonicIndex]
                .setPhase(
                    harmonic.phase
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
    std::array<
        Oscillator,
        HumEstimator::maxHarmonics
    > oscillators;

    std::array<
        float,
        HumEstimator::maxHarmonics
    > amplitudes {};
};