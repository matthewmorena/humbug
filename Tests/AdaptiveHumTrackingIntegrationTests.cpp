#include <JuceHeader.h>

#include "../Source/DSP/AdaptiveHumCanceller.h"
#include "../Source/DSP/FixedHumCanceller.h"
#include "../Source/DSP/HumAnalyzer.h"
#include "../Source/DSP/HumGenerator.h"
#include "../Source/DSP/HumTrackingController.h"
#include "../Source/DSP/HumTrackingWorker.h"
#include "../Source/DSP/LearnedHumModelMailbox.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class AdaptiveHumTrackingIntegrationTests final
    : public juce::UnitTest
{
public:
    AdaptiveHumTrackingIntegrationTests()
        : juce::UnitTest(
            "Adaptive Hum Tracking Integration",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testAdaptiveTrackingMaintainsCancellationDuringFrequencyDrift();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testAdaptiveTrackingMaintainsCancellationDuringFrequencyDrift()
    {
        beginTest(
            "Adaptive tracking maintains cancellation during frequency drift"
        );

        constexpr int analysisSamples =
            12000;

        constexpr int driftSamples =
            240000;

        constexpr int measurementSamples =
            12000;

        constexpr int hostBlockSize =
            512;

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

        // ------------------------------------------------
        // Initial fixed Learn
        // ------------------------------------------------

        juce::AudioBuffer<float> initialAnalysisBuffer(
            1,
            analysisSamples
        );

        auto* initialAnalysisData =
            initialAnalysisBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            initialAnalysisData[sample] =
                generator.processSample();
        }

        HumAnalyzer analyzer;

        const auto initialModel =
            analyzer.analyze(
                initialAnalysisBuffer,
                0,
                sampleRate
            );

        expect(
            initialModel.valid
        );

        expect(
            initialModel.humDetected
        );

        expectWithinAbsoluteError(
            initialModel.frequencyHz,
            startFrequencyHz,
            0.01
        );

        // ------------------------------------------------
        // Tracking infrastructure
        // ------------------------------------------------

        HumTrackingController trackingController;

        trackingController.prepare(
            sampleRate,
            1
        );

        // Advance the tracking controller's absolute
        // timeline through the initial Learn window.
        //
        // Tracking is still inactive, so this does
        // not capture anything.
        trackingController.processBlock(
            initialAnalysisBuffer
        );

        expect(
            trackingController.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    analysisSamples
                )
        );

        LearnedHumModelMailbox trackingMailbox;

        HumTrackingWorker trackingWorker(
            trackingController,
            trackingMailbox
        );

        trackingWorker.prepare(
            sampleRate,
            0
        );

        expect(
            trackingWorker.start()
        );

        expect(
            trackingController.startTracking()
        );

        // ------------------------------------------------
        // Fixed and adaptive cancellation begin from
        // the exact same learned model and timeline.
        // ------------------------------------------------

        FixedHumCanceller fixedCanceller;

        fixedCanceller.prepare(
            sampleRate
        );

        fixedCanceller.activateModel(
            initialModel,
            analysisSamples
        );

        AdaptiveHumCanceller adaptiveCanceller;

        adaptiveCanceller.prepare(
            sampleRate
        );

        adaptiveCanceller.activateModel(
            initialModel,
            analysisSamples
        );

        expect(
            fixedCanceller.isActive()
        );

        expect(
            adaptiveCanceller.isActive()
        );

        double lateInputEnergy = 0.0;
        double lateFixedOutputEnergy = 0.0;
        double lateAdaptiveOutputEnergy = 0.0;

        int driftPosition = 0;

        int trackingUpdatesApplied = 0;

        while (
            driftPosition < driftSamples
        )
        {
            // Consume completed tracking updates only
            // at the beginning of a host block.
            if (
                trackingController.isModelReady()
            )
            {
                PendingLearnResult result;

                expect(
                    trackingMailbox.tryConsume(
                        result
                    )
                );

                const auto activationSample =
                    trackingController
                        .getCurrentSamplePosition();

                expect(
                    activationSample
                        >= result.analysisStartSample
                );

                const auto sampleOffset =
                    activationSample
                    - result.analysisStartSample;

                const auto updateApplied =
                    adaptiveCanceller.transitionToModel(
                        result.model,
                        static_cast<std::size_t>(
                            sampleOffset
                        )
                    );

                expect(
                    updateApplied
                );

                if (updateApplied)
                {
                    ++trackingUpdatesApplied;
                }

                trackingController
                    .finishModelHandoff();
            }

            const auto samplesThisBlock =
                std::min(
                    hostBlockSize,
                    driftSamples
                        - driftPosition
                );

            juce::AudioBuffer<float> block(
                1,
                samplesThisBlock
            );

            auto* blockData =
                block.getWritePointer(0);

            for (
                int sample = 0;
                sample < samplesThisBlock;
                ++sample
            )
            {
                const auto absoluteDriftSample =
                    driftPosition + sample;

                const auto progress =
                    static_cast<double>(
                        absoluteDriftSample
                    )
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

                blockData[sample] =
                    generator.processSample();
            }

            // IMPORTANT:
            //
            // Tracking sees the RAW source before
            // either canceller modifies it.
            trackingController.processBlock(
                block
            );

            for (
                int sample = 0;
                sample < samplesThisBlock;
                ++sample
            )
            {
                const auto inputSample =
                    blockData[sample];

                const auto fixedOutput =
                    fixedCanceller.processSample(
                        inputSample
                    );

                const auto adaptiveOutput =
                    adaptiveCanceller.processSample(
                        inputSample
                    );

                const auto absoluteDriftSample =
                    driftPosition + sample;

                if (
                    absoluteDriftSample
                    >= driftSamples
                        - measurementSamples
                )
                {
                    lateInputEnergy +=
                        static_cast<double>(
                            inputSample
                        )
                        * inputSample;

                    lateFixedOutputEnergy +=
                        static_cast<double>(
                            fixedOutput
                        )
                        * fixedOutput;

                    lateAdaptiveOutputEnergy +=
                        static_cast<double>(
                            adaptiveOutput
                        )
                        * adaptiveOutput;
                }
            }

            driftPosition +=
                samplesThisBlock;

            // Unit tests can simulate five seconds of
            // audio much faster than the worker can
            // execute in wall-clock time.
            //
            // When a capture has completed, allow the
            // background analysis to finish before
            // advancing the simulated audio timeline.
            if (
                trackingController.isReadyForAnalysis()
                || trackingController.isAnalyzing()
            )
            {
                constexpr double timeoutMs =
                    5000.0;

                const auto startTime =
                    juce::Time::
                        getMillisecondCounterHiRes();

                while (
                    !trackingController.isModelReady()
                    && (
                        juce::Time::
                            getMillisecondCounterHiRes()
                        - startTime
                    ) < timeoutMs
                )
                {
                    juce::Thread::sleep(
                        1
                    );
                }

                expect(
                    trackingController.isModelReady()
                );
            }
        }

        trackingWorker.stop();

        const auto calculateAttenuationDb =
            [](
                double inputEnergy,
                double outputEnergy
            )
            {
                return 10.0
                    * std::log10(
                        outputEnergy
                        / inputEnergy
                    );
            };

        const auto fixedAttenuationDb =
            calculateAttenuationDb(
                lateInputEnergy,
                lateFixedOutputEnergy
            );

        const auto adaptiveAttenuationDb =
            calculateAttenuationDb(
                lateInputEnergy,
                lateAdaptiveOutputEnergy
            );

        expect(
            trackingUpdatesApplied >= 3
        );

        // The fixed model should exhibit the same
        // severe late-drift degradation established
        // by the earlier regression.
        expect(
            fixedAttenuationDb > -10.0
        );

        // Adaptive tracking should still provide
        // meaningful cancellation near the end
        // of the drift.
        expect(
            adaptiveAttenuationDb < -15.0
        );

        // Adaptive tracking should substantially
        // outperform the fixed model.
        expect(
            adaptiveAttenuationDb
                < fixedAttenuationDb - 20.0
        );
    }
};

static AdaptiveHumTrackingIntegrationTests
    adaptiveHumTrackingIntegrationTests;