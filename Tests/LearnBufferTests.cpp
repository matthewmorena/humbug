#include <JuceHeader.h>

#include "../Source/DSP/LearnBuffer.h"

#include <array>

class LearnBufferTests final
    : public juce::UnitTest
{
public:
    LearnBufferTests()
        : juce::UnitTest(
            "Learn Buffer",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testCollectsAcrossArbitraryBlockSizes();
        testCanRestartCollection();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testCollectsAcrossArbitraryBlockSizes()
    {
        beginTest(
            "Collects complete analysis window "
            "across arbitrary block sizes"
        );

        LearnBuffer learnBuffer;

        learnBuffer.prepare(
            sampleRate,
            2
        );

        expect(
            !learnBuffer.isCollecting()
        );

        expect(
            !learnBuffer.isReady()
        );

        learnBuffer.start();

        expect(
            learnBuffer.isCollecting()
        );

        expect(
            !learnBuffer.isReady()
        );

        constexpr std::array<int, 7>
            blockSizes {
                512,
                512,
                1024,
                256,
                2048,
                4096,
                4096
            };

        int sourcePosition = 0;

        for (
            const auto blockSize
            : blockSizes
        )
        {
            juce::AudioBuffer<float>
                input(
                    2,
                    blockSize
                );

            for (
                int sample = 0;
                sample < blockSize;
                ++sample
            )
            {
                const auto sourceSample =
                    static_cast<float>(
                        sourcePosition
                        + sample
                    );

                input.setSample(
                    0,
                    sample,
                    sourceSample
                );

                input.setSample(
                    1,
                    sample,
                    -sourceSample
                );
            }

            learnBuffer.push(
                input
            );

            sourcePosition +=
                blockSize;
        }

        expect(
            learnBuffer.isReady()
        );

        expect(
            !learnBuffer.isCollecting()
        );

        const auto& capturedBuffer =
            learnBuffer.getBuffer();

        constexpr int expectedSamples =
            12000;

        expectEquals(
            capturedBuffer.getNumChannels(),
            2
        );

        expectEquals(
            capturedBuffer.getNumSamples(),
            expectedSamples
        );

        for (
            int sample = 0;
            sample < expectedSamples;
            ++sample
        )
        {
            const auto expectedSample =
                static_cast<float>(
                    sample
                );

            expectEquals(
                capturedBuffer.getSample(
                    0,
                    sample
                ),
                expectedSample
            );

            expectEquals(
                capturedBuffer.getSample(
                    1,
                    sample
                ),
                -expectedSample
            );
        }

        juce::AudioBuffer<float> extraBlock(
            2,
            512
        );

        extraBlock.clear();

        for (
            int channel = 0;
            channel < extraBlock.getNumChannels();
            ++channel
        )
        {
            for (
                int sample = 0;
                sample < extraBlock.getNumSamples();
                ++sample
            )
            {
                extraBlock.setSample(
                    channel,
                    sample,
                    99.0f
                );
            }
        }

        learnBuffer.push(
            extraBlock
        );

        expect(
            learnBuffer.isReady()
        );

        expect(
            !learnBuffer.isCollecting()
        );

        expectEquals(
            capturedBuffer.getSample(
                0,
                expectedSamples - 1
            ),
            static_cast<float>(
                expectedSamples - 1
            )
        );

        expectEquals(
            capturedBuffer.getSample(
                1,
                expectedSamples - 1
            ),
            -static_cast<float>(
                expectedSamples - 1
            )
        );
    }

    void testCanRestartCollection()
    {
        beginTest(
            "Can restart collection after reset"
        );

        LearnBuffer learnBuffer;

        learnBuffer.prepare(
            sampleRate,
            1
        );

        constexpr int expectedSamples =
            12000;

        juce::AudioBuffer<float> firstInput(
            1,
            expectedSamples
        );

        for (
            int sample = 0;
            sample < expectedSamples;
            ++sample
        )
        {
            firstInput.setSample(
                0,
                sample,
                1.0f
            );
        }

        learnBuffer.start();
        learnBuffer.push(firstInput);

        expect(
            learnBuffer.isReady()
        );

        learnBuffer.reset();

        expect(
            !learnBuffer.isCollecting()
        );

        expect(
            !learnBuffer.isReady()
        );

        learnBuffer.start();

        expect(
            learnBuffer.isCollecting()
        );

        juce::AudioBuffer<float> secondInput(
            1,
            expectedSamples
        );

        for (
            int sample = 0;
            sample < expectedSamples;
            ++sample
        )
        {
            secondInput.setSample(
                0,
                sample,
                2.0f
            );
        }

        learnBuffer.push(secondInput);

        expect(
            learnBuffer.isReady()
        );

        const auto& capturedBuffer =
            learnBuffer.getBuffer();

        for (
            int sample = 0;
            sample < expectedSamples;
            ++sample
        )
        {
            expectEquals(
                capturedBuffer.getSample(
                    0,
                    sample
                ),
                2.0f
            );
        }
    }
};

static LearnBufferTests
    learnBufferTests;