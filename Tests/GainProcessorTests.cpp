#include <JuceHeader.h>

#include "../Source/PluginProcessor.h"

class GainProcessorTests final : public juce::UnitTest
{
public:
    GainProcessorTests()
        : juce::UnitTest("Gain Processor", "DSP")
    {
    }

    void runTest() override
    {
        testUnityGain();
        testMinusSixDb();
        testStereoProcessing();
        testStateSerialization();
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 512;

    void testUnityGain()
    {
        beginTest("0 dB leaves audio unchanged");

        HumbugAudioProcessor processor;
        processor.prepareToPlay(sampleRate, blockSize);

        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;

        buffer.clear();

        buffer.setSample(0, 0, 0.5f);
        buffer.setSample(1, 0, -0.25f);

        processor.processBlock(buffer, midi);

        expectWithinAbsoluteError(
            buffer.getSample(0, 0),
            0.5f,
            0.0001f
        );

        expectWithinAbsoluteError(
            buffer.getSample(1, 0),
            -0.25f,
            0.0001f
        );
    }

    void testMinusSixDb()
    {
        beginTest("-6 dB applies expected gain");

        HumbugAudioProcessor processor;
        processor.prepareToPlay(sampleRate, blockSize);

        auto* gain =
            processor.getParameterState().getParameter(
                ParameterIDs::gain
            );

        expect(gain != nullptr);

        if (gain == nullptr)
            return;

        gain->setValueNotifyingHost(
            gain->convertTo0to1(-6.0f)
        );

        juce::MidiBuffer midi;

        // Advance through the 20 ms smoothing ramp.
        juce::AudioBuffer<float> warmupBuffer(2, blockSize);

        for (int block = 0; block < 2; ++block)
        {
            warmupBuffer.clear();

            for (int sample = 0; sample < blockSize; ++sample)
            {
                warmupBuffer.setSample(0, sample, 1.0f);
                warmupBuffer.setSample(1, sample, 1.0f);
            }

            processor.processBlock(warmupBuffer, midi);
        }

        // Test using fresh, unprocessed samples.
        juce::AudioBuffer<float> testBuffer(2, blockSize);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            testBuffer.setSample(0, sample, 1.0f);
            testBuffer.setSample(1, sample, 1.0f);
        }

        processor.processBlock(testBuffer, midi);

        constexpr float expectedGain = 0.501187f;

        expectWithinAbsoluteError(
            testBuffer.getSample(0, blockSize - 1),
            expectedGain,
            0.001f
        );
    }

    void testStereoProcessing()
    {
        beginTest("Gain processes both stereo channels");

        HumbugAudioProcessor processor;
        processor.prepareToPlay(sampleRate, blockSize);

        auto* gain =
            processor.getParameterState().getParameter(
                ParameterIDs::gain
            );

        expect(gain != nullptr);

        if (gain == nullptr)
            return;

        gain->setValueNotifyingHost(
            gain->convertTo0to1(-6.0f)
        );

        juce::MidiBuffer midi;

        // Allow smoothing to reach the target gain.
        juce::AudioBuffer<float> warmupBuffer(2, blockSize);

        for (int block = 0; block < 2; ++block)
        {
            warmupBuffer.clear();

            for (int sample = 0; sample < blockSize; ++sample)
            {
                warmupBuffer.setSample(0, sample, 1.0f);
                warmupBuffer.setSample(1, sample, 1.0f);
            }

            processor.processBlock(warmupBuffer, midi);
        }

        juce::AudioBuffer<float> testBuffer(2, blockSize);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            testBuffer.setSample(0, sample, 1.0f);
            testBuffer.setSample(1, sample, 0.5f);
        }

        processor.processBlock(testBuffer, midi);

        constexpr float expectedGain = 0.501187f;

        expectWithinAbsoluteError(
            testBuffer.getSample(0, blockSize - 1),
            expectedGain,
            0.001f
        );

        expectWithinAbsoluteError(
            testBuffer.getSample(1, blockSize - 1),
            0.5f * expectedGain,
            0.001f
        );
    }

    void testStateSerialization()
    {
        beginTest("Gain state serializes and restores");

        HumbugAudioProcessor sourceProcessor;

        auto* sourceGain =
            sourceProcessor.getParameterState().getParameter(
                ParameterIDs::gain
            );

        expect(sourceGain != nullptr);

        if (sourceGain == nullptr)
            return;

        sourceGain->setValueNotifyingHost(
            sourceGain->convertTo0to1(-13.7f)
        );

        juce::MemoryBlock state;
        sourceProcessor.getStateInformation(state);

        expect(state.getSize() > 0);

        HumbugAudioProcessor restoredProcessor;

        restoredProcessor.setStateInformation(
            state.getData(),
            static_cast<int>(state.getSize())
        );

        const auto* restoredGain =
            restoredProcessor.getParameterState()
                .getRawParameterValue(
                    ParameterIDs::gain
                );

        expect(restoredGain != nullptr);

        if (restoredGain == nullptr)
            return;

        expectWithinAbsoluteError(
            restoredGain->load(),
            -13.7f,
            0.001f
        );
    }
};

static GainProcessorTests gainProcessorTests;