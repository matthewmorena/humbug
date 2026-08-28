#include <JuceHeader.h>

#include "../Source/DSP/HumAnalyzer.h"
#include "../Source/DSP/HumGenerator.h"

#include <cmath>
#include <numbers>

class HumAnalyzerTests final
    : public juce::UnitTest
{
public:
    HumAnalyzerTests()
        : juce::UnitTest(
            "Hum Analyzer",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testAnalyzesHumFromMixedSignal();
        testReportsNoHumForUnrelatedSignal();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testAnalyzesHumFromMixedSignal()
    {
        beginTest(
            "Analyzer learns hum model from mixed signal"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(
            sampleRate
        );

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

        constexpr int analysisSamples =
            12000;

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        analysisBuffer.clear();

        generator.addToBuffer(
            analysisBuffer
        );

        constexpr double cleanFrequencyHz =
            997.0;

        constexpr float cleanAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        auto* samples =
            analysisBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            const auto time =
                static_cast<double>(sample)
                / sampleRate;

            const auto cleanSample =
                cleanAmplitude
                * static_cast<float>(
                    std::sin(
                        twoPi
                        * cleanFrequencyHz
                        * time
                    )
                );

            samples[sample] +=
                cleanSample;
        }

        HumAnalyzer analyzer;

        const auto result =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            result.valid
        );

        expect(
            result.humDetected
        );

        expectWithinAbsoluteError(
            result.frequencyHz,
            60.0,
            0.01
        );

        expectWithinAbsoluteError(
            result.harmonics[0].frequencyHz,
            60.0,
            0.01
        );

        expectWithinAbsoluteError(
            result.harmonics[1].frequencyHz,
            120.0,
            0.02
        );

        expectWithinAbsoluteError(
            result.harmonics[2].frequencyHz,
            180.0,
            0.03
        );

        expectWithinAbsoluteError(
            result.harmonics[0].amplitude,
            0.30f,
            0.01f
        );

        expectWithinAbsoluteError(
            result.harmonics[1].amplitude,
            0.15f,
            0.01f
        );

        expectWithinAbsoluteError(
            result.harmonics[2].amplitude,
            0.08f,
            0.01f
        );
    }

    void testReportsNoHumForUnrelatedSignal()
    {
        beginTest(
            "Analyzer reports no hum for unrelated signal"
        );

        constexpr int analysisSamples =
            12000;

        constexpr double signalFrequencyHz =
            997.0;

        constexpr float signalAmplitude =
            0.20f;

        constexpr auto twoPi =
            2.0 * std::numbers::pi;

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        auto* samples =
            analysisBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < analysisSamples;
            ++sample
        )
        {
            const auto time =
                static_cast<double>(sample)
                / sampleRate;

            samples[sample] =
                signalAmplitude
                * static_cast<float>(
                    std::sin(
                        twoPi
                        * signalFrequencyHz
                        * time
                    )
                );
        }

        HumAnalyzer analyzer;

        const auto result =
            analyzer.analyze(
                analysisBuffer,
                0,
                sampleRate
            );

        expect(
            result.valid
        );

        expect(
            !result.humDetected
        );

        for (const auto& harmonic : result.harmonics)
        {
            expectWithinAbsoluteError(
                harmonic.amplitude,
                0.0f,
                0.000001f
            );
        }
    }
};

static HumAnalyzerTests
    humAnalyzerTests;