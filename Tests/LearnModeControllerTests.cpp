#include <JuceHeader.h>

#include "../Source/DSP/LearnModeController.h"

#include <cstdint>

class LearnModeControllerTests final
    : public juce::UnitTest
{
public:
    LearnModeControllerTests()
        : juce::UnitTest(
            "Learn Mode Controller",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testTracksAbsoluteSamplePosition();
        testRecordsLearnStartPosition();
        testTimelineIncludesCaptureOvershootAndAnalysisDelay();
        testCompletedCaptureCanBeClaimedOnce();
        testLearnCannotRestartWhileBusy();
        testAnalysisBufferRemainsFrozenWhileAudioContinues();
        testBufferCanBeReusedAfterModelHandoffFinishes();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    static constexpr int numChannels =
        1;

    void testTracksAbsoluteSamplePosition()
    {
        beginTest(
            "Tracks absolute sample position across blocks"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.getCurrentSamplePosition() == 0
        );

        juce::AudioBuffer<float> firstBlock(
            numChannels,
            512
        );

        firstBlock.clear();

        controller.processBlock(
            firstBlock
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(512)
        );

        juce::AudioBuffer<float> secondBlock(
            numChannels,
            1024
        );

        secondBlock.clear();

        controller.processBlock(
            secondBlock
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(1536)
        );
    }

    void testRecordsLearnStartPosition()
    {
        beginTest(
            "Records Learn start at current sample position"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        constexpr int preRollSamples =
            1000;

        juce::AudioBuffer<float> preRollBlock(
            numChannels,
            preRollSamples
        );

        preRollBlock.clear();

        controller.processBlock(
            preRollBlock
        );

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
        );

        expect(
            controller.startLearn()
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
        );
    }

    void testTimelineIncludesCaptureOvershootAndAnalysisDelay()
    {
        beginTest(
            "Timeline includes capture overshoot and analysis delay"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        constexpr int preRollSamples =
            1000;

        juce::AudioBuffer<float> preRollBlock(
            numChannels,
            preRollSamples
        );

        preRollBlock.clear();

        controller.processBlock(
            preRollBlock
        );

        expect(
            controller.startLearn()
        );

        expect(
            controller.isCollecting()
        );

        expect(
            controller.getAnalysisStartSample()
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
        );

        constexpr int hostBlockSize =
            4096;

        juce::AudioBuffer<float> hostBlock(
            numChannels,
            hostBlockSize
        );

        hostBlock.clear();

        controller.processBlock(
            hostBlock
        );

        expect(
            controller.isCollecting()
        );

        controller.processBlock(
            hostBlock
        );

        expect(
            controller.isCollecting()
        );

        controller.processBlock(
            hostBlock
        );

        expect(
            !controller.isCollecting()
        );

        expect(
            controller.isReadyForAnalysis()
        );

        constexpr std::uint64_t
            capturedTimelineSamples =
                3 * hostBlockSize;

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(
                    preRollSamples
                )
                + capturedTimelineSamples
        );

        const auto elapsedAtCaptureComplete =
            controller.getCurrentSamplePosition()
            - controller.getAnalysisStartSample();

        expect(
            elapsedAtCaptureComplete
                == capturedTimelineSamples
        );

        constexpr int firstAnalysisDelayBlock =
            512;

        juce::AudioBuffer<float> delayBlockA(
            numChannels,
            firstAnalysisDelayBlock
        );

        delayBlockA.clear();

        controller.processBlock(
            delayBlockA
        );

        constexpr int secondAnalysisDelayBlock =
            777;

        juce::AudioBuffer<float> delayBlockB(
            numChannels,
            secondAnalysisDelayBlock
        );

        delayBlockB.clear();

        controller.processBlock(
            delayBlockB
        );

        expect(
            controller.isReadyForAnalysis()
        );

        const auto elapsedAtActivation =
            controller.getCurrentSamplePosition()
            - controller.getAnalysisStartSample();

        constexpr std::uint64_t
            expectedElapsedSamples =
                capturedTimelineSamples
                + firstAnalysisDelayBlock
                + secondAnalysisDelayBlock;

        expect(
            elapsedAtActivation
                == expectedElapsedSamples
        );
    }

    void testCompletedCaptureCanBeClaimedOnce()
    {
        beginTest(
            "Completed capture can be claimed exactly once"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.startLearn()
        );

        constexpr int analysisSamples =
            12000;

        juce::AudioBuffer<float> block(
            numChannels,
            analysisSamples
        );

        block.clear();

        controller.processBlock(
            block
        );

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            !controller.isAnalyzing()
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

        expect(
            !controller.tryBeginAnalysis()
        );
    }

    void testLearnCannotRestartWhileBusy()
    {
        beginTest(
            "Learn cannot restart while controller is busy"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.startLearn()
        );

        expect(
            !controller.startLearn()
        );

        constexpr int analysisSamples =
            12000;

        juce::AudioBuffer<float> block(
            numChannels,
            analysisSamples
        );

        block.clear();

        controller.processBlock(
            block
        );

        expect(
            controller.isReadyForAnalysis()
        );

        // Capture is complete but has not yet
        // been claimed by the worker.
        expect(
            !controller.startLearn()
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        // Worker now owns the frozen buffer.
        expect(
            !controller.startLearn()
        );
    }

    void testAnalysisBufferRemainsFrozenWhileAudioContinues()
    {
        beginTest(
            "Analysis buffer remains frozen while audio continues"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        expect(
            controller.startLearn()
        );

        constexpr int analysisSamples =
            12000;

        juce::AudioBuffer<float> captureBlock(
            numChannels,
            analysisSamples
        );

        auto* captureSamples =
            captureBlock.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            captureSamples[sample] =
                static_cast<float>(sample)
                / static_cast<float>(
                    analysisSamples
                );
        }

        controller.processBlock(
            captureBlock
        );

        expect(
            controller.isReadyForAnalysis()
        );

        expect(
            controller.tryBeginAnalysis()
        );

        expect(
            controller.isAnalyzing()
        );

        const auto& analysisBuffer =
            controller.getAnalysisBuffer();

        expectEquals(
            analysisBuffer.getNumSamples(),
            analysisSamples
        );

        const auto firstSample =
            analysisBuffer.getSample(
                0,
                0
            );

        const auto middleSample =
            analysisBuffer.getSample(
                0,
                analysisSamples / 2
            );

        const auto lastSample =
            analysisBuffer.getSample(
                0,
                analysisSamples - 1
            );

        constexpr int realtimeBlockSize =
            4096;

        juce::AudioBuffer<float> realtimeBlock(
            numChannels,
            realtimeBlockSize
        );

        realtimeBlock.clear();

        for (
            int sample = 0;
            sample < realtimeBlockSize;
            ++sample
        )
        {
            realtimeBlock.setSample(
                0,
                sample,
                -0.75f
            );
        }

        const auto positionBefore =
            controller.getCurrentSamplePosition();

        controller.processBlock(
            realtimeBlock
        );

        controller.processBlock(
            realtimeBlock
        );

        const auto positionAfter =
            controller.getCurrentSamplePosition();

        expect(
            positionAfter
                == positionBefore
                + static_cast<std::uint64_t>(
                    2 * realtimeBlockSize
                )
        );

        expectEquals(
            analysisBuffer.getSample(
                0,
                0
            ),
            firstSample
        );

        expectEquals(
            analysisBuffer.getSample(
                0,
                analysisSamples / 2
            ),
            middleSample
        );

        expectEquals(
            analysisBuffer.getSample(
                0,
                analysisSamples - 1
            ),
            lastSample
        );

        expect(
            controller.isAnalyzing()
        );
    }

    void testBufferCanBeReusedAfterModelHandoffFinishes()
    {
        beginTest(
            "Buffer can be reused after model handoff finishes"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        const auto firstLearnStart =
            controller.getCurrentSamplePosition();

        expect(
            controller.startLearn()
        );

        constexpr int analysisSamples =
            12000;

        juce::AudioBuffer<float> firstCapture(
            numChannels,
            analysisSamples
        );

        firstCapture.clear();

        controller.processBlock(
            firstCapture
        );

        expect(
            controller.isReadyForAnalysis()
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

        expect(
            !controller.startLearn()
        );

        controller.finishModelHandoff();

        expect(
            !controller.isModelReady()
        );

        const auto secondLearnStart =
            controller.getCurrentSamplePosition();

        expect(
            controller.startLearn()
        );

        expect(
            controller.getAnalysisStartSample()
                == secondLearnStart
        );

        expect(
            secondLearnStart
                > firstLearnStart
        );

        expect(
            secondLearnStart
                == firstLearnStart
                + static_cast<std::uint64_t>(
                    analysisSamples
                )
        );

        expect(
            controller.isCollecting()
        );

        juce::AudioBuffer<float> secondCapture(
            numChannels,
            analysisSamples
        );

        secondCapture.clear();

        controller.processBlock(
            secondCapture
        );

        expect(
            controller.isReadyForAnalysis()
        );
    }
};

static LearnModeControllerTests
    learnModeControllerTests;