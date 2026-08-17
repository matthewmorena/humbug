#include <JuceHeader.h>

#include "../Source/DSP/HumReconstructor.h"
#include "../Source/DSP/HumGenerator.h"

#include <cmath>
#include <numbers>

class HumReconstructorTests final
    : public juce::UnitTest
{
public:
    HumReconstructorTests()
        : juce::UnitTest(
            "Hum Reconstructor",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testReconstructsKnownModel();
        testReconstructsEstimatedHum();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testReconstructsKnownModel()
    {
        beginTest(
            "Reconstructor reproduces a known harmonic model"
        );

        HumEstimator::Result model {};

        model[0].frequencyHz = 60.0;
        model[0].amplitude = 0.30f;
        model[0].phase = 0.08;

        model[1].frequencyHz = 120.0;
        model[1].amplitude = 0.12f;
        model[1].phase = 0.31;

        model[2].frequencyHz = 180.0;
        model[2].amplitude = 0.05f;
        model[2].phase = 0.73;

        HumReconstructor reconstructor;

        reconstructor.prepare(sampleRate);
        reconstructor.setModel(model);

        constexpr int numSamples = 2000;

        for (
            int sample = 0;
            sample < numSamples;
            ++sample
        )
        {
            const auto time =
                static_cast<double>(sample)
                / sampleRate;

            float expectedSample = 0.0f;

            for (const auto& harmonic : model)
            {
                expectedSample +=
                    harmonic.amplitude
                    * static_cast<float>(
                        std::sin(
                            2.0
                            * std::numbers::pi
                            * (
                                harmonic.frequencyHz
                                    * time
                                + harmonic.phase
                            )
                        )
                    );
            }

            expectWithinAbsoluteError(
                reconstructor.processSample(),
                expectedSample,
                0.00001f
            );
        }
    }

    void testReconstructsEstimatedHum()
    {
        beginTest(
            "Reconstructor reproduces estimated synthetic hum"
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
            0.15f
        );

        generator.setHarmonicAmplitude(
            3,
            0.08f
        );

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicPhase(
            2,
            0.25
        );

        generator.setHarmonicPhase(
            3,
            0.41
        );

        generator.reset();

        constexpr int numSamples = 2000;

        // 41.67 ms of audio at 48 kHz.
        // Contains exactly 2.5 cycles of a 60 Hz fundamental.
        juce::AudioBuffer<float> buffer(
            1,
            numSamples
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        const auto* samples =
            buffer.getReadPointer(0);

        HumEstimator estimator;

        const auto estimatedModel =
            estimator.estimate(
                buffer,
                0,
                sampleRate,
                60.0
            );

        HumReconstructor reconstructor;

        reconstructor.prepare(sampleRate);
        reconstructor.setModel(
            estimatedModel
        );

        for (
            int sample = 0;
            sample < numSamples;
            ++sample
        )
        {
            const auto reconstructedSample =
                reconstructor.processSample();

            expectWithinAbsoluteError(
                reconstructedSample,
                samples[sample],
                0.0001f
            );
        }
    }
};

static HumReconstructorTests
    humReconstructorTests;