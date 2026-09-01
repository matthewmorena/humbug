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
        testTracksSlowFrequencyDrift();
        testReportsNoHumForUnrelatedSignal();

        testAnalyzesHumNearReferenceFrequency();
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

    void testTracksSlowFrequencyDrift()
    {
        beginTest(
            "Analyzer tracks slow fundamental frequency drift"
        );

        constexpr int analysisSamples =
            12000;

        constexpr int driftSamples =
            240000;

        constexpr int trackingWindows =
            5;

        constexpr int windowSpacingSamples =
            (driftSamples - analysisSamples)
            / (trackingWindows - 1);

        constexpr double startFrequencyHz =
            60.0;

        constexpr double endFrequencyHz =
            60.2;

        HumGenerator generator;

        generator.setFundamentalFrequency(
            startFrequencyHz
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

        juce::AudioBuffer<float> driftBuffer(
            1,
            driftSamples
        );

        auto* driftData =
            driftBuffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < driftSamples;
            ++sample
        )
        {
            const auto progress =
                static_cast<double>(sample)
                / static_cast<double>(
                    driftSamples - 1
                );

            const auto frequencyHz =
                startFrequencyHz
                + progress
                * (
                    endFrequencyHz
                    - startFrequencyHz
                );

            generator.setFundamentalFrequency(
                frequencyHz
            );

            driftData[sample] =
                generator.processSample();
        }

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        HumAnalyzer analyzer;

        double firstEstimatedFrequencyHz =
            0.0;

        double lastEstimatedFrequencyHz =
            0.0;

        for (
            int window = 0;
            window < trackingWindows;
            ++window
        )
        {
            const auto windowStartSample =
                window
                * windowSpacingSamples;

            analysisBuffer.copyFrom(
                0,
                0,
                driftBuffer,
                0,
                windowStartSample,
                analysisSamples
            );

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

            const auto midpointSample =
                static_cast<double>(
                    windowStartSample
                )
                + 0.5
                * static_cast<double>(
                    analysisSamples - 1
                );

            const auto midpointProgress =
                midpointSample
                / static_cast<double>(
                    driftSamples - 1
                );

            const auto expectedFrequencyHz =
                startFrequencyHz
                + midpointProgress
                * (
                    endFrequencyHz
                    - startFrequencyHz
                );

            expectWithinAbsoluteError(
                result.frequencyHz,
                expectedFrequencyHz,
                0.02
            );

            if (window == 0)
            {
                firstEstimatedFrequencyHz =
                    result.frequencyHz;
            }

            if (
                window
                == trackingWindows - 1
            )
            {
                lastEstimatedFrequencyHz =
                    result.frequencyHz;
            }
        }

        expect(
            lastEstimatedFrequencyHz
            - firstEstimatedFrequencyHz
            > 0.15
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

    void testAnalyzesHumNearReferenceFrequency()
    {
        beginTest(
            "Analyzer learns hum model near reference frequency"
        );

        constexpr int analysisSamples =
            12000;

        constexpr double actualFrequencyHz =
            60.18;

        constexpr double referenceFrequencyHz =
            60.0;

        constexpr double searchRadiusHz =
            0.3;

        HumGenerator generator;

        generator.setFundamentalFrequency(
            actualFrequencyHz
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

        juce::AudioBuffer<float> analysisBuffer(
            1,
            analysisSamples
        );

        analysisBuffer.clear();

        generator.addToBuffer(
            analysisBuffer
        );

        // Include the same kind of unrelated material
        // tracking may encounter in production.
        constexpr double desiredFrequencyHz =
            997.0;

        constexpr float desiredAmplitude =
            0.10f;

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

            samples[sample] +=
                desiredAmplitude
                * static_cast<float>(
                    std::sin(
                        juce::MathConstants<double>::twoPi
                        * desiredFrequencyHz
                        * time
                    )
                );
        }

        HumAnalyzer analyzer;

        const auto result =
            analyzer.analyzeNear(
                analysisBuffer,
                0,
                sampleRate,
                referenceFrequencyHz,
                searchRadiusHz
            );

        expect(
            result.valid
        );

        expect(
            result.humDetected
        );

        expectWithinAbsoluteError(
            result.frequencyHz,
            actualFrequencyHz,
            0.01
        );

        expectWithinAbsoluteError(
            result.harmonics[0].frequencyHz,
            actualFrequencyHz,
            0.01
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
};

static HumAnalyzerTests
    humAnalyzerTests;