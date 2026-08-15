#include <JuceHeader.h>

#include "../Source/DSP/HumEstimator.h"
#include "../Source/DSP/HumGenerator.h"
#include "../Source/DSP/Oscillator.h"

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
        testHumWithWhiteNoise();
        testHumWithUnrelatedTone();
        testMultipleHarmonicsWithArbitraryWindow();
        testHumWithWhiteNoiseAndArbitraryWindow();
        testHumWithUnrelatedToneAndArbitraryWindow();
        testCorrectFrequencyProducesSmallResidual();
        testIncorrectFrequencyProducesLargerResidual();
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

    void testHumWithWhiteNoise()
    {
        beginTest(
            "Estimator recovers hum in white noise"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.2f
        );

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicAmplitude(
            2,
            0.08f
        );

        generator.setHarmonicPhase(
            2,
            0.41
        );

        generator.reset();

        // One second of audio at 48 kHz.
        // Contains exactly 60 cycles of a 60 Hz fundamental.
        juce::AudioBuffer<float> buffer(
            1,
            48000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        juce::Random random(12345);

        for (
            int sample = 0; 
            sample < buffer.getNumSamples(); 
            ++sample
        )
        {
            const auto noise = ((random.nextFloat() * 2.0f) - 1.0f) * 0.05f;
            buffer.addSample(
                0,
                sample,
                noise
            );
        }

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
            0.2f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.18,
            0.01
        );

        expectWithinAbsoluteError(
            result[1].amplitude,
            0.08f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[1].phase,
            0.41,
            0.01
        );
    }

    void testHumWithUnrelatedTone()
    {
        beginTest(
            "Estimator recovers hum with unrelated tonal interference"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.2f
        );

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicAmplitude(
            2,
            0.08f
        );

        generator.setHarmonicPhase(
            2,
            0.41
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            48000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        Oscillator interferingTone;

        interferingTone.prepare(sampleRate);
        interferingTone.setFrequency(440.0);
        interferingTone.setPhase(0.27);

        constexpr float interferenceAmplitude = 0.15f;

        for (
            int sample = 0;
            sample < buffer.getNumSamples();
            ++sample
        )
        {
            buffer.addSample(
                0,
                sample,
                interferenceAmplitude
                    * interferingTone.processSample()
            );
        }

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
            0.2f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.18,
            0.01
        );

        expectWithinAbsoluteError(
            result[1].amplitude,
            0.08f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[1].phase,
            0.41,
            0.01
        );
    }

    void testMultipleHarmonicsWithArbitraryWindow()
    {
        beginTest(
            "Estimator handles non-integer-cycle analysis window"
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

        // 2000 samples at 48 kHz is 41.67 ms,
        // or 2.5 cycles of a 60 Hz fundamental.
        //
        // The original correlation estimator failed
        // for this window length.
        juce::AudioBuffer<float> buffer(
            1,
            2000 // Can go to 1200 but keeping at 2000 for consistency
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
    }

    void testHumWithWhiteNoiseAndArbitraryWindow()
    {
        beginTest(
            "Estimator recovers hum in white noise and non-integer-cycle analysis window..."
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.2f
        );

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicAmplitude(
            2,
            0.08f
        );

        generator.setHarmonicPhase(
            2,
            0.41
        );

        generator.reset();

        // 41.67 ms of audio at 48 kHz.
        // Contains exactly 2.5 cycles of a 60 Hz fundamental.
        juce::AudioBuffer<float> buffer(
            1,
            2000 // Can go to 1200 but keeping at 2000 for consistency
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        juce::Random random(12345);

        for (
            int sample = 0; 
            sample < buffer.getNumSamples(); 
            ++sample
        )
        {
            const auto noise = ((random.nextFloat() * 2.0f) - 1.0f) * 0.05f;
            buffer.addSample(
                0,
                sample,
                noise
            );
        }

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
            0.2f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.18,
            0.01
        );

        expectWithinAbsoluteError(
            result[1].amplitude,
            0.08f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[1].phase,
            0.41,
            0.01
        );
    }

    void testHumWithUnrelatedToneAndArbitraryWindow()
    {
        beginTest(
            "Estimator recovers hum with unrelated tonal interference and non-integer-cycle analysis window..."
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            60.0
        );

        generator.prepare(sampleRate);

        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.2f
        );

        generator.setHarmonicPhase(
            1,
            0.18
        );

        generator.setHarmonicAmplitude(
            2,
            0.08f
        );

        generator.setHarmonicPhase(
            2,
            0.41
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            2000 // 2000 samples seems to be the limitation at this point
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        Oscillator interferingTone;

        interferingTone.prepare(sampleRate);
        interferingTone.setFrequency(440.0);
        interferingTone.setPhase(0.27);

        constexpr float interferenceAmplitude = 0.15f;

        for (
            int sample = 0;
            sample < buffer.getNumSamples();
            ++sample
        )
        {
            buffer.addSample(
                0,
                sample,
                interferenceAmplitude
                    * interferingTone.processSample()
            );
        }

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
            0.2f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[0].phase,
            0.18,
            0.01
        );

        expectWithinAbsoluteError(
            result[1].amplitude,
            0.08f,
            0.005f
        );

        expectWithinAbsoluteError(
            result[1].phase,
            0.41,
            0.01
        );
    }

    void testCorrectFrequencyProducesSmallResidual()
    {
        beginTest(
            "Estimator produces small residual given correct frequency"
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

        // 2000 samples at 48 kHz is 41.67 ms,
        // or 2.5 cycles of a 60 Hz fundamental.
        //
        // The original correlation estimator failed
        // for this window length.
        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        HumEstimator estimator;

        const auto fit =
            estimator.fit(
                buffer,
                0,
                sampleRate,
                60.0
            );

        expect(fit.valid);

        expect(
            fit.residualEnergy < 0.0001
        );
    }

    void testIncorrectFrequencyProducesLargerResidual()
    {
        beginTest(
            "Estimator produces larger residual given incorrect frequency"
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

        // 2000 samples at 48 kHz is about 41.67 ms,
        // or 2.5 cycles of a 60 Hz fundamental.
        juce::AudioBuffer<float> buffer(
            1,
            2000
        );

        buffer.clear();

        generator.addToBuffer(buffer);

        HumEstimator estimator;

        const auto correctFit =
            estimator.fit(
                buffer,
                0,
                sampleRate,
                60.0
            );

        const auto incorrectFit =
            estimator.fit(
                buffer,
                0,
                sampleRate,
                59.0
            );

        expect(correctFit.valid);
        expect(incorrectFit.valid);

        expect(
            correctFit.residualEnergy
                < incorrectFit.residualEnergy
        );
    }
};

static HumEstimatorTests humEstimatorTests;