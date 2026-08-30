#include <JuceHeader.h>

#include "../Source/DSP/HumGenerator.h"
#include "../Source/DSP/HumTrackingController.h"
#include "../Source/DSP/HumTrackingWorker.h"
#include "../Source/DSP/LearnedHumModelMailbox.h"

#include <cstdint>

class HumTrackingWorkerTests final
    : public juce::UnitTest
{
public:
    HumTrackingWorkerTests()
        : juce::UnitTest(
            "Hum Tracking Worker",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testAnalyzesPeriodicTrackingCapture();
        testProducesSuccessiveTrackingUpdates();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testAnalyzesPeriodicTrackingCapture()
    {
        beginTest(
            "Worker analyzes periodic tracking capture"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            1
        );

        LearnedHumModelMailbox mailbox;

        HumTrackingWorker worker(
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

        expect(
            controller.startTracking()
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

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            1,
            hostBlockSize
        );

        const auto processSourceBlock =
            [&]()
            {
                auto* samples =
                    block.getWritePointer(0);

                for (
                    int sample = 0;
                    sample < hostBlockSize;
                    ++sample
                )
                {
                    samples[sample] =
                        generator.processSample();
                }

                controller.processBlock(
                    block
                );
            };

        // 94 blocks advance the absolute timeline
        // to 48128. The following block begins
        // the first tracking capture.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            processSourceBlock();
        }

        expect(
            controller.isWaiting()
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        // 24 blocks provide the entire 12000-sample
        // tracking window, including the partial
        // final block.
        for (
            int blockIndex = 0;
            blockIndex < 24;
            ++blockIndex
        )
        {
            processSourceBlock();
        }

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

        // ModelReady must only become visible after
        // the worker has successfully published.
        expect(
            mailbox.hasPendingResult()
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
                    48128
                )
        );

        controller.finishModelHandoff();

        expect(
            controller.isWaiting()
        );

        worker.stop();
    }

    void testProducesSuccessiveTrackingUpdates()
    {
        beginTest(
            "Worker produces successive tracking updates"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            1
        );

        LearnedHumModelMailbox mailbox;

        HumTrackingWorker worker(
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

        expect(
            controller.startTracking()
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

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            1,
            hostBlockSize
        );

        const auto processSourceBlock =
            [&]()
            {
                auto* samples =
                    block.getWritePointer(0);

                for (
                    int sample = 0;
                    sample < hostBlockSize;
                    ++sample
                )
                {
                    samples[sample] =
                        generator.processSample();
                }

                controller.processBlock(
                    block
                );
            };

        const auto waitForModelReady =
            [&]()
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

        // Advance to the first tracking boundary.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            processSourceBlock();
        }

        // Capture the first 250 ms window.
        for (
            int blockIndex = 0;
            blockIndex < 24;
            ++blockIndex
        )
        {
            processSourceBlock();
        }

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
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        controller.finishModelHandoff();

        expect(
            controller.isWaiting()
        );

        // Change the physical source without
        // resetting oscillator phase.
        generator.setFundamentalFrequency(
            60.10
        );

        // The next nominal target is:
        //
        // 48128 + 48000 = 96128
        //
        // Current position after the first capture
        // is 60416.
        //
        // 69 blocks move to 95744.
        for (
            int blockIndex = 0;
            blockIndex < 69;
            ++blockIndex
        )
        {
            processSourceBlock();
        }

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    95744
                )
        );

        expect(
            controller.isWaiting()
        );

        // This block still begins before 96128.
        processSourceBlock();

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    96256
                )
        );

        expect(
            controller.isWaiting()
        );

        // Capture the second tracking window.
        // Its first block begins at 96256.
        for (
            int blockIndex = 0;
            blockIndex < 24;
            ++blockIndex
        )
        {
            processSourceBlock();
        }

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
            secondResult.model.humDetected
        );

        expectWithinAbsoluteError(
            secondResult.model.frequencyHz,
            60.10,
            0.01
        );

        expect(
            secondResult.analysisStartSample
                == static_cast<std::uint64_t>(
                    96256
                )
        );

        expect(
            secondResult.analysisStartSample
                > firstResult.analysisStartSample
        );

        expect(
            secondResult.model.frequencyHz
                - firstResult.model.frequencyHz
            > 0.05
        );

        controller.finishModelHandoff();

        expect(
            controller.isWaiting()
        );

        worker.stop();
    }
};

static HumTrackingWorkerTests
    humTrackingWorkerTests;