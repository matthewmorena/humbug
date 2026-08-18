#include <JuceHeader.h>

#include "../Source/DSP/FixedHumCanceller.h"
#include "../Source/DSP/HumGenerator.h"

#include <cmath>
#include <numbers>

class FixedHumCancellerTests final
    : public juce::UnitTest
{
public:
    FixedHumCancellerTests()
        : juce::UnitTest(
            "Fixed Hum Canceller",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testLearnsAndCancelsHumFromMixedSignal();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testLearnsAndCancelsHumFromMixedSignal()
    {
        beginTest(
            "Canceller learns and removes hum from mixed signal"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(
            sampleRate
        );

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f
        );

        generator.setHarmonicAmplitude(
            2,
            0.15f
        );

        generator.setHarmonicAmplitude(
            3,
            0.08f
        );

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicPhase(
            2,
            0.25
        );

        generator.setHarmonicPhase(
            3,
            0.41
        );

        generator.reset();

        constexpr int analysisSamples =
            12000;

        constexpr int cancellationSamples =
            2000;

        constexpr int totalSamples =
            analysisSamples
            + cancellationSamples;

        juce::AudioBuffer<float> humBuffer(
            1,
            totalSamples
        );

        humBuffer.clear();

        generator.addToBuffer(
            humBuffer
        );

        constexpr double cleanFrequencyHz =
            997.0;

        constexpr float cleanAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        juce::AudioBuffer<float> mixedBuffer(
            1,
            totalSamples
        );

        mixedBuffer.clear();

        const auto* humSamples =
            humBuffer.getReadPointer(0);

        auto* mixedSamples =
            mixedBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < totalSamples;
            ++sample
        )
        {
            const auto time =
                static_cast<double>(sample)
                / sampleRate;

            const auto cleanSample =
                cleanAmplitude
                * static_cast<float>(
                    std::sin(
                        twoPi
                        * cleanFrequencyHz
                        * time
                    )
                );

            mixedSamples[sample] =
                cleanSample
                + humSamples[sample];
        }

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        analysisBuffer.copyFrom(
            0,
            0,
            mixedBuffer,
            0,
            0,
            analysisSamples
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        const auto learnResult =
            canceller.learn(
                analysisBuffer,
                0
            );

        expect(
            learnResult.valid
        );

        expect(
            learnResult.humDetected
        );

        expect(
            canceller.isActive()
        );

        expectWithinAbsoluteError(
            learnResult.frequencyHz,
            60.0,
            0.01
        );

        double errorEnergyBefore = 0.0;
        double errorEnergyAfter = 0.0;

        for (
            int sample = 0;
            sample < cancellationSamples;
            ++sample
        )
        {
            const auto sourceIndex =
                analysisSamples
                + sample;

            const auto time =
                static_cast<double>(
                    sourceIndex
                )
                / sampleRate;

            const auto cleanSample =
                cleanAmplitude
                * static_cast<float>(
                    std::sin(
                        twoPi
                        * cleanFrequencyHz
                        * time
                    )
                );

            const auto mixedSample =
                mixedSamples[sourceIndex];

            const auto outputSample =
                canceller.processSample(
                    mixedSample
                );

            const auto errorBefore =
                mixedSample
                - cleanSample;

            const auto errorAfter =
                outputSample
                - cleanSample;

            errorEnergyBefore +=
                static_cast<double>(
                    errorBefore
                )
                * errorBefore;

            errorEnergyAfter +=
                static_cast<double>(
                    errorAfter
                )
                * errorAfter;
        }

        const auto rmsBefore =
            std::sqrt(
                errorEnergyBefore
                / static_cast<double>(
                    cancellationSamples
                )
            );

        const auto rmsAfter =
            std::sqrt(
                errorEnergyAfter
                / static_cast<double>(
                    cancellationSamples
                )
            );

        expect(
            rmsBefore > 0.0
        );

        if (rmsAfter > 0.0)
        {
            const auto attenuationDb =
                20.0
                * std::log10(
                    rmsAfter
                    / rmsBefore
                );

            expect(
                attenuationDb < -40.0
            );
        }
    }
};

static FixedHumCancellerTests
    fixedHumCancellerTests;