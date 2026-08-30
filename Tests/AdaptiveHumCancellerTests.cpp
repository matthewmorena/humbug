#include <JuceHeader.h>

#include "../Source/DSP/AdaptiveHumCanceller.h"
#include "../Source/DSP/FixedHumCanceller.h"
#include "../Source/DSP/HumAnalyzer.h"
#include "../Source/DSP/HumGenerator.h"

class AdaptiveHumCancellerTests final
    : public juce::UnitTest
{
public:
    AdaptiveHumCancellerTests()
        : juce::UnitTest(
            "Adaptive Hum Canceller",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testPassesThroughExactlyWhenInactive();
        testCrossfadesBetweenPhaseAlignedModels();
        testRejectsInvalidUpdatesWithoutChangingActiveModel();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    static constexpr int analysisSamples =
        12000;

    LearnedHumModel createModel(
        double frequencyHz,
        float amplitudeScale,
        double phaseOffset
    )
    {
        HumGenerator generator;

        generator.setFundamentalFrequency(
            frequencyHz
        );

        generator.prepare(
            sampleRate
        );

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f * amplitudeScale
        );

        generator.setHarmonicAmplitude(
            2,
            0.15f * amplitudeScale
        );

        generator.setHarmonicAmplitude(
            3,
            0.08f * amplitudeScale
        );

        generator.setHarmonicPhase(
            1,
            0.18 + phaseOffset
        );

        generator.setHarmonicPhase(
            2,
            0.25 + phaseOffset
        );

        generator.setHarmonicPhase(
            3,
            0.41 + phaseOffset
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            analysisSamples
        );

        auto* data =
            buffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            data[sample] =
                generator.processSample();
        }

        HumAnalyzer analyzer;

        return analyzer.analyze(
            buffer,
            0,
            sampleRate
        );
    }

    void testPassesThroughExactlyWhenInactive()
    {
        beginTest(
            "Inactive adaptive canceller is exact pass-through"
        );

        AdaptiveHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        expect(
            !canceller.isActive()
        );

        constexpr std::array<float, 8> inputSamples {
            0.0f,
            0.25f,
            -0.25f,
            0.75f,
            -0.75f,
            1.0f,
            -1.0f,
            0.123456f
        };

        for (const auto inputSample : inputSamples)
        {
            const auto outputSample =
                canceller.processSample(
                    inputSample
                );

            expectEquals(
                outputSample,
                inputSample
            );
        }
    }

    void testCrossfadesBetweenPhaseAlignedModels()
    {
        beginTest(
            "Crossfades between phase-aligned models"
        );

        const auto oldModel =
            createModel(
                60.0,
                1.0f,
                0.0
            );

        const auto newModel =
            createModel(
                60.08,
                0.8f,
                0.07
            );

        expect(oldModel.valid);
        expect(oldModel.humDetected);

        expect(newModel.valid);
        expect(newModel.humDetected);

        AdaptiveHumCanceller adaptive;

        FixedHumCanceller oldReference;
        FixedHumCanceller newReference;

        adaptive.prepare(
            sampleRate
        );

        oldReference.prepare(
            sampleRate
        );

        newReference.prepare(
            sampleRate
        );

        constexpr std::size_t
            initialActivationOffset =
                analysisSamples;

        adaptive.activateModel(
            oldModel,
            initialActivationOffset
        );

        oldReference.activateModel(
            oldModel,
            initialActivationOffset
        );

        expect(
            adaptive.isActive()
        );

        constexpr int
            samplesBeforeTransition =
                4096;

        for (
            int sample = 0;
            sample < samplesBeforeTransition;
            ++sample
        )
        {
            const auto adaptiveOutput =
                adaptive.processSample(
                    0.0f
                );

            const auto referenceOutput =
                oldReference.processSample(
                    0.0f
                );

            expectWithinAbsoluteError(
                adaptiveOutput,
                referenceOutput,
                1.0e-6f
            );
        }

        const auto transitionActivationOffset =
            initialActivationOffset
            + static_cast<std::size_t>(
                samplesBeforeTransition
            );

        newReference.activateModel(
            newModel,
            transitionActivationOffset
        );

        expect(
            adaptive.transitionToModel(
                newModel,
                transitionActivationOffset
            )
        );

        expect(
            adaptive.isTransitioning()
        );

        const auto transitionSamples =
            adaptive.getTransitionLengthSamples();

        for (
            int sample = 0;
            sample < transitionSamples;
            ++sample
        )
        {
            const auto oldOutput =
                oldReference.processSample(
                    0.0f
                );

            const auto newOutput =
                newReference.processSample(
                    0.0f
                );

            const auto adaptiveOutput =
                adaptive.processSample(
                    0.0f
                );

            const auto alpha =
                static_cast<float>(
                    sample
                )
                / static_cast<float>(
                    transitionSamples - 1
                );

            const auto expectedOutput =
                (1.0f - alpha)
                    * oldOutput
                + alpha
                    * newOutput;

            expectWithinAbsoluteError(
                adaptiveOutput,
                expectedOutput,
                1.0e-5f
            );
        }

        expect(
            !adaptive.isTransitioning()
        );

        // Once the transition has completed,
        // the new model must continue as the
        // sole active reconstruction.
        for (
            int sample = 0;
            sample < 1024;
            ++sample
        )
        {
            const auto adaptiveOutput =
                adaptive.processSample(
                    0.0f
                );

            const auto referenceOutput =
                newReference.processSample(
                    0.0f
                );

            expectWithinAbsoluteError(
                adaptiveOutput,
                referenceOutput,
                1.0e-5f
            );
        }
    }

    void testRejectsInvalidUpdatesWithoutChangingActiveModel()
    {
        beginTest(
            "Rejects invalid updates without changing active model"
        );

        const auto activeModel =
            createModel(
                60.0,
                1.0f,
                0.0
            );

        expect(
            activeModel.valid
        );

        expect(
            activeModel.humDetected
        );

        AdaptiveHumCanceller adaptive;
        FixedHumCanceller reference;

        adaptive.prepare(
            sampleRate
        );

        reference.prepare(
            sampleRate
        );

        constexpr std::size_t activationOffset =
            analysisSamples;

        adaptive.activateModel(
            activeModel,
            activationOffset
        );

        reference.activateModel(
            activeModel,
            activationOffset
        );

        expect(
            adaptive.isActive()
        );

        constexpr int samplesBeforeUpdate =
            4096;

        // Advance both reconstructors to the same
        // point on their oscillator timelines.
        for (
            int sample = 0;
            sample < samplesBeforeUpdate;
            ++sample
        )
        {
            const auto adaptiveOutput =
                adaptive.processSample(
                    0.0f
                );

            const auto referenceOutput =
                reference.processSample(
                    0.0f
                );

            expectWithinAbsoluteError(
                adaptiveOutput,
                referenceOutput,
                1.0e-6f
            );
        }

        LearnedHumModel invalidModel;

        expect(
            !invalidModel.valid
        );

        expect(
            !adaptive.transitionToModel(
                invalidModel,
                activationOffset
                    + samplesBeforeUpdate
            )
        );

        expect(
            adaptive.isActive()
        );

        expect(
            !adaptive.isTransitioning()
        );

        // Rejecting the invalid update must not have
        // disturbed the active reconstruction.
        for (
            int sample = 0;
            sample < 1024;
            ++sample
        )
        {
            const auto adaptiveOutput =
                adaptive.processSample(
                    0.0f
                );

            const auto referenceOutput =
                reference.processSample(
                    0.0f
                );

            expectWithinAbsoluteError(
                adaptiveOutput,
                referenceOutput,
                1.0e-6f
            );
        }

        // Construct a model that is structurally valid
        // but explicitly classified as no hum.
        auto noHumModel =
            activeModel;

        noHumModel.humDetected =
            false;

        expect(
            noHumModel.valid
        );

        expect(
            !noHumModel.humDetected
        );

        expect(
            !adaptive.transitionToModel(
                noHumModel,
                activationOffset
                    + samplesBeforeUpdate
                    + 1024
            )
        );

        expect(
            adaptive.isActive()
        );

        expect(
            !adaptive.isTransitioning()
        );

        // The active model must again continue from
        // exactly where it was before the rejected
        // tracking update.
        for (
            int sample = 0;
            sample < 1024;
            ++sample
        )
        {
            const auto adaptiveOutput =
                adaptive.processSample(
                    0.0f
                );

            const auto referenceOutput =
                reference.processSample(
                    0.0f
                );

            expectWithinAbsoluteError(
                adaptiveOutput,
                referenceOutput,
                1.0e-6f
            );
        }
    }
};

static AdaptiveHumCancellerTests
    adaptiveHumCancellerTests;