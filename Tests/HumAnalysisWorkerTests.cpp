#include <JuceHeader.h>

#include "../Source/DSP/HumAnalysisWorker.h"
#include "../Source/DSP/HumGenerator.h"

#include <cmath>
#include <numbers>

class HumAnalysisWorkerTests final
    : public juce::UnitTest
{
public:
    HumAnalysisWorkerTests()
        : juce::UnitTest(
            "Hum Analysis Worker",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testAnalyzesCompletedCapture();
        testPublishesNoHumResult();
        testSupportsConsecutiveLearnOperations();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testAnalyzesCompletedCapture()
    {
        beginTest(
            "Worker analyzes completed Learn capture"
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

        expect(
            worker.start()
        );

        juce::AudioBuffer<float> preRoll(
            1,
            1234
        );

        preRoll.clear();

        constexpr int analysisSamples =
            12000;

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

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        analysisBuffer.clear();

        generator.addToBuffer(
            analysisBuffer
        );

        constexpr double cleanFrequencyHz =
            997.0;

        constexpr float cleanAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

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

        controller.processBlock(
            preRoll
        );

        expect(
            controller.startLearn()
        );

        controller.processBlock(
            analysisBuffer
        );

        constexpr double timeoutMs =
            5000.0;

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
            juce::Thread::sleep(
                1
            );
        }

        expect(
            controller.isModelReady()
        );

        PendingLearnResult result;

        expect(
            mailbox.tryConsume(
                result
            )
        );

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
                    1234
                )
        );

        controller.finishModelHandoff();

        expect(
            !controller.isModelReady()
        );

        worker.stop();
    }

    void testPublishesNoHumResult()
    {
        beginTest(
            "Worker publishes completed no-hum analysis"
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

        expect(
            worker.start()
        );

        constexpr int preRollSamples =
            777;

        juce::AudioBuffer<float> preRoll(
            1,
            preRollSamples
        );

        preRoll.clear();

        controller.processBlock(
            preRoll
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

        expect(
            controller.startLearn()
        );

        controller.processBlock(
            analysisBuffer
        );

        constexpr double timeoutMs =
            5000.0;

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
            juce::Thread::sleep(
                1
            );
        }

        expect(
            controller.isModelReady()
        );

        PendingLearnResult result;

        expect(
            mailbox.tryConsume(
                result
            )
        );

        expect(
            result.model.valid
        );

        expect(
            !result.model.humDetected
        );

        expect(
            result.analysisStartSample
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
        );

        controller.finishModelHandoff();

        expect(
            !controller.isModelReady()
        );

        worker.stop();
    }

    void testSupportsConsecutiveLearnOperations()
    {
        beginTest(
            "Worker supports consecutive Learn operations"
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

        expect(
            worker.start()
        );

        constexpr int analysisSamples =
            12000;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        //
        // First Learn: 60 Hz hum
        //

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

        generator.reset();

        juce::AudioBuffer<float> firstBuffer(
            1,
            analysisSamples
        );

        firstBuffer.clear();

        generator.addToBuffer(
            firstBuffer
        );

        expect(
            controller.startLearn()
        );

        controller.processBlock(
            firstBuffer
        );

        auto waitForModelReady =
            [&controller]()
            {
                constexpr double timeoutMs =
                    5000.0;
                
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
                    juce::Thread::sleep(
                        1
                    );
                }

                return controller.isModelReady();
            };

        expect(
            waitForModelReady()
        );

        PendingLearnResult firstResult;

        expect(
            mailbox.tryConsume(
                firstResult
            )
        );

        expect(
            firstResult.model.valid
        );

        expect(
            firstResult.model.humDetected
        );

        expectWithinAbsoluteError(
            firstResult.model.frequencyHz,
            60.0,
            0.01
        );

        expect(
            firstResult.analysisStartSample
                == static_cast<std::uint64_t>(0)
        );

        controller.finishModelHandoff();

        //
        // Second Learn: unrelated 997 Hz signal
        //

        constexpr double unrelatedFrequencyHz =
            997.0;

        constexpr float unrelatedAmplitude =
            0.20f;

        juce::AudioBuffer<float> secondBuffer(
            1,
            analysisSamples
        );

        auto* secondSamples =
            secondBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            const auto time =
                static_cast<double>(sample)
                / sampleRate;

            secondSamples[sample] =
                unrelatedAmplitude
                * static_cast<float>(
                    std::sin(
                        twoPi
                        * unrelatedFrequencyHz
                        * time
                    )
                );
        }

        expect(
            controller.startLearn()
        );

        controller.processBlock(
            secondBuffer
        );

        expect(
            waitForModelReady()
        );

        PendingLearnResult secondResult;

        expect(
            mailbox.tryConsume(
                secondResult
            )
        );

        expect(
            secondResult.model.valid
        );

        expect(
            !secondResult.model.humDetected
        );

        expect(
            secondResult.analysisStartSample
                == static_cast<std::uint64_t>(
                    analysisSamples
                )
        );

        controller.finishModelHandoff();

        expect(
            !controller.isModelReady()
        );

        expect(
            !controller.isAnalyzing()
        );

        worker.stop();
    }
};

static HumAnalysisWorkerTests
    humAnalysisWorkerTests;