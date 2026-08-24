#include <JuceHeader.h>

#include "../Source/DSP/FixedHumCanceller.h"
#include "../Source/DSP/HumAnalysisWorker.h"
#include "../Source/DSP/HumGenerator.h"
#include "../Source/DSP/LearnedHumModelMailbox.h"
#include "../Source/DSP/LearnModeController.h"

#include <cmath>
#include <cstdint>
#include <numbers>

class RealtimeLearnIntegrationTests final
    : public juce::UnitTest
{
public:
    RealtimeLearnIntegrationTests()
        : juce::UnitTest(
            "Realtime Learn Integration",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testBackgroundLearnActivatesPhaseAlignedCancellation();
        testRelearnCanDisableActiveCancellation();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testBackgroundLearnActivatesPhaseAlignedCancellation()
    {
        beginTest(
            "Background Learn activates phase-aligned cancellation"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            1
        );

        LearnedHumModelMailbox mailbox;

        HumAnalysisWorker worker(
            controller,
            mailbox
        );

        worker.prepare(
            sampleRate,
            0
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        const auto workerStarted =
            worker.start();

        expect(
            workerStarted
        );

        if (!workerStarted)
        {
            return;
        }

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

        std::uint64_t sourceSamplePosition =
            0;

        constexpr double cleanFrequencyHz =
            997.0;

        constexpr float cleanAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        auto generateNextBlock =
            [&](
                juce::AudioBuffer<float>& buffer
            )
            {
                buffer.clear();

                generator.addToBuffer(
                    buffer
                );

                auto* samples =
                    buffer.getWritePointer(0);

                for (
                    int sample = 0;
                    sample < buffer.getNumSamples();
                    ++sample
                )
                {
                    const auto absoluteSample =
                        sourceSamplePosition
                        + static_cast<std::uint64_t>(
                            sample
                        );

                    const auto time =
                        static_cast<double>(
                            absoluteSample
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

                    samples[sample] +=
                        cleanSample;
                }

                sourceSamplePosition +=
                    static_cast<std::uint64_t>(
                        buffer.getNumSamples()
                    );
            };
        
        constexpr int preRollSamples =
            1234;

        juce::AudioBuffer<float> preRoll(
            1,
            preRollSamples
        );

        generateNextBlock(
            preRoll
        );

        controller.processBlock(
            preRoll
        );

        expect(
            controller.getCurrentSamplePosition()
                == sourceSamplePosition
        );

        expect(
            controller.startLearn()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> hostBlock(
            1,
            hostBlockSize
        );

        while (
            controller.isCollecting()
        )
        {
            generateNextBlock(
                hostBlock
            );

            controller.processBlock(
                hostBlock
            );

            expect(
                controller.getCurrentSamplePosition()
                    == sourceSamplePosition
            );
        }

        constexpr double timeoutMs =
            5000.0;

        const auto waitStartTime =
            juce::Time::getMillisecondCounterHiRes();

        while (
            !controller.isModelReady()
            && (
                juce::Time::getMillisecondCounterHiRes()
                - waitStartTime
            ) < timeoutMs
        )
        {
            generateNextBlock(
                hostBlock
            );

            controller.processBlock(
                hostBlock
            );

            expect(
                controller.getCurrentSamplePosition()
                    == sourceSamplePosition
            );

            juce::Thread::sleep(
                1
            );
        }

        expect(
            controller.isModelReady()
        );

        if (!controller.isModelReady())
        {
            worker.stop();
            return;
        }

        PendingLearnResult result;

        const auto consumed =
            mailbox.tryConsume(
                result
            );

        expect(
            consumed
        );

        if (!consumed)
        {
            worker.stop();
            return;
        }

        expect(
            result.model.valid
        );

        expect(
            result.model.humDetected
        );

        expectWithinAbsoluteError(
            result.model.frequencyHz,
            60.0,
            0.01
        );

        expect(
            result.analysisStartSample
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
        );

        const auto activationSample =
            controller.getCurrentSamplePosition();

        expect(
            activationSample
                == sourceSamplePosition
        );

        const auto sampleOffset =
            activationSample
            - result.analysisStartSample;

        expect(
            sampleOffset
                >= static_cast<std::uint64_t>(
                    12288
                )
        );

        canceller.activateModel(
            result.model,
            sampleOffset
        );

        expect(
            canceller.isActive()
        );

        controller.finishModelHandoff();

        expect(
            !controller.isModelReady()
        );

        constexpr int cancellationSamples =
            2000;

        juce::AudioBuffer<float> cancellationBlock(
            1,
            cancellationSamples
        );

        generateNextBlock(
            cancellationBlock
        );

        const auto* mixedSamples =
            cancellationBlock.getReadPointer(
                0
            );

        double errorEnergyBefore = 0.0;
        double errorEnergyAfter = 0.0;

        for (
            int sample = 0;
            sample < cancellationSamples;
            ++sample
        )
        {
            const auto absoluteSample =
                activationSample
                + static_cast<std::uint64_t>(
                    sample
                );

            const auto time =
                static_cast<double>(
                    absoluteSample
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
                mixedSamples[sample];

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

        expect(
            rmsAfter < rmsBefore
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

        controller.processBlock(
            cancellationBlock
        );

        expect(
            controller.getCurrentSamplePosition()
                == sourceSamplePosition
        );

        worker.stop();
    }

    void testRelearnCanDisableActiveCancellation()
    {
        beginTest(
            "Relearn with no hum disables active cancellation"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            1
        );

        LearnedHumModelMailbox mailbox;

        HumAnalysisWorker worker(
            controller,
            mailbox
        );

        worker.prepare(
            sampleRate,
            0
        );

        FixedHumCanceller canceller;

        canceller.prepare(
            sampleRate
        );

        const auto workerStarted =
            worker.start();

        expect(
            workerStarted
        );

        if (!workerStarted)
            return;

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(
            sampleRate
        );

        generator.clearHarmonics();

        expect(
            canceller.isActive()
        );

        expect(
            controller.startLearn()
        );

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

        std::uint64_t sourceSamplePosition =
            0;

        constexpr double cleanFrequencyHz =
            997.0;

        constexpr float cleanAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        auto generateNextBlock =
            [&](
                juce::AudioBuffer<float>& buffer
            )
            {
                buffer.clear();

                generator.addToBuffer(
                    buffer
                );

                auto* samples =
                    buffer.getWritePointer(0);

                for (
                    int sample = 0;
                    sample < buffer.getNumSamples();
                    ++sample
                )
                {
                    const auto absoluteSample =
                        sourceSamplePosition
                        + static_cast<std::uint64_t>(
                            sample
                        );

                    const auto time =
                        static_cast<double>(
                            absoluteSample
                        )
                        / sampleRate;

                    samples[sample] +=
                        cleanAmplitude
                        * static_cast<float>(
                            std::sin(
                                twoPi
                                * cleanFrequencyHz
                                * time
                            )
                        );
                }

                sourceSamplePosition +=
                    static_cast<std::uint64_t>(
                        buffer.getNumSamples()
                    );
            };

        auto processRealtimeBlock =
            [&](
                juce::AudioBuffer<float>& buffer
            )
            {
                // Capture raw input before cancellation.
                controller.processBlock(
                    buffer
                );

                // Simulate the realtime cancellation path.
                for (
                    int sample = 0;
                    sample < buffer.getNumSamples();
                    ++sample
                )
                {
                    static_cast<void>(
                        canceller.processSample(
                            buffer.getSample(
                                0,
                                sample
                            )
                        )
                    );
                }
            };

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> hostBlock(
            1,
            hostBlockSize
        );

        expect(
            controller.startLearn()
        );

        while (
            controller.isCollecting()
        )
        {
            generateNextBlock(
                hostBlock
            );

            processRealtimeBlock(
                hostBlock
            );
        }

        constexpr double timeoutMs =
            5000.0;

        auto waitForModelWhileProcessingAudio =
            [&]()
            {
                const auto startTime =
                    juce::Time::getMillisecondCounterHiRes();

                while (
                    !controller.isModelReady()
                    && (
                        juce::Time::getMillisecondCounterHiRes()
                        - startTime
                    ) < timeoutMs
                )
                {
                    generateNextBlock(
                        hostBlock
                    );

                    processRealtimeBlock(
                        hostBlock
                    );

                    juce::Thread::sleep(
                        1
                    );
                }

                return controller.isModelReady();
            };

        expect(
            waitForModelWhileProcessingAudio()
        );

        if (!controller.isModelReady())
        {
            worker.stop();
            return;
        }

        PendingLearnResult firstResult;

        const auto consumedFirst =
            mailbox.tryConsume(
                firstResult
            );

        expect(
            consumedFirst
        );

        if (!consumedFirst)
        {
            worker.stop();
            return;
        }

        expect(
            firstResult.model.valid
        );

        expect(
            firstResult.model.humDetected
        );

        if (
            !firstResult.model.valid
            || !firstResult.model.humDetected
        )
        {
            worker.stop();
            return;
        }

        const auto firstActivationSample =
            controller.getCurrentSamplePosition();

        expect(
            firstActivationSample
                == sourceSamplePosition
        );

        const auto firstSampleOffset =
            firstActivationSample
            - firstResult.analysisStartSample;

        canceller.activateModel(
            firstResult.model,
            firstSampleOffset
        );

        expect(
            canceller.isActive()
        );

        controller.finishModelHandoff();

        generator.clearHarmonics();

        expect(
            controller.startLearn()
        );

        const auto secondAnalysisStart =
            controller.getAnalysisStartSample();

        expect(
            secondAnalysisStart
                == controller.getCurrentSamplePosition()
        );

        while (
            controller.isCollecting()
        )
        {
            generateNextBlock(
                hostBlock
            );

            processRealtimeBlock(
                hostBlock
            );
        }

        expect(
            waitForModelWhileProcessingAudio()
        );

        if (!controller.isModelReady())
        {
            worker.stop();
            return;
        }

        PendingLearnResult secondResult;

        const auto consumedSecond =
            mailbox.tryConsume(
                secondResult
            );

        expect(
            consumedSecond
        );

        if (!consumedSecond)
        {
            worker.stop();
            return;
        }

        expect(
            secondResult.model.valid
        );

        expect(
            !secondResult.model.humDetected
        );

        expect(
            secondResult.analysisStartSample
                == secondAnalysisStart
        );

        if (
            !secondResult.model.valid
            || secondResult.model.humDetected
        )
        {
            worker.stop();
            return;
        }

        const auto secondActivationSample =
            controller.getCurrentSamplePosition();

        expect(
            secondActivationSample
                == sourceSamplePosition
        );

        const auto secondSampleOffset =
            secondActivationSample
            - secondResult.analysisStartSample;

        expect(
            secondSampleOffset
                >= static_cast<std::uint64_t>(12000)
        );

        canceller.activateModel(
            secondResult.model,
            secondSampleOffset
        );

        expect(
            !canceller.isActive()
        );

        controller.finishModelHandoff();

        expect(
            !controller.isModelReady()
        );

        juce::AudioBuffer<float> passThroughBlock(
            1,
            hostBlockSize
        );

        generateNextBlock(
            passThroughBlock
        );

        for (
            int sample = 0;
            sample < passThroughBlock.getNumSamples();
            ++sample
        )
        {
            const auto inputSample =
                passThroughBlock.getSample(
                    0,
                    sample
                );

            const auto outputSample =
                canceller.processSample(
                    inputSample
                );

            expectEquals(
                outputSample,
                inputSample
            );
        }

        controller.processBlock(
            passThroughBlock
        );

        expect(
            controller.getCurrentSamplePosition()
                == sourceSamplePosition
        );

        worker.stop();
    }
};

static RealtimeLearnIntegrationTests
    realtimeLearnIntegrationTests;