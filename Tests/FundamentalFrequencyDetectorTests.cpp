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
        testDetectsNonGridFrequencyWithWhiteNoise();
        testDetectsNonGridFrequencyWithUnrelatedTone();
        testDetectsNonGridFrequencyWithWeakFundamentalAndUnrelatedTone();
        testRejectsSilence();
        testRejectsWhiteNoise();
        testRejectsUnrelatedTone();
        testRejectsSingleHumHarmonic();
        testDetectsTwoHarmonicHum();
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
        expect(result.humDetected);

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
        expect(result.humDetected);

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
        expect(result.humDetected);

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
        expect(result.humDetected);

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
        expect(result.humDetected);

        expectWithinAbsoluteError(
            result.frequencyHz,
            59.73,
            0.01
        );
    }

    void testDetectsNonGridFrequencyWithWhiteNoise()
    {
        beginTest(
            "Detector identifies fundamental between search steps with white noise"
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
            12000
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

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);
        expect(result.humDetected);

        expectWithinAbsoluteError(
            result.frequencyHz,
            59.73,
            0.01
        );
    }

    void testDetectsNonGridFrequencyWithUnrelatedTone()
    {
        beginTest(
            "Detector identifies fundamental between search steps with unrelated tone"
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
            12000
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

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);
        expect(result.humDetected);

        expectWithinAbsoluteError(
            result.frequencyHz,
            59.73,
            0.01
        );
    }

    void testDetectsNonGridFrequencyWithWeakFundamentalAndUnrelatedTone()
    {
        beginTest(
            "Detector identifies fundamental between search steps with weak fundamental and unrelated tone"
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

        generator.setHarmonicPhase(
            1,
            0.13
        );

        generator.setHarmonicPhase(
            2,
            0.47
        );

        generator.setHarmonicPhase(
            3,
            0.81
        );

        generator.setHarmonicPhase(
            4,
            0.29
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            12000
        );

        constexpr std::array<double, 4> interferencePhases {
            0.0,
            0.17,
            0.43,
            0.79
        };

        for (const auto interferencePhase : interferencePhases)
        {
            buffer.clear();

            generator.addToBuffer(buffer);

            Oscillator interferingTone;

            interferingTone.prepare(sampleRate);
            interferingTone.setFrequency(440.0);
            interferingTone.setPhase(interferencePhase);

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

            FundamentalFrequencyDetector detector;

            const auto result =
                detector.detect(
                    buffer,
                    0,
                    sampleRate
                );

            expect(result.valid);
            expect(result.humDetected);

            expectWithinAbsoluteError(
                result.frequencyHz,
                59.73,
                0.01
            );

        }
    }

    void testRejectsSilence()
    {
        beginTest(
            "Detector does not report hum for silence"
        );

        juce::AudioBuffer<float> buffer(
            1,
            12000
        );

        buffer.clear();

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);
        expect(!result.humDetected);
    }

    void testRejectsWhiteNoise()
    {
        beginTest(
            "Detector does not report hum for white noise"
        );

        juce::AudioBuffer<float> buffer(
            1,
            12000
        );

        buffer.clear();

        juce::Random random(12345);

        for (
            int sample = 0;
            sample < buffer.getNumSamples();
            ++sample
        )
        {
            const auto noise =
                ((random.nextFloat() * 2.0f) - 1.0f)
                * 0.05f;

            buffer.setSample(
                0,
                sample,
                noise
            );
        }

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);
        expect(!result.humDetected);
    }

    void testRejectsUnrelatedTone()
    {
        beginTest(
            "Detector does not report hum for unrelated tone"
        );

        juce::AudioBuffer<float> buffer(
            1,
            12000
        );

        buffer.clear();

        Oscillator oscillator;

        oscillator.prepare(sampleRate);
        oscillator.setFrequency(440.0);
        oscillator.setPhase(0.27);

        for (
            int sample = 0;
            sample < buffer.getNumSamples();
            ++sample
        )
        {
            buffer.setSample(
                0,
                sample,
                0.15f * oscillator.processSample()
            );
        }

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);
        expect(!result.humDetected);
    }

    void testRejectsSingleHumHarmonic()
    {
        beginTest(
            "Detector does not report hum for isolated hum harmonic"
        );

        juce::AudioBuffer<float> buffer(
            1,
            12000
        );

        buffer.clear();

        Oscillator oscillator;

        oscillator.prepare(sampleRate);
        oscillator.setFrequency(
            420.0
        );
        oscillator.setPhase(
            0.27
        );

        constexpr float amplitude =
            0.15f;

        for (
            int sample = 0;
            sample < buffer.getNumSamples();
            ++sample
        )
        {
            buffer.setSample(
                0,
                sample,
                amplitude
                    * oscillator.processSample()
            );
        }

        FundamentalFrequencyDetector detector;

        const auto result =
            detector.detect(
                buffer,
                0,
                sampleRate
            );

        expect(result.valid);
        expect(!result.humDetected);
    }

    void testDetectsTwoHarmonicHum()
    {
        beginTest(
            "Detector reports hum when two harmonics support fundamental"
        );

        HumGenerator generator;

        generator.setFundamentalFrequency(
            59.73
        );

        generator.prepare(sampleRate);
        generator.clearHarmonics();

        generator.setHarmonicAmplitude(
            1,
            0.20f
        );

        generator.setHarmonicAmplitude(
            2,
            0.08f
        );

        generator.reset();

        juce::AudioBuffer<float> buffer(
            1,
            12000
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
        expect(result.humDetected);

        expectEquals(
            static_cast<int>(
                result.supportedHarmonics
            ),
            2
        );

        expectWithinAbsoluteError(
            result.frequencyHz,
            59.73,
            0.01
        );
    }
};

static FundamentalFrequencyDetectorTests
    fundamentalFrequencyDetectorTests;