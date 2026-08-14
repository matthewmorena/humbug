#include <JuceHeader.h>

#include "../Source/DSP/HumEstimator.h"
#include "../Source/DSP/HumGenerator.h"

class HumEstimatorTests final
    : public juce::UnitTest
{
public:
    HumEstimatorTests()
        : juce::UnitTest(
            "Hum Estimator",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testFundamentalAmplitude();
        testFundamentalPhase();
        testMultipleHarmonics();
        testFiftyHzHum();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testFundamentalAmplitude()
    {
        beginTest(
            "Estimator recovers fundamental amplitude"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.4f
        );

        generator.setHarmonicPhase(
            1,
            0.0
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            8000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        HumEstimator estimator;

        const auto result =
            estimator.estimate(
                buffer,
                0,
                sampleRate,
                60.0
            );

        expectWithinAbsoluteError(
            result[0].amplitude,
            0.4f,
            0.0001f
        );
    }

    void testFundamentalPhase()
    {
        beginTest(
            "Estimator recovers fundamental phase"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.35f
        );

        generator.setHarmonicPhase(
            1,
            0.23
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            8000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        HumEstimator estimator;

        const auto result =
            estimator.estimate(
                buffer,
                0,
                sampleRate,
                60.0
            );

        expectWithinAbsoluteError(
            result[0].amplitude,
            0.35f,
            0.0001f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.23,
            0.0001
        );
    }

    void testMultipleHarmonics()
    {
        beginTest(
            "Estimator separates multiple harmonics"
        );

        HumGenerator generator;
        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.30f
        );

        generator.setHarmonicPhase(
            1,
            0.08
        );

        generator.setHarmonicAmplitude(
            2,
            0.12f
        );

        generator.setHarmonicPhase(
            2,
            0.31
        );

        generator.setHarmonicAmplitude(
            3,
            0.05f
        );

        generator.setHarmonicPhase(
            3,
            0.73
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            8000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        HumEstimator estimator;

        const auto result =
            estimator.estimate(
                buffer,
                0,
                sampleRate,
                60.0
            );

        expectWithinAbsoluteError(
            result[0].amplitude,
            0.30f,
            0.0001f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.08,
            0.0001
        );

        expectWithinAbsoluteError(
            result[1].amplitude,
            0.12f,
            0.0001f
        );

        expectWithinAbsoluteError(
            result[1].phase,
            0.31,
            0.0001
        );

        expectWithinAbsoluteError(
            result[2].amplitude,
            0.05f,
            0.0001f
        );

        expectWithinAbsoluteError(
            result[2].phase,
            0.73,
            0.0001
        );

        // No fourth harmonic was generated.
        expectWithinAbsoluteError(
            result[3].amplitude,
            0.0f,
            0.0001f
        );
    }

    void testFiftyHzHum()
    {
        beginTest(
            "Estimator supports 50 Hz hum"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            50.0
        );

        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.2f
        );

        generator.setHarmonicPhase(
            1,
            0.42
        );

        generator.reset();

        // 50 Hz at 48 kHz = 960 samples/cycle.
        // 9600 samples = exactly 10 cycles.
        juce::AudioBuffer<float> buffer(
            1,
            9600
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        HumEstimator estimator;

        const auto result =
            estimator.estimate(
                buffer,
                0,
                sampleRate,
                50.0
            );

        expectWithinAbsoluteError(
            result[0].amplitude,
            0.2f,
            0.0001f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.42,
            0.0001
        );
    }
};

static HumEstimatorTests humEstimatorTests;