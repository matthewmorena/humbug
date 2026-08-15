#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "LinearSystemSolver.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <algorithm>
#include <limits>

class HumEstimator
{
public:
    static constexpr std::size_t maxHarmonics = 8;

    static constexpr std::size_t numCoefficients = maxHarmonics * 2;

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

    struct FitResult
    {
        Result harmonics {};
        double residualEnergy =
            std::numeric_limits<double>::infinity();
        bool valid = false;
    };

    FitResult fit(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        double fundamentalFrequencyHz
    ) const noexcept
    {
        FitResult fitResult {};
        auto& result = fitResult.harmonics;

        if (
            channel < 0
            || channel >= buffer.getNumChannels()
            || buffer.getNumSamples() < static_cast<int>(numCoefficients)
            || sampleRate <= 0.0
            || fundamentalFrequencyHz <= 0.0
        )
        {
            return fitResult;
        }

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        // Fill in the frequency metadata first.
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

            result[harmonicIndex].frequencyHz =
                fundamentalFrequencyHz
                * harmonicNumber;
        }

        // For now, require all configured harmonics
        // to fall below Nyquist.
        if (
            result[maxHarmonics - 1].frequencyHz
            >= sampleRate * 0.5
        )
        {
            return fitResult;
        }

        Matrix<numCoefficients> normalMatrix {};
        Vector<numCoefficients> rightHandSide {};

        double inputEnergy = 0.0;

        const auto* samples =
            buffer.getReadPointer(channel);

        const auto numSamples =
            buffer.getNumSamples();

        const auto fundamentalPhaseIncrement =
            twoPi
            * fundamentalFrequencyHz
            / sampleRate;

        for (
            int sample = 0;
            sample < numSamples;
            ++sample
        )
        {
            Vector<numCoefficients> basis {};

            const auto fundamentalAngle =
                fundamentalPhaseIncrement
                * static_cast<double>(sample);

            // Construct one row of A.
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

                const auto angle =
                    fundamentalAngle
                    * harmonicNumber;

                const auto sineIndex =
                    harmonicIndex * 2;

                const auto cosineIndex =
                    sineIndex + 1;

                basis[sineIndex] =
                    std::sin(angle);

                basis[cosineIndex] =
                    std::cos(angle);
            }

            const auto inputSample =
                static_cast<double>(
                    samples[sample]
                );

            inputEnergy +=
                inputSample * inputSample;

            // Accumulate A^T x.
            for (
                std::size_t row = 0;
                row < numCoefficients;
                ++row
            )
            {
                rightHandSide[row] +=
                    basis[row]
                    * inputSample;
            }

            // Accumulate A^T A.
            for (
                std::size_t row = 0;
                row < numCoefficients;
                ++row
            )
            {
                for (
                    std::size_t column = 0;
                    column < numCoefficients;
                    ++column
                )
                {
                    normalMatrix[row][column] +=
                        basis[row]
                        * basis[column];
                }
            }
        }

        Vector<numCoefficients> coefficients {};

        const auto solved =
            solveLinearSystem(
                normalMatrix,
                rightHandSide,
                coefficients
            );

        if (!solved)
            return fitResult;

        // Convert sine/cosine coefficients back
        // into amplitude and normalized phase.
        for (
            std::size_t harmonicIndex = 0;
            harmonicIndex < maxHarmonics;
            ++harmonicIndex
        )
        {
            const auto sineCoefficient =
                coefficients[
                    harmonicIndex * 2
                ];

            const auto cosineCoefficient =
                coefficients[
                    harmonicIndex * 2 + 1
                ];

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

            result[harmonicIndex].amplitude =
                static_cast<float>(
                    amplitude
                );

            result[harmonicIndex].phase =
                phaseRadians / twoPi;
        }

        double explainedEnergy = 0.0;

        for (
            std::size_t coefficientIndex = 0;
            coefficientIndex < numCoefficients;
            ++coefficientIndex
        )
        {
            explainedEnergy +=
                coefficients[coefficientIndex]
                * rightHandSide[coefficientIndex];
        }

        fitResult.residualEnergy =
            std::max(
                0.0,
                inputEnergy - explainedEnergy
            );

        fitResult.valid = true;

        return fitResult;
    }

    Result estimate(
        const juce::AudioBuffer<float>& buffer,
        int channel,
        double sampleRate,
        double fundamentalFrequencyHz
    ) const noexcept
    {
        return fit(
            buffer,
            channel,
            sampleRate,
            fundamentalFrequencyHz
        ).harmonics;
    }
};