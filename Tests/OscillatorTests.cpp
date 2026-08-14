#include <JuceHeader.h>

#include "../Source/DSP/Oscillator.h"

class OscillatorTests final : public juce::UnitTest
{
public:
    OscillatorTests()
        : juce::UnitTest("Oscillator", "DSP")
    {
    }

    void runTest() override
    {
        testStartsAtZero();
        testQuarterCycle();
        testFullCycle();
        testReset();
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr double frequency = 60.0;

    void testStartsAtZero()
    {
        beginTest("Oscillator starts at zero phase");

        Oscillator oscillator;

        oscillator.prepare(sampleRate);
        oscillator.setFrequency(frequency);

        const auto sample = oscillator.processSample();

        expectWithinAbsoluteError(
            sample,
            0.0f,
            0.0001f
        );
    }

    void testQuarterCycle()
    {
        beginTest("60 Hz oscillator reaches +1 at quarter cycle");

        Oscillator oscillator;

        oscillator.prepare(sampleRate);
        oscillator.setFrequency(frequency);

        // 60 Hz at 48 kHz:
        // 800 samples per cycle,
        // 200 samples per quarter cycle.
        for (int sample = 0; sample < 200; ++sample)
            oscillator.processSample();

        const auto quarterCycleSample =
            oscillator.processSample();

        expectWithinAbsoluteError(
            quarterCycleSample,
            1.0f,
            0.0001f
        );
    }

    void testFullCycle()
    {
        beginTest("60 Hz oscillator completes a cycle in 800 samples");

        Oscillator oscillator;

        oscillator.prepare(sampleRate);
        oscillator.setFrequency(frequency);

        for (int sample = 0; sample < 800; ++sample)
            oscillator.processSample();

        const auto fullCycleSample =
            oscillator.processSample();

        expectWithinAbsoluteError(
            fullCycleSample,
            0.0f,
            0.0001f
        );
    }

    void testReset()
    {
        beginTest("Reset returns oscillator to zero phase");

        Oscillator oscillator;

        oscillator.prepare(sampleRate);
        oscillator.setFrequency(frequency);

        for (int sample = 0; sample < 137; ++sample)
            oscillator.processSample();

        oscillator.reset();

        expectWithinAbsoluteError(
            oscillator.processSample(),
            0.0f,
            0.0001f
        );
    }
};

static OscillatorTests oscillatorTests;