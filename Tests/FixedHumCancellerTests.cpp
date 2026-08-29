#include <JuceHeader.h>

#include "../Source/DSP/FixedHumCanceller.h"
#include "../Source/DSP/HumGenerator.h"
#include "../Source/DSP/HumAnalyzer.h"

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
        testActivatesAndCancelsHumFromMixedSignal();
        testDelayedActivationRemainsPhaseAligned();
        testFixedModelLosesAlignmentUnderFrequencyDrift();
        testPassesThroughWhenNoHumDetected();
        testResetDisablesCancellation();
        testTrackedModelRemainsPhaseAlignedAfterDelayedActivation();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testActivatesAndCancelsHumFromMixedSignal()
    {
        beginTest(
            "Canceller activates learned model and removes hum"
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

        HumAnalyzer analyzer;

        const auto model =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            model.valid
        );

        expect(
            model.humDetected
        );

        expectWithinAbsoluteError(
            model.frequencyHz,
            60.0,
            0.01
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        canceller.activateModel(
            model,
            analysisSamples
        );

        expect(
            canceller.isActive()
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

    void testDelayedActivationRemainsPhaseAligned()
    {
        beginTest(
            "Delayed model activation remains phase aligned"
        );

        constexpr int analysisSamples =
            12000;

        constexpr int analysisDelaySamples =
            317;

        constexpr int cancellationSamples =
            2000;

        constexpr int activationSample =
            analysisSamples
            + analysisDelaySamples;

        constexpr int totalSamples =
            activationSample
            + cancellationSamples;

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

        HumAnalyzer analyzer;

        const auto model =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            model.valid
        );

        expect(
            model.humDetected
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        constexpr std::uint64_t sampleOffset =
            activationSample;

        canceller.activateModel(
            model,
            sampleOffset
        );

        expect(
            canceller.isActive()
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
                activationSample
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

    void testFixedModelLosesAlignmentUnderFrequencyDrift()
    {
        beginTest(
            "Fixed model loses alignment under frequency drift"
        );

        constexpr int analysisSamples =
            12000;

        constexpr int driftSamples =
            240000;

        constexpr int measurementSamples =
            12000;

        constexpr double startFrequencyHz =
            60.0;

        constexpr double endFrequencyHz =
            60.2;

        HumGenerator generator;

        generator.setFundamentalFrequency(
            startFrequencyHz
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

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        auto* analysisData =
            analysisBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            analysisData[sample] =
                generator.processSample();
        }

        HumAnalyzer analyzer;

        const auto model =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            model.valid
        );

        expect(
            model.humDetected
        );

        expectWithinAbsoluteError(
            model.frequencyHz,
            startFrequencyHz,
            0.01
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        canceller.activateModel(
            model,
            analysisSamples
        );

        expect(
            canceller.isActive()
        );

        double earlyInputEnergy = 0.0;
        double earlyOutputEnergy = 0.0;

        double lateInputEnergy = 0.0;
        double lateOutputEnergy = 0.0;

        for (
            int sample = 0;
            sample < driftSamples;
            ++sample
        )
        {
            const auto progress =
                static_cast<double>(sample)
                / static_cast<double>(
                    driftSamples - 1
                );

            const auto frequencyHz =
                startFrequencyHz
                + progress
                * (
                    endFrequencyHz
                    - startFrequencyHz
                );

            generator.setFundamentalFrequency(
                frequencyHz
            );

            const auto inputSample =
                generator.processSample();

            const auto outputSample =
                canceller.processSample(
                    inputSample
                );

            if (sample < measurementSamples)
            {
                earlyInputEnergy +=
                    static_cast<double>(
                        inputSample
                    )
                    * inputSample;

                earlyOutputEnergy +=
                    static_cast<double>(
                        outputSample
                    )
                    * outputSample;
            }

            if (
                sample
                >= driftSamples
                    - measurementSamples
            )
            {
                lateInputEnergy +=
                    static_cast<double>(
                        inputSample
                    )
                    * inputSample;

                lateOutputEnergy +=
                    static_cast<double>(
                        outputSample
                    )
                    * outputSample;
            }
        }

        const auto calculateAttenuationDb =
            [](
                double inputEnergy,
                double outputEnergy,
                int numSamples
            )
            {
                const auto rmsInput =
                    std::sqrt(
                        inputEnergy
                        / static_cast<double>(
                            numSamples
                        )
                    );

                const auto rmsOutput =
                    std::sqrt(
                        outputEnergy
                        / static_cast<double>(
                            numSamples
                        )
                    );

                return 20.0
                    * std::log10(
                        rmsOutput
                        / rmsInput
                    );
            };

        const auto earlyAttenuationDb =
            calculateAttenuationDb(
                earlyInputEnergy,
                earlyOutputEnergy,
                measurementSamples
            );

        const auto lateAttenuationDb =
            calculateAttenuationDb(
                lateInputEnergy,
                lateOutputEnergy,
                measurementSamples
            );

        expect(
            earlyAttenuationDb < -35.0
        );

        expect(
            lateAttenuationDb > -10.0
        );

        expect(
            lateAttenuationDb
            - earlyAttenuationDb
            > 25.0
        );
    }

    void testPassesThroughWhenNoHumDetected()
    {
        beginTest(
            "Canceller remains inactive when no hum is detected"
        );

        constexpr int analysisSamples =
            12000;

        constexpr double signalFrequencyHz =
            997.0;

        constexpr float signalAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        auto* samples =
            analysisBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            const auto time =
                static_cast<double>(sample)
                / sampleRate;

            samples[sample] =
                signalAmplitude
                * static_cast<float>(
                    std::sin(
                        twoPi
                        * signalFrequencyHz
                        * time
                    )
                );
        }

        HumAnalyzer analyzer;

        const auto model =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            model.valid
        );

        expect(
            !model.humDetected
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        canceller.activateModel(
            model,
            analysisSamples
        );

        expect(
            !canceller.isActive()
        );

        constexpr float inputSample =
            0.375f;

        const auto outputSample =
            canceller.processSample(
                inputSample
            );

        expectEquals(
            outputSample,
            inputSample
        );
    }

    void testResetDisablesCancellation()
    {
        beginTest(
            "Reset disables active cancellation"
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

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicPhase(
            2,
            0.25
        );

        generator.reset();

        constexpr int analysisSamples =
            12000;

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        analysisBuffer.clear();

        generator.addToBuffer(
            analysisBuffer
        );

        HumAnalyzer analyzer;

        const auto model =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            model.valid
        );

        expect(
            model.humDetected
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        canceller.activateModel(
            model,
            analysisSamples
        );

        expect(
            canceller.isActive()
        );

        canceller.reset();

        expect(
            !canceller.isActive()
        );

        constexpr float inputSample =
            0.375f;

        expectEquals(
            canceller.processSample(
                inputSample
            ),
            inputSample
        );
    }

    void testTrackedModelRemainsPhaseAlignedAfterDelayedActivation()
    {
        beginTest(
            "Tracked model remains phase aligned after delayed activation"
        );

        constexpr int analysisStartSample =
            10000;

        constexpr int analysisSamples =
            12000;

        constexpr int workerDelaySamples =
            36000;

        constexpr int measurementSamples =
            12000;

        constexpr double frequencyHz =
            60.07;

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

        // Advance the continuous source so the tracking
        // window does not begin at absolute sample zero.
        for (
            int sample = 0;
            sample < analysisStartSample;
            ++sample
        )
        {
            generator.processSample();
        }

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        auto* analysisData =
            analysisBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            analysisData[sample] =
                generator.processSample();
        }

        HumAnalyzer analyzer;

        const auto model =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(model.valid);
        expect(model.humDetected);

        // Simulate time spent on background analysis.
        for (
            int sample = 0;
            sample < workerDelaySamples;
            ++sample
        )
        {
            generator.processSample();
        }

        const auto activationSample =
            analysisStartSample
            + analysisSamples
            + workerDelaySamples;

        const auto sampleOffset =
            activationSample
            - analysisStartSample;

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        canceller.activateModel(
            model,
            sampleOffset
        );

        expect(canceller.isActive());

        double inputEnergy = 0.0;
        double outputEnergy = 0.0;

        for (
            int sample = 0;
            sample < measurementSamples;
            ++sample
        )
        {
            const auto inputSample =
                generator.processSample();

            const auto outputSample =
                canceller.processSample(
                    inputSample
                );

            inputEnergy +=
                static_cast<double>(
                    inputSample
                )
                * inputSample;

            outputEnergy +=
                static_cast<double>(
                    outputSample
                )
                * outputSample;
        }

        const auto inputRms =
            std::sqrt(
                inputEnergy
                / static_cast<double>(
                    measurementSamples
                )
            );

        const auto outputRms =
            std::sqrt(
                outputEnergy
                / static_cast<double>(
                    measurementSamples
                )
            );

        const auto attenuationDb =
            20.0
            * std::log10(
                outputRms
                / inputRms
            );

        expect(
            attenuationDb < -40.0
        );
    }
};

static FixedHumCancellerTests
    fixedHumCancellerTests;