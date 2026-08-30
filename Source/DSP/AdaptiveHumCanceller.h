#pragma once

#include "HumReconstructor.h"
#include "LearnedHumModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

class AdaptiveHumCanceller
{
public:
    static constexpr double transitionDurationSeconds =
        0.02;

    void prepare(
        double newSampleRate
    ) noexcept
    {
        sampleRate =
            newSampleRate;

        for (auto& reconstructor : reconstructors)
        {
            reconstructor.prepare(
                sampleRate
            );
        }

        transitionLengthSamples =
            std::max(
                2,
                static_cast<int>(
                    std::round(
                        sampleRate
                        * transitionDurationSeconds
                    )
                )
            );

        reset();
    }

    void reset() noexcept
    {
        active = false;
        transitioning = false;

        activeIndex = 0;
        targetIndex = 1;

        transitionPosition = 0;

        for (auto& reconstructor : reconstructors)
        {
            reconstructor.reset();
        }
    }

    void activateModel(
        const LearnedHumModel& model,
        std::size_t sampleOffset
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

        reconstructors[activeIndex].setModel(
            model.harmonics,
            sampleOffset
        );

        active = true;
    }

    bool transitionToModel(
        const LearnedHumModel& model,
        std::size_t sampleOffset
    ) noexcept
    {
        if (
            !active
            || transitioning
            || !model.valid
            || !model.humDetected
        )
        {
            return false;
        }

        reconstructors[targetIndex].reset();

        reconstructors[targetIndex].setModel(
            model.harmonics,
            sampleOffset
        );

        transitionPosition = 0;
        transitioning = true;

        return true;
    }

    float processSample(
        float inputSample
    ) noexcept
    {
        if (!active)
        {
            return inputSample;
        }

        const auto activeEstimate =
            reconstructors[activeIndex]
                .processSample();

        if (!transitioning)
        {
            return inputSample
                - activeEstimate;
        }

        const auto targetEstimate =
            reconstructors[targetIndex]
                .processSample();

        const auto alpha =
            static_cast<float>(
                transitionPosition
            )
            / static_cast<float>(
                transitionLengthSamples - 1
            );

        const auto humEstimate =
            (1.0f - alpha)
                * activeEstimate
            + alpha
                * targetEstimate;

        ++transitionPosition;

        if (
            transitionPosition
            >= transitionLengthSamples
        )
        {
            std::swap(
                activeIndex,
                targetIndex
            );

            transitionPosition = 0;
            transitioning = false;
        }

        return inputSample
            - humEstimate;
    }

    bool isActive() const noexcept
    {
        return active;
    }

    bool isTransitioning() const noexcept
    {
        return transitioning;
    }

    int getTransitionLengthSamples() const noexcept
    {
        return transitionLengthSamples;
    }

private:
    double sampleRate = 44100.0;

    std::array<
        HumReconstructor,
        2
    > reconstructors;

    std::size_t activeIndex = 0;
    std::size_t targetIndex = 1;

    int transitionLengthSamples = 2;
    int transitionPosition = 0;

    bool active = false;
    bool transitioning = false;
};