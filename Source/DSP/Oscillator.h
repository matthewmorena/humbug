#pragma once

#include <cmath>
#include <numbers>

class Oscillator
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        updatePhaseIncrement();
    }

    void setFrequency(double newFrequencyHz) noexcept
    {
        frequencyHz = newFrequencyHz;
        updatePhaseIncrement();
    }

    void setPhase(double newPhase) noexcept
    {
        initialPhase = wrapPhase(newPhase);
        phase = initialPhase;
    }

    void reset() noexcept
    {
        phase = initialPhase;
    }

    static double wrapPhase(double value) noexcept
    {
        while (value >= 1.0)
            value -= 1.0;

        while (value < 0.0)
            value += 1.0;

        return value;
    }

    float processSample() noexcept
    {
        const auto sample = static_cast<float>(
            std::sin(
                2.0
                * std::numbers::pi
                * phase
            )
        );

        phase += phaseIncrement;

        if (phase >= 1.0)
            phase -= 1.0;

        return sample;
    }

private:
    void updatePhaseIncrement() noexcept
    {
        if (sampleRate > 0.0)
            phaseIncrement = frequencyHz / sampleRate;
        else
            phaseIncrement = 0.0;
    }

    double sampleRate = 0.0;
    double frequencyHz = 0.0;

    // Normalized phase:
    // 0.0 = 0 degrees
    // 0.25 = 90 degrees
    // 0.5 = 180 degrees
    // 0.75 = 270 degrees
    double phase = 0.0;

    double phaseIncrement = 0.0;

    double initialPhase = 0.0;
};