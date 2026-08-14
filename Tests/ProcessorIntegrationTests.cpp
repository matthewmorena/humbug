#include <JuceHeader.h>

#include "../Source/PluginProcessor.h"

class ProcessorIntegrationTests final
    : public juce::UnitTest
{
public:
    ProcessorIntegrationTests()
        : juce::UnitTest(
            "Processor Integration",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testSyntheticHumIsGenerated();
        testSyntheticHumMatchesAcrossStereoChannels();
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 512;

    void testSyntheticHumIsGenerated()
    {
        beginTest(
            "Processor generates synthetic hum"
        );

        HumbugAudioProcessor processor;
        processor.setSyntheticHumEnabled(true);

        processor.prepareToPlay(
            sampleRate,
            blockSize
        );

        juce::AudioBuffer<float> buffer(
            2,
            blockSize
        );

        buffer.clear();

        juce::MidiBuffer midi;

        processor.processBlock(
            buffer,
            midi
        );

        float maximumMagnitude = 0.0f;

        for (
            int sample = 0;
            sample < blockSize;
            ++sample
        )
        {
            maximumMagnitude = std::max(
                maximumMagnitude,
                std::abs(
                    buffer.getSample(0, sample)
                )
            );
        }

        expect(
            maximumMagnitude > 0.0f
        );
    }

    void testSyntheticHumMatchesAcrossStereoChannels()
    {
        beginTest(
            "Processor injects identical hum into stereo channels"
        );

        HumbugAudioProcessor processor;
        processor.setSyntheticHumEnabled(true);
        
        processor.prepareToPlay(
            sampleRate,
            blockSize
        );

        juce::AudioBuffer<float> buffer(
            2,
            blockSize
        );

        buffer.clear();

        juce::MidiBuffer midi;

        processor.processBlock(
            buffer,
            midi
        );

        for (
            int sample = 0;
            sample < blockSize;
            ++sample
        )
        {
            expectWithinAbsoluteError(
                buffer.getSample(0, sample),
                buffer.getSample(1, sample),
                0.000001f
            );
        }
    }
};

static ProcessorIntegrationTests
    processorIntegrationTests;