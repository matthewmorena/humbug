#pragma once

#include "Oscillator.h"

#include <array>
#include <cstddef>

class HumGenerator
{
public:
    static constexpr std::size_t maxHarmonics = 8;

    HumGenerator() noexcept
    {
        // By default, generate only the fundamental.
        harmonicAmplitudes.fill(0.0f);
        harmonicAmplitudes[0] = 1.0f;

        updateFrequencies();
    }

    void prepare(double sampleRate) noexcept
    {
        for (auto& oscillator : oscillators)
            oscillator.prepare(sampleRate);

        updateFrequencies();
    }

    void reset() noexcept
    {
        for (auto& oscillator : oscillators)
            oscillator.reset();
    }

    void setFundamentalFrequency(double frequencyHz) noexcept
    {
        fundamentalFrequencyHz = frequencyHz;
        updateFrequencies();
    }

    void setHarmonicAmplitude(
        std::size_t harmonicNumber,
        float amplitude
    ) noexcept
    {
        if (
            harmonicNumber == 0
            || harmonicNumber > maxHarmonics
        )
        {
            return;
        }

        harmonicAmplitudes[harmonicNumber - 1] =
            amplitude;
    }

    void setHarmonicPhase(
        std::size_t harmonicNumber,
        double phase
    ) noexcept
    {
        if (
            harmonicNumber == 0
            || harmonicNumber > maxHarmonics
        )
        {
            return;
        }

        oscillators[harmonicNumber - 1]
            .setPhase(phase);
    }

    void clearHarmonics() noexcept
    {
        harmonicAmplitudes.fill(0.0f);
    }

    float processSample() noexcept
    {
        float output = 0.0f;

        for (
            std::size_t i = 0;
            i < maxHarmonics;
            ++i
        )
        {
            output +=
                harmonicAmplitudes[i]
                * oscillators[i].processSample();
        }

        return output;
    }

private:
    void updateFrequencies() noexcept
    {
        for (
            std::size_t i = 0;
            i < maxHarmonics;
            ++i
        )
        {
            const auto harmonicNumber =
                static_cast<double>(i + 1);

            oscillators[i].setFrequency(
                fundamentalFrequencyHz
                * harmonicNumber
            );
        }
    }

    double fundamentalFrequencyHz = 60.0;

    std::array<Oscillator, maxHarmonics>
        oscillators;

    std::array<float, maxHarmonics>
        harmonicAmplitudes;
};