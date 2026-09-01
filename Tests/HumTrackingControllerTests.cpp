#include <JuceHeader.h>

#include "../Source/DSP/HumTrackingController.h"

#include <cstdint>

class HumTrackingControllerTests final
    : public juce::UnitTest
{
public:
    HumTrackingControllerTests()
        : juce::UnitTest(
            "Hum Tracking Controller",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testBeginsCaptureAtTrackingCadence();
        testCompletesCaptureAcrossHostBlocks();
        testResumesPeriodicCaptureAfterModelHandoff();
        testDoesNotCaptureWhilePreviousUpdateIsOutstanding();

        testStopsImmediatelyWhileWaiting();
        testStopsAfterOutstandingOperationCompletes();
        testStopRequestIsHonoredWhenAnalysisAborts();
        testSnapshotsReferenceFrequencyForEachCapture();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    static constexpr int numChannels =
        1;

    void testBeginsCaptureAtTrackingCadence()
    {
        beginTest(
            "Begins capture at first eligible block after tracking interval"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.isInactive()
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        expect(
            controller.isWaiting()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // 93 blocks = 47616 samples.
        // We have not yet reached the 48000-sample
        // tracking interval.
        for (
            int blockIndex = 0;
            blockIndex < 93;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    47616
                )
        );

        expect(
            controller.isWaiting()
        );

        // This block begins at 47616, which is still
        // before the 48000-sample target.
        controller.processBlock(
            block
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        expect(
            controller.isWaiting()
        );

        // The next block begins at 48128, so this is
        // the first block boundary at or beyond the
        // requested tracking interval.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    48640
                )
        );
    }

    void testCompletesCaptureAcrossHostBlocks()
    {
        beginTest(
            "Completes exact tracking window across host blocks"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // Advance through 94 blocks.
        //
        // 94 * 512 = 48128 samples.
        //
        // The final waiting block begins at 47616,
        // so tracking has still not started during it.
        // The following block begins at 48128 and
        // becomes the first capture block.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
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

        constexpr int fullCaptureBlocks =
            23;

        // Fill the capture blocks with a simple
        // continuous sequence:
        //
        // 0, 1, 2, ... 11999
        //
        // This lets us verify that the partial
        // final host block is clipped correctly.
        for (
            int blockIndex = 0;
            blockIndex < fullCaptureBlocks;
            ++blockIndex
        )
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
                    static_cast<float>(
                        blockIndex
                        * hostBlockSize
                        + sample
                    );
            }

            controller.processBlock(
                block
            );
        }

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    59904
                )
        );

        // 23 * 512 = 11776 samples have been
        // captured. Only another 224 samples
        // are required to reach 12000.
        auto* finalSamples =
            block.getWritePointer(0);

        for (
            int sample = 0;
            sample < hostBlockSize;
            ++sample
        )
        {
            finalSamples[sample] =
                static_cast<float>(
                    fullCaptureBlocks
                    * hostBlockSize
                    + sample
                );
        }

        controller.processBlock(
            block
        );

        expect(
            !controller.isCollecting()
        );

        expect(
            controller.isReadyForAnalysis()
        );

        // The host timeline advances by the
        // entire final block, not only the
        // 224 samples retained by LearnBuffer.
        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    60416
                )
        );

        const auto elapsedTimelineSamples =
            controller.getCurrentSamplePosition()
            - controller.getAnalysisStartSample();

        expect(
            elapsedTimelineSamples
                == static_cast<std::uint64_t>(
                    12288
                )
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        expect(
            !controller.isReadyForAnalysis()
        );

        const auto& analysisBuffer =
            controller.getAnalysisBuffer();

        expectEquals(
            analysisBuffer.getNumSamples(),
            12000
        );

        expectEquals(
            analysisBuffer.getSample(
                0,
                0
            ),
            0.0f
        );

        expectEquals(
            analysisBuffer.getSample(
                0,
                11999
            ),
            11999.0f
        );
    }

    void testResumesPeriodicCaptureAfterModelHandoff()
    {
        beginTest(
            "Resumes periodic capture after model handoff"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // Advance to absolute sample 48128.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        expect(
            controller.isWaiting()
        );

        // First capture begins at 48128.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        // The first block above already contributed
        // 512 samples. Process another 23 blocks to
        // complete the 12000-sample analysis window.
        for (
            int blockIndex = 0;
            blockIndex < 23;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    60416
                )
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        controller.finishAnalysis();

        expect(
            controller.isModelReady()
        );

        expect(
            !controller.isAnalyzing()
        );

        controller.finishModelHandoff();

        expect(
            controller.isWaiting()
        );

        // First actual capture began at 48128,
        // so the next nominal capture start is:
        //
        // 48128 + 48000 = 96128.
        //
        // From 60416, 69 blocks move us to 95744.
        for (
            int blockIndex = 0;
            blockIndex < 69;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
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

        // This block begins before 96128,
        // so it must remain a waiting block.
        controller.processBlock(
            block
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    96256
                )
        );

        expect(
            controller.isWaiting()
        );

        // This is now the first block beginning
        // at or after the nominal capture target.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    96256
                )
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    96768
                )
        );
    }

    void testDoesNotCaptureWhilePreviousUpdateIsOutstanding()
    {
        beginTest(
            "Does not capture while previous update is outstanding"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // Reach the first eligible capture boundary.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        // Begin the first capture.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        // Complete the 12000-sample capture.
        //
        // The first block above already supplied
        // 512 samples, so another 23 host blocks
        // take us through the partial final block.
        for (
            int blockIndex = 0;
            blockIndex < 23;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    60416
                )
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        const auto originalAnalysisStartSample =
            controller.getAnalysisStartSample();

        // The next nominal capture target would be:
        //
        // 48128 + 48000 = 96128
        //
        // Deliberately keep analysis outstanding
        // well beyond that point.
        while (
            controller.getCurrentSamplePosition()
            < static_cast<std::uint64_t>(
                110000
            )
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isAnalyzing()
        );

        // No new capture may have replaced the
        // original tracking window.
        expect(
            controller.getAnalysisStartSample()
                == originalAnalysisStartSample
        );

        controller.finishAnalysis();

        expect(
            controller.isModelReady()
        );

        // Keep the completed model outstanding for
        // additional blocks. Tracking still must not
        // start another capture.
        for (
            int blockIndex = 0;
            blockIndex < 10;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isModelReady()
        );

        expect(
            controller.getAnalysisStartSample()
                == originalAnalysisStartSample
        );

        // Simulate the audio thread consuming the
        // published tracking result.
        controller.finishModelHandoff();

        expect(
            controller.isWaiting()
        );

        const auto positionBeforeNextBlock =
            controller.getCurrentSamplePosition();

        // The nominal cadence is already overdue,
        // so the first block after handoff should
        // immediately begin a fresh capture.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == positionBeforeNextBlock
        );

        expect(
            controller.getAnalysisStartSample()
                != originalAnalysisStartSample
        );
    }

    void testStopsImmediatelyWhileWaiting()
    {
        beginTest(
            "Stops immediately while waiting"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.isInactive()
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        expect(
            controller.isWaiting()
        );

        controller.requestStopTracking();

        expect(
            controller.isInactive()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // Process well beyond the one-second tracking
        // interval. No capture should begin because
        // tracking is inactive.
        for (
            int blockIndex = 0;
            blockIndex < 100;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isInactive()
        );

        expect(
            !controller.isCollecting()
        );

        expect(
            !controller.isReadyForAnalysis()
        );
    }

    void testStopsAfterOutstandingOperationCompletes()
    {
        beginTest(
            "Stops after outstanding tracking operation completes"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // Advance to absolute sample 48128.
        //
        // The first tracking capture will begin on
        // the next block.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
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

        // Begin the tracking capture.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    48128
                )
        );

        // The first block supplied 512 samples.
        // Another 23 blocks complete the 12000-sample
        // tracking window.
        for (
            int blockIndex = 0;
            blockIndex < 23;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        const auto originalAnalysisStartSample =
            controller.getAnalysisStartSample();

        // Request the stop while the worker conceptually
        // owns the frozen analysis buffer.
        controller.requestStopTracking();

        // Tracking must not stop immediately because an
        // operation is still outstanding.
        expect(
            controller.isAnalyzing()
        );

        expect(
            !controller.isInactive()
        );

        // The analysis completes normally.
        controller.finishAnalysis();

        expect(
            controller.isModelReady()
        );

        expect(
            !controller.isInactive()
        );

        // Simulate the audio thread consuming the result.
        // Because a stop was requested, handoff should now
        // transition to Inactive rather than Waiting.
        controller.finishModelHandoff();

        expect(
            controller.isInactive()
        );

        expect(
            !controller.isWaiting()
        );

        expect(
            controller.getAnalysisStartSample()
                == originalAnalysisStartSample
        );

        // Process well beyond another tracking interval.
        // No new capture should occur.
        for (
            int blockIndex = 0;
            blockIndex < 100;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isInactive()
        );

        expect(
            !controller.isCollecting()
        );

        expect(
            !controller.isReadyForAnalysis()
        );
    }

    void testStopRequestIsHonoredWhenAnalysisAborts()
    {
        beginTest(
            "Stop request is honored when analysis aborts"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        // Capture 12000 samples.
        for (
            int blockIndex = 0;
            blockIndex < 24;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        controller.requestStopTracking();

        expect(
            controller.isAnalyzing()
        );

        controller.abortAnalysis();

        expect(
            controller.isInactive()
        );

        // Confirm the aborted operation cannot cause a
        // later automatic capture.
        for (
            int blockIndex = 0;
            blockIndex < 100;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isInactive()
        );
    }

    void testSnapshotsReferenceFrequencyForEachCapture()
    {
        beginTest(
            "Snapshots reference frequency for each tracking capture"
        );

        HumTrackingController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.setReferenceFrequency(
                60.0
            )
        );

        expect(
            controller.startTracking()
        );

        constexpr int hostBlockSize =
            512;

        juce::AudioBuffer<float> block(
            numChannels,
            hostBlockSize
        );

        block.clear();

        // Advance to the first eligible capture.
        for (
            int blockIndex = 0;
            blockIndex < 94;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        // First capture begins with 60.0 Hz as
        // its tracking reference.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        // Complete the first tracking window.
        for (
            int blockIndex = 0;
            blockIndex < 23;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isReadyForAnalysis()
        );

        // Change the reference AFTER this capture
        // completed but BEFORE the worker claims it.
        expect(
            controller.setReferenceFrequency(
                60.15
            )
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expectWithinAbsoluteError(
            controller
                .getAnalysisReferenceFrequency(),
            60.0,
            1.0e-12
        );

        controller.finishAnalysis();
        controller.finishModelHandoff();

        // Current position is 60416.
        // Advance toward the next nominal target.
        for (
            int blockIndex = 0;
            blockIndex < 69;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        controller.processBlock(
            block
        );

        expect(
            controller.isWaiting()
        );

        // Next block begins the second capture.
        controller.processBlock(
            block
        );

        expect(
            controller.isCollecting()
        );

        for (
            int blockIndex = 0;
            blockIndex < 23;
            ++blockIndex
        )
        {
            controller.processBlock(
                block
            );
        }

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            controller.tryBeginAnalysis()
        );

        // The second window must use the newer
        // reference frequency.
        expectWithinAbsoluteError(
            controller
                .getAnalysisReferenceFrequency(),
            60.15,
            1.0e-12
        );

        controller.finishAnalysis();
        controller.finishModelHandoff();
    }
};

static HumTrackingControllerTests
    humTrackingControllerTests;