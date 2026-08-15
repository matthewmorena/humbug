#include <JuceHeader.h>

#include "../Source/DSP/FundamentalFrequencyDetector.h"
#include "../Source/DSP/HumGenerator.h"

class FundamentalFrequencyDetectorTests final
    : public juce::UnitTest
{
public:
    FundamentalFrequencyDetectorTests()
        : juce::UnitTest(
            "Fundamental Frequency Detector",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testDetectsSixtyHz();
        testDetectsFiftyHz();
        testDetectsNonGridFrequency();
        testDetectsNonGridFiftyHzFrequency();
        testDetectsWeakFundamental();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testDetectsSixtyHz()
    {
        beginTest(
            "Detector identifies 60 Hz hum"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(sampleRate);
        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f
        );

        generator.setHarmonicAmplitude(
            2,
            0.12f
        );

        generator.setHarmonicAmplitude(
            3,
            0.05f
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);

        expectWithinAbsoluteError(
            result.frequencyHz,
            60.0,
            0.001
        );
    }

    void testDetectsFiftyHz()
    {
        beginTest(
            "Detector identifies 50 Hz hum"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            50.0
        );

        generator.prepare(sampleRate);
        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f
        );

        generator.setHarmonicAmplitude(
            2,
            0.12f
        );

        generator.setHarmonicAmplitude(
            3,
            0.05f
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);

        expectWithinAbsoluteError(
            result.frequencyHz,
            50.0,
            0.001
        );
    }

    void testDetectsNonGridFrequency()
    {
        beginTest(
            "Detector identifies fundamental between search steps"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            59.73
        );

        generator.prepare(sampleRate);
        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f
        );

        generator.setHarmonicAmplitude(
            2,
            0.12f
        );

        generator.setHarmonicAmplitude(
            3,
            0.05f
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);

        expectWithinAbsoluteError(
            result.frequencyHz,
            59.73,
            0.01
        );
    }

    void testDetectsNonGridFiftyHzFrequency()
    {
        beginTest(
            "Detector identifies fundamental near 50 Hz between search steps"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            50.27
        );

        generator.prepare(sampleRate);
        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f
        );

        generator.setHarmonicAmplitude(
            2,
            0.12f
        );

        generator.setHarmonicAmplitude(
            3,
            0.05f
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);

        expectWithinAbsoluteError(
            result.frequencyHz,
            50.27,
            0.01
        );
    }

    void testDetectsWeakFundamental()
    {
        beginTest(
            "Detector identifies fundamental when fundamental is weak"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            59.73
        );

        generator.prepare(sampleRate);
        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.01f
        );

        generator.setHarmonicAmplitude(
            2,
            0.30f
        );

        generator.setHarmonicAmplitude(
            3,
            0.15f
        );

        generator.setHarmonicAmplitude(
            4,
            0.08f
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);

        expectWithinAbsoluteError(
            result.frequencyHz,
            59.73,
            0.01
        );
    }
};

static FundamentalFrequencyDetectorTests
    fundamentalFrequencyDetectorTests;