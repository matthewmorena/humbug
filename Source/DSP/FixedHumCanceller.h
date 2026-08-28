#pragma once

#include <cstdint>

#include "HumReconstructor.h"
#include "LearnedHumModel.h"

class FixedHumCanceller
{
public:
    
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

    void activateModel(
        const LearnedHumModel& model,
        std::uint64_t sampleOffset
    ) noexcept
    {
        reset();

        if (
            !model.valid
            || !model.humDetected
        )
        {
            return;
        }

        reconstructor.setModel(
            model.harmonics,
            sampleOffset
        );

        active = true;
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
    
    HumReconstructor reconstructor;
};