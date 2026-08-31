#include <JuceHeader.h>

#include "../Source/PluginProcessor.h"
#include "../Source/DSP/HumGenerator.h"

#include <algorithm>
#include <cmath>

class ProcessorIntegrationTests final
    : public juce::UnitTest
{
public:
    ProcessorIntegrationTests()
        : juce::UnitTest(
            "Processor Integration",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testSyntheticHumIsGenerated();
        testSyntheticHumMatchesAcrossStereoChannels();
        testRealtimeLearnCancelsSyntheticHum();
        testRealtimeRelearnDisablesCancellation();
        testProcessorAdaptiveTrackingMaintainsCancellationDuringFrequencyDrift();
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 512;

    void testSyntheticHumIsGenerated()
    {
        beginTest(
            "Processor generates synthetic hum"
        );

        HumbugAudioProcessor processor;
        processor.setSyntheticHumEnabled(true);

        processor.prepareToPlay(
            sampleRate,
            blockSize
        );

        juce::AudioBuffer<float> buffer(
            2,
            blockSize
        );

        buffer.clear();

        juce::MidiBuffer midi;

        processor.processBlock(
            buffer,
            midi
        );

        float maximumMagnitude = 0.0f;

        for (
            int sample = 0;
            sample < blockSize;
            ++sample
        )
        {
            maximumMagnitude = std::max(
                maximumMagnitude,
                std::abs(
                    buffer.getSample(0, sample)
                )
            );
        }

        expect(
            maximumMagnitude > 0.0f
        );
    }

    void testSyntheticHumMatchesAcrossStereoChannels()
    {
        beginTest(
            "Processor injects identical hum into stereo channels"
        );

        HumbugAudioProcessor processor;
        processor.setSyntheticHumEnabled(true);
        
        processor.prepareToPlay(
            sampleRate,
            blockSize
        );

        juce::AudioBuffer<float> buffer(
            2,
            blockSize
        );

        buffer.clear();

        juce::MidiBuffer midi;

        processor.processBlock(
            buffer,
            midi
        );

        for (
            int sample = 0;
            sample < blockSize;
            ++sample
        )
        {
            expectWithinAbsoluteError(
                buffer.getSample(0, sample),
                buffer.getSample(1, sample),
                0.000001f
            );
        }
    }

    void testRealtimeLearnCancelsSyntheticHum()
    {
        beginTest(
            "Processor learns and cancels synthetic hum"
        );

        HumbugAudioProcessor processor;

        auto layout =
            processor.getBusesLayout();

        layout.getChannelSet(
            true,
            0
        ) = juce::AudioChannelSet::mono();

        layout.getChannelSet(
            false,
            0
        ) = juce::AudioChannelSet::mono();

        const auto monoLayoutSet =
            processor.setBusesLayout(
                layout
            );

        expect(
            monoLayoutSet
        );

        if (!monoLayoutSet)
            return;

        processor.setSyntheticHumEnabled(
            true
        );

        processor.prepareToPlay(
            sampleRate,
            blockSize
        );

        juce::AudioBuffer<float> buffer(
            1,
            blockSize
        );

        juce::MidiBuffer midi;

        constexpr int baselineBlocks =
            16;

        double baselineMeanSquare =
            0.0;

        for (
            int block = 0;
            block < baselineBlocks;
            ++block
        )
        {
            buffer.clear();

            processor.processBlock(
                buffer,
                midi
            );

            const auto blockRms =
                buffer.getRMSLevel(
                    0,
                    0,
                    buffer.getNumSamples()
                );

            baselineMeanSquare +=
                static_cast<double>(
                    blockRms
                )
                * blockRms;
        }

        const auto baselineRms =
            std::sqrt(
                baselineMeanSquare
                / static_cast<double>(
                    baselineBlocks
                )
            );

        expect(
            baselineRms > 0.0
        );

        processor.requestLearn();

        constexpr double timeoutMs =
            5000.0;

        constexpr double requiredAttenuationDb =
            -40.0;

        const auto startTime =
            juce::Time::getMillisecondCounterHiRes();

        double bestOutputRms =
            baselineRms;

        bool cancellationObserved =
            false;

        while (
            (
                juce::Time::getMillisecondCounterHiRes()
                - startTime
            ) < timeoutMs
        )
        {
            buffer.clear();

            processor.processBlock(
                buffer,
                midi
            );

            const auto blockRms =
                static_cast<double>(
                    buffer.getRMSLevel(
                        0,
                        0,
                        buffer.getNumSamples()
                    )
                );

            bestOutputRms =
                std::min(
                    bestOutputRms,
                    blockRms
                );

            if (
                blockRms <= 0.0
            )
            {
                cancellationObserved =
                    true;

                break;
            }

            const auto attenuationDb =
                20.0
                * std::log10(
                    blockRms
                    / baselineRms
                );

            if (
                attenuationDb
                    < requiredAttenuationDb
            )
            {
                cancellationObserved =
                    true;

                break;
            }

            juce::Thread::sleep(
                1
            );
        }

        expect(
            cancellationObserved
        );

        expect(
            bestOutputRms
                < baselineRms
        );

        if (
            baselineRms > 0.0
            && bestOutputRms > 0.0
        )
        {
            const auto attenuationDb =
                20.0
                * std::log10(
                    bestOutputRms
                    / baselineRms
                );

            expect(
                attenuationDb
                    < requiredAttenuationDb
            );
        }

        processor.releaseResources();
    }

    void testRealtimeRelearnDisablesCancellation()
    {
        beginTest(
            "Processor relearn with no hum disables cancellation"
        );

        HumbugAudioProcessor processor;

        auto layout =
            processor.getBusesLayout();

        layout.getChannelSet(
            true,
            0
        ) = juce::AudioChannelSet::mono();

        layout.getChannelSet(
            false,
            0
        ) = juce::AudioChannelSet::mono();

        const auto monoLayoutSet =
            processor.setBusesLayout(
                layout
            );

        expect(
            monoLayoutSet
        );

        if (!monoLayoutSet)
            return;

        processor.setSyntheticHumEnabled(
            true
        );

        processor.prepareToPlay(
            sampleRate,
            blockSize
        );

        juce::AudioBuffer<float> buffer(
            1,
            blockSize
        );

        juce::MidiBuffer midi;

        //
        // Establish the uncancelled hum level.
        //

        constexpr int baselineBlocks =
            16;

        double baselineMeanSquare =
            0.0;

        for (
            int block = 0;
            block < baselineBlocks;
            ++block
        )
        {
            buffer.clear();

            processor.processBlock(
                buffer,
                midi
            );

            const auto blockRms =
                buffer.getRMSLevel(
                    0,
                    0,
                    buffer.getNumSamples()
                );

            baselineMeanSquare +=
                static_cast<double>(
                    blockRms
                )
                * blockRms;
        }

        const auto baselineRms =
            std::sqrt(
                baselineMeanSquare
                / static_cast<double>(
                    baselineBlocks
                )
            );

        expect(
            baselineRms > 0.0
        );

        //
        // Learn #1: hum is present.
        //

        processor.requestLearn();

        constexpr double timeoutMs =
            5000.0;

        constexpr double requiredAttenuationDb =
            -40.0;

        const auto firstLearnStartTime =
            juce::Time::getMillisecondCounterHiRes();

        bool cancellationObserved =
            false;

        while (
            (
                juce::Time::getMillisecondCounterHiRes()
                - firstLearnStartTime
            ) < timeoutMs
        )
        {
            buffer.clear();

            processor.processBlock(
                buffer,
                midi
            );

            const auto blockRms =
                static_cast<double>(
                    buffer.getRMSLevel(
                        0,
                        0,
                        buffer.getNumSamples()
                    )
                );

            if (
                blockRms <= 0.0
            )
            {
                cancellationObserved =
                    true;

                break;
            }

            const auto attenuationDb =
                20.0
                * std::log10(
                    blockRms
                    / baselineRms
                );

            if (
                attenuationDb
                    < requiredAttenuationDb
            )
            {
                cancellationObserved =
                    true;

                break;
            }

            juce::Thread::sleep(
                1
            );
        }

        expect(
            cancellationObserved
        );

        if (!cancellationObserved)
        {
            processor.releaseResources();
            return;
        }

        //
        // Remove the source hum.
        //
        // The old cancellation model should remain active until
        // another Learn operation tells the processor otherwise.
        //

        processor.setSyntheticHumEnabled(
            false
        );

        buffer.clear();

        processor.processBlock(
            buffer,
            midi
        );

        const auto staleCancellationRms =
            buffer.getRMSLevel(
                0,
                0,
                buffer.getNumSamples()
            );

        expect(
            staleCancellationRms > 0.0f
        );

        //
        // Learn #2: raw input is now silence.
        //
        // The controller must capture that raw silence before the
        // stale canceller modifies the output.
        //

        processor.requestLearn();

        const auto secondLearnStartTime =
            juce::Time::getMillisecondCounterHiRes();

        bool cancellationDisabled =
            false;

        while (
            (
                juce::Time::getMillisecondCounterHiRes()
                - secondLearnStartTime
            ) < timeoutMs
        )
        {
            buffer.clear();

            processor.processBlock(
                buffer,
                midi
            );

            bool blockIsSilent =
                true;

            for (
                int sample = 0;
                sample < buffer.getNumSamples();
                ++sample
            )
            {
                if (
                    buffer.getSample(
                        0,
                        sample
                    ) != 0.0f
                )
                {
                    blockIsSilent =
                        false;

                    break;
                }
            }

            if (blockIsSilent)
            {
                cancellationDisabled =
                    true;

                break;
            }

            juce::Thread::sleep(
                1
            );
        }

        expect(
            cancellationDisabled
        );

        //
        // Verify that the processor remains in exact pass-through
        // on the following block as well.
        //

        buffer.clear();

        processor.processBlock(
            buffer,
            midi
        );

        const auto finalMagnitude =
            buffer.getMagnitude(
                0,
                0,
                buffer.getNumSamples()
            );

        expectEquals(
            finalMagnitude,
            0.0f
        );

        processor.releaseResources();
    }

    void testProcessorAdaptiveTrackingMaintainsCancellationDuringFrequencyDrift()
    {
        beginTest(
            "Processor adaptive tracking maintains cancellation during frequency drift"
        );

        constexpr int hostBlockSize =
            512;

        constexpr int driftSamples =
            240000;

        constexpr int measurementSamples =
            12000;

        constexpr double startFrequencyHz =
            60.0;

        constexpr double endFrequencyHz =
            60.2;

        HumbugAudioProcessor processor;

        // Configure the production processor for the
        // current mono-only Learn/tracking path.
        processor.setPlayConfigDetails(
            1,
            1,
            sampleRate,
            hostBlockSize
        );

        processor.prepareToPlay(
            sampleRate,
            hostBlockSize
        );

        processor.setSyntheticHumEnabled(
            false
        );

        expectEquals(
            processor.getTotalNumInputChannels(),
            1
        );

        expect(
            processor.isLearnAvailable()
        );

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

        juce::MidiBuffer midiBuffer;

        juce::AudioBuffer<float> learnBlock(
            1,
            hostBlockSize
        );

        const auto generateStationaryBlock =
            [&]()
            {
                auto* samples =
                    learnBlock.getWritePointer(0);

                for (
                    int sample = 0;
                    sample < hostBlockSize;
                    ++sample
                )
                {
                    samples[sample] =
                        generator.processSample();
                }
            };

        // ------------------------------------------------
        // Initial manual Learn through the actual
        // PluginProcessor path.
        // ------------------------------------------------

        processor.requestLearn();

        expect(
            processor.isLearnInProgress()
        );

        constexpr double learnTimeoutMs =
            5000.0;

        const auto learnStartTime =
            juce::Time::
                getMillisecondCounterHiRes();

        while (
            processor.isLearnInProgress()
            && (
                juce::Time::
                    getMillisecondCounterHiRes()
                - learnStartTime
            ) < learnTimeoutMs
        )
        {
            generateStationaryBlock();

            processor.processBlock(
                learnBlock,
                midiBuffer
            );

            // Give the background Learn worker CPU time
            // while audio continues advancing.
            juce::Thread::sleep(
                1
            );
        }

        expect(
            !processor.isLearnInProgress()
        );

        if (
            processor.isLearnInProgress()
        )
        {
            processor.releaseResources();
            return;
        }

        // At this point the processor has consumed the
        // valid manual model and should have automatically
        // enabled adaptive tracking.

        // ------------------------------------------------
        // Drift the same continuous physical source from
        // 60.0 Hz to 60.2 Hz over five seconds.
        // ------------------------------------------------

        double lateInputEnergy =
            0.0;

        double lateOutputEnergy =
            0.0;

        int driftPosition =
            0;

        while (
            driftPosition < driftSamples
        )
        {
            const auto samplesThisBlock =
                std::min(
                    hostBlockSize,
                    driftSamples
                        - driftPosition
                );

            juce::AudioBuffer<float> block(
                1,
                samplesThisBlock
            );

            auto* samples =
                block.getWritePointer(0);

            for (
                int sample = 0;
                sample < samplesThisBlock;
                ++sample
            )
            {
                const auto absoluteDriftSample =
                    driftPosition
                    + sample;

                const auto progress =
                    static_cast<double>(
                        absoluteDriftSample
                    )
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

                // HumGenerator changes oscillator frequency
                // without resetting phase, so this remains
                // one continuous drifting source.
                generator.setFundamentalFrequency(
                    frequencyHz
                );

                const auto inputSample =
                    generator.processSample();

                samples[sample] =
                    inputSample;

                if (
                    absoluteDriftSample
                    >= driftSamples
                        - measurementSamples
                )
                {
                    lateInputEnergy +=
                        static_cast<double>(
                            inputSample
                        )
                        * inputSample;
                }
            }

            processor.processBlock(
                block,
                midiBuffer
            );

            const auto* outputSamples =
                block.getReadPointer(0);

            for (
                int sample = 0;
                sample < samplesThisBlock;
                ++sample
            )
            {
                const auto absoluteDriftSample =
                    driftPosition
                    + sample;

                if (
                    absoluteDriftSample
                    >= driftSamples
                        - measurementSamples
                )
                {
                    const auto outputSample =
                        outputSamples[sample];

                    lateOutputEnergy +=
                        static_cast<double>(
                            outputSample
                        )
                        * outputSample;
                }
            }

            driftPosition +=
                samplesThisBlock;

            // For this first processor-level regression,
            // run the simulated audio approximately in
            // realtime so the tracking worker gets a
            // realistic opportunity to complete analyses.
            //
            // 512 samples / 48 kHz ≈ 10.67 ms.
            juce::Thread::sleep(
                10
            );
        }

        processor.releaseResources();

        expect(
            lateInputEnergy > 0.0
        );

        expect(
            lateOutputEnergy >= 0.0
        );

        if (
            lateInputEnergy <= 0.0
        )
        {
            return;
        }

        const auto attenuationDb =
            10.0
            * std::log10(
                lateOutputEnergy
                / lateInputEnergy
            );

        // Broad first-run requirement:
        // the adaptive production path should at least
        // provide useful late cancellation. The equivalent
        // stale fixed-model fixture previously ended around
        // +5 dB.
        expect(
            attenuationDb < -15.0
        );
    }
};

static ProcessorIntegrationTests
    processorIntegrationTests;