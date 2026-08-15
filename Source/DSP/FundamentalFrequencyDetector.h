#pragma once

#include "HumEstimator.h"

#include <cmath>
#include <limits>

class FundamentalFrequencyDetector
{
public:
    struct Result
    {
        double frequencyHz = 0.0;

        double residualEnergy =
            std::numeric_limits<double>::infinity();

        bool valid = false;
    };

    Result detect(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate
    ) const noexcept
    {
        const auto fiftyHzResult =
            searchRange(
                buffer,
                channel,
                sampleRate,
                48.0,
                52.0
            );

        const auto sixtyHzResult =
            searchRange(
                buffer,
                channel,
                sampleRate,
                58.0,
                62.0
            );

        if (!fiftyHzResult.valid)
            return sixtyHzResult;

        if (!sixtyHzResult.valid)
            return fiftyHzResult;

        return (
            fiftyHzResult.residualEnergy
            < sixtyHzResult.residualEnergy
        )
            ? fiftyHzResult
            : sixtyHzResult;
    }

private:
    static constexpr double searchStepHz = 0.1;

    Result searchRange(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        double minimumFrequencyHz,
        double maximumFrequencyHz
    ) const noexcept
    {
        Result bestResult {};

        HumEstimator estimator;

        const auto numberOfSteps =
            static_cast<int>(
                std::round(
                    (maximumFrequencyHz - minimumFrequencyHz)
                    / searchStepHz
                )
            );

        for (
            int step = 0;
            step <= numberOfSteps;
            ++step
        )
        {
            const auto frequency =
                minimumFrequencyHz
                + static_cast<double>(step)
                    * searchStepHz;

            const auto fit =
                estimator.fit(
                    buffer,
                    channel,
                    sampleRate,
                    frequency
                );

            if (
                fit.valid
                && fit.residualEnergy
                    < bestResult.residualEnergy
            )
            {
                bestResult.frequencyHz =
                    frequency;

                bestResult.residualEnergy =
                    fit.residualEnergy;

                bestResult.valid = true;
            }
        }

        if (!bestResult.valid)
            return bestResult;

        return refineResult(
            buffer,
            channel,
            sampleRate,
            minimumFrequencyHz,
            maximumFrequencyHz,
            bestResult
        );
    }

    Result refineResult(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        double minimumFrequencyHz,
        double maximumFrequencyHz,
        const Result& coarseResult
    ) const noexcept
    {
        if (
            coarseResult.frequencyHz - searchStepHz
                < minimumFrequencyHz
            || coarseResult.frequencyHz + searchStepHz
                > maximumFrequencyHz
        )
        {
            return coarseResult;
        }

        HumEstimator estimator;

        const auto leftFit =
            estimator.fit(
                buffer,
                channel,
                sampleRate,
                coarseResult.frequencyHz
                    - searchStepHz
            );

        const auto rightFit =
            estimator.fit(
                buffer,
                channel,
                sampleRate,
                coarseResult.frequencyHz
                    + searchStepHz
            );

        if (!leftFit.valid || !rightFit.valid)
            return coarseResult;

        const auto leftResidual =
            leftFit.residualEnergy;

        const auto centerResidual =
            coarseResult.residualEnergy;

        const auto rightResidual =
            rightFit.residualEnergy;

        const auto denominator =
            leftResidual
            - 2.0 * centerResidual
            + rightResidual;

        if (denominator <= 0.0)
            return coarseResult;

        const auto offsetInSteps =
            0.5
            * (leftResidual - rightResidual)
            / denominator;

        if (std::abs(offsetInSteps) > 0.5)
            return coarseResult;

        const auto refinedFrequency =
            coarseResult.frequencyHz
            + offsetInSteps * searchStepHz;

        const auto refinedFit =
            estimator.fit(
                buffer,
                channel,
                sampleRate,
                refinedFrequency
            );

        if (
            !refinedFit.valid
            || refinedFit.residualEnergy
                > coarseResult.residualEnergy
        )
        {
            return coarseResult;
        }

        Result refinedResult {};

        refinedResult.frequencyHz =
            refinedFrequency;

        refinedResult.residualEnergy =
            refinedFit.residualEnergy;

        refinedResult.valid = true;

        return refinedResult;
    }
};