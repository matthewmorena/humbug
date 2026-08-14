#include <JuceHeader.h>

#include "../Source/DSP/HumGenerator.h"

class HumGeneratorTests final : public juce::UnitTest
{
public:
    HumGeneratorTests()
        : juce::UnitTest("Hum Generator", "DSP")
    {
    }

    void runTest() override
    {
        testDefaultFundamental();
        testSecondHarmonic();
        testHarmonicSum();
        testFiftyHzFundamental();
        testHarmonicPhase();
        testReset();
    }

private:
    static constexpr double sampleRate = 48000.0;

    void testDefaultFundamental()
    {
        beginTest(
            "Default generator produces 60 Hz fundamental"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        // 60 Hz at 48 kHz:
        // 800 samples/cycle,
        // 200 samples to quarter-cycle.
        for (int sample = 0; sample < 200; ++sample)
            generator.processSample();

        expectWithinAbsoluteError(
            generator.processSample(),
            1.0f,
            0.0001f
        );
    }

    void testSecondHarmonic()
    {
        beginTest(
            "Second harmonic produces 120 Hz"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.clearHarmonics();
        generator.setHarmonicAmplitude(2, 1.0f);

        // 120 Hz at 48 kHz:
        // 400 samples/cycle,
        // 100 samples to quarter-cycle.
        for (int sample = 0; sample < 100; ++sample)
            generator.processSample();

        expectWithinAbsoluteError(
            generator.processSample(),
            1.0f,
            0.0001f
        );
    }

    void testHarmonicSum()
    {
        beginTest(
            "Harmonics are summed with their amplitudes"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            1.0f
        );

        generator.setHarmonicAmplitude(
            2,
            0.5f
        );

        // At sample 100:
        //
        // 60 Hz component:
        // sin(pi / 4) ~= 0.7071068
        //
        // 120 Hz component:
        // 0.5 * sin(pi / 2) = 0.5
        //
        // Sum ~= 1.2071068
        for (int sample = 0; sample < 100; ++sample)
            generator.processSample();

        expectWithinAbsoluteError(
            generator.processSample(),
            1.2071068f,
            0.0001f
        );
    }

    void testFiftyHzFundamental()
    {
        beginTest(
            "Generator supports 50 Hz fundamental"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            50.0
        );

        generator.prepare(sampleRate);

        // 50 Hz at 48 kHz:
        // 960 samples/cycle,
        // 240 samples to quarter-cycle.
        for (int sample = 0; sample < 240; ++sample)
            generator.processSample();

        expectWithinAbsoluteError(
            generator.processSample(),
            1.0f,
            0.0001f
        );
    }

    void testHarmonicPhase()
    {
        beginTest(
            "Harmonics support independent phase offsets"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            2,
            0.5f
        );

        generator.setHarmonicPhase(
            2,
            0.25
        );

        // sin(pi / 2) == 1,
        // scaled by amplitude 0.5.
        expectWithinAbsoluteError(
            generator.processSample(),
            0.5f,
            0.0001f
        );
    }

    void testReset()
    {
        beginTest(
            "Reset restores all harmonic phases"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.setHarmonicAmplitude(
            2,
            0.5f
        );

        for (int sample = 0; sample < 137; ++sample)
            generator.processSample();

        generator.reset();

        // All sine oscillators restart at phase zero.
        expectWithinAbsoluteError(
            generator.processSample(),
            0.0f,
            0.0001f
        );
    }
};

static HumGeneratorTests humGeneratorTests;