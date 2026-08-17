#include <JuceHeader.h>

#include "../Source/DSP/HumReconstructor.h"

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
};

static HumReconstructorTests
    humReconstructorTests;