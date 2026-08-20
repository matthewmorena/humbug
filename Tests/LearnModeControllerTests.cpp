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

        controller.startLearn();

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

        controller.startLearn();

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
            controller.isReady()
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
            controller.isReady()
        );

        expectEquals(
            controller
                .getAnalysisBuffer()
                .getNumSamples(),
            12000
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

    void testResetPreservesSamplePosition()
    {
        beginTest(
            "Reset preserves absolute sample position"
        );

        LearnModeController controller;

        controller.prepare(
            sampleRate,
            numChannels
        );

        juce::AudioBuffer<float> block(
            numChannels,
            512
        );

        block.clear();

        controller.processBlock(block);

        controller.reset();

        expect(
            controller.getCurrentSamplePosition()
                == static_cast<std::uint64_t>(512)
        );

        expect(
            !controller.isCollecting()
        );

        expect(
            !controller.isReady()
        );
    }
};

static LearnModeControllerTests
    learnModeControllerTests;