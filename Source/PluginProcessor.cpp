#include "PluginProcessor.h"
#include "PluginEditor.h"

HumbugAudioProcessor::HumbugAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput(
                  "Input",
                  juce::AudioChannelSet::stereo(),
                  true
              )
              .withOutput(
                  "Output",
                  juce::AudioChannelSet::stereo(),
                  true
              )
      ),
      parameterState(
          *this,
          nullptr,
          "Parameters",
          createParameterLayout()
      ),
    humAnalysisWorker(
        learnModeController,
        learnedModelMailbox
    ),
    humTrackingWorker(
        humTrackingController,
        trackingModelMailbox
    )
{
    gainParameter = parameterState.getRawParameterValue(
        ParameterIDs::gain
    );

    jassert(gainParameter != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout
HumbugAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto gainAttributes =
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withCategory(
                juce::AudioProcessorParameter::outputGain);

    layout.add(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{
                ParameterIDs::gain,
                1},
            "Gain",
            juce::NormalisableRange<float>{
                -24.0f,
                12.0f,
                0.1f},
            0.0f,
            gainAttributes));

    return layout;
}

void HumbugAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock
)
{
    humAnalysisWorker.stop();
    humTrackingWorker.stop();

    learnedModelMailbox.reset();
    trackingModelMailbox.reset();

    learnModeController.prepare(
        sampleRate,
        getTotalNumInputChannels()
    );

    humTrackingController.prepare(
        sampleRate,
        getTotalNumInputChannels()
    );

    adaptiveHumCanceller.prepare(
        sampleRate
    );

    humAnalysisWorker.prepare(
        sampleRate,
        0
    );

    humTrackingWorker.prepare(
        sampleRate,
        0
    );

    learnRequested.store(
        false,
        std::memory_order_release
    );

    adaptiveTrackingWanted =
        false;

    minimumTrackingAnalysisStartSample =
        0;

    const auto analysisWorkerStarted =
        humAnalysisWorker.start();

    jassert(
        analysisWorkerStarted
    );

    const auto trackingWorkerStarted =
        humTrackingWorker.start();

    jassert(
        trackingWorkerStarted
    );

    const juce::dsp::ProcessSpec processSpec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(
            getTotalNumOutputChannels()
        )
    };

    gainProcessor.setGainDecibels(
        gainParameter->load()
    );

    gainProcessor.prepare(processSpec);
    gainProcessor.setRampDurationSeconds(0.02);

    /* Preparing the hum generator */
    humGenerator.prepare(sampleRate);

    humGenerator.setFundamentalFrequency(60.0);

    humGenerator.clearHarmonics();

    humGenerator.setHarmonicAmplitude(1, 0.05f);
    humGenerator.setHarmonicPhase(1, 0.13);

    humGenerator.setHarmonicAmplitude(2, 0.02f);
    humGenerator.setHarmonicPhase(2, 0.37);

    humGenerator.setHarmonicAmplitude(3, 0.01f);
    humGenerator.setHarmonicPhase(3, 0.71);

    humGenerator.reset();
}

void HumbugAudioProcessor::setSyntheticHumEnabled(
    bool shouldBeEnabled
) noexcept
{
    syntheticHumEnabled = shouldBeEnabled;
}

void HumbugAudioProcessor::releaseResources()
{
    humAnalysisWorker.stop();
    humTrackingWorker.stop();

    learnRequested.store(
        false,
        std::memory_order_release
    );

    adaptiveTrackingWanted =
        false;

    adaptiveHumCanceller.reset();
}

bool HumbugAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const
{
    const auto inputLayout =
        layouts.getMainInputChannelSet();

    const auto outputLayout =
        layouts.getMainOutputChannelSet();

    const bool isSupportedChannelCount =
        outputLayout == juce::AudioChannelSet::mono() || outputLayout == juce::AudioChannelSet::stereo();

    return isSupportedChannelCount && inputLayout == outputLayout;
}

void HumbugAudioProcessor::processBlock(
    juce::AudioBuffer<float> &buffer,
    juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const auto totalInputChannels =
        getTotalNumInputChannels();

    const auto totalOutputChannels =
        getTotalNumOutputChannels();

    for (
        auto channel = totalInputChannels;
        channel < totalOutputChannels;
        ++channel)
    {
        buffer.clear(
            channel,
            0,
            buffer.getNumSamples());
    }

    if (
        learnModeController.isModelReady()
    )
    {
        PendingLearnResult result;

        if (
            learnedModelMailbox.tryConsume(
                result
            )
        )
        {
            const auto activationSample =
                learnModeController
                    .getCurrentSamplePosition();

            jassert(
                activationSample
                    >= result.analysisStartSample
            );

            const auto sampleOffset =
                activationSample
                - result.analysisStartSample;

            // Manual Learn is authoritative:
            // replace any existing adaptive model.
            adaptiveHumCanceller.activateModel(
                result.model,
                static_cast<std::size_t>(
                    sampleOffset
                )
            );

            // Any automatic tracking window that began
            // before this manual model became authoritative
            // must not later replace it.
            minimumTrackingAnalysisStartSample =
                activationSample;

            if (
                result.model.valid
                && result.model.humDetected
            )
            {
                const auto referenceUpdated =
                    humTrackingController
                        .setReferenceFrequency(
                            result.model.frequencyHz
                        );

                jassert(
                    referenceUpdated
                );

                adaptiveTrackingWanted =
                    true;

                if (
                    humTrackingController.isInactive()
                )
                {
                    const auto trackingStarted =
                        humTrackingController
                            .startTracking();

                    jassert(
                        trackingStarted
                    );
                }
            }
            else
            {
                // Manual no-hum Learn keeps its existing
                // semantics: disable cancellation and
                // stop automatic tracking.
                adaptiveTrackingWanted =
                    false;

                humTrackingController
                    .requestStopTracking();
            }

            learnModeController
                .finishModelHandoff();
        }
        else
        {
            jassertfalse;
        }
    }

    if (
        humTrackingController.isModelReady()
    )
    {
        PendingLearnResult result;

        if (
            trackingModelMailbox.tryConsume(
                result
            )
        )
        {
            const auto activationSample =
                humTrackingController
                    .getCurrentSamplePosition();

            jassert(
                activationSample
                    >= result.analysisStartSample
            );

            const auto resultIsCurrent =
                result.analysisStartSample
                >= minimumTrackingAnalysisStartSample;

            if (
                adaptiveTrackingWanted
                && resultIsCurrent
                && result.model.valid
                && result.model.humDetected
            )
            {
                const auto sampleOffset =
                    activationSample
                    - result.analysisStartSample;

                const auto updateApplied =
                    adaptiveHumCanceller.transitionToModel(
                        result.model,
                        static_cast<std::size_t>(
                            sampleOffset
                        )
                    );

                if (updateApplied)
                {
                    const auto referenceUpdated =
                        humTrackingController
                            .setReferenceFrequency(
                                result.model.frequencyHz
                            );

                    jassert(
                        referenceUpdated
                    );
                }
            }

            // Always complete the handoff, even when the
            // model was rejected or stale.
            humTrackingController
                .finishModelHandoff();

            // Important edge case:
            //
            // A previous no-hum Learn may have requested
            // tracking stop while an operation was still
            // outstanding. If a newer valid Learn has since
            // made tracking desirable again, draining that
            // old operation can leave the controller
            // Inactive. Restart it here.
            if (
                adaptiveTrackingWanted
                && humTrackingController.isInactive()
            )
            {
                const auto trackingStarted =
                    humTrackingController
                        .startTracking();

                jassert(
                    trackingStarted
                );
            }
        }
        else
        {
            jassertfalse;
        }
    }

    // Begin requested Learn operations only on the audio thread.
    if (
        totalInputChannels == 1
        && learnRequested.load(
            std::memory_order_acquire
        )
    )
    {
        if (
            learnModeController.startLearn()
        )
        {
            learnRequested.store(
                false,
                std::memory_order_release
            );
        }
    }

    // Development-only hum injection.
    if (syntheticHumEnabled)
    {
        humGenerator.addToBuffer(
            buffer
        );
    }

    // Capture/advance analysis timelines from RAW input
    // before realtime cancellation.
    learnModeController.processBlock(
        buffer
    );

    humTrackingController.processBlock(
        buffer
    );

    // Initial integration is mono-only.
    if (
        totalInputChannels == 1
    )
    {
        auto* channelData =
            buffer.getWritePointer(0);

        for (
            int sample = 0;
            sample < buffer.getNumSamples();
            ++sample
        )
        {
            channelData[sample] =
                adaptiveHumCanceller.processSample(
                    channelData[sample]
                );
        }
    }

    gainProcessor.setGainDecibels(
        gainParameter->load());

    auto audioBlock =
        juce::dsp::AudioBlock<float>(buffer);

    auto context =
        juce::dsp::ProcessContextReplacing<float>(
            audioBlock);

    gainProcessor.process(context);
}

void HumbugAudioProcessor::requestLearn() noexcept
{
    learnRequested.store(
        true,
        std::memory_order_release
    );
}

bool HumbugAudioProcessor::isLearnAvailable() const noexcept
{
    return getTotalNumInputChannels() == 1;
}

bool HumbugAudioProcessor::isLearnInProgress() const noexcept
{
    return
        learnRequested.load(
            std::memory_order_acquire
        )
        || learnModeController.isCollecting()
        || learnModeController.isReadyForAnalysis()
        || learnModeController.isAnalyzing()
        || learnModeController.isModelReady();
}

juce::AudioProcessorValueTreeState &
HumbugAudioProcessor::getParameterState() noexcept
{
    return parameterState;
}

const juce::AudioProcessorValueTreeState &
HumbugAudioProcessor::getParameterState() const noexcept
{
    return parameterState;
}

juce::AudioProcessorEditor *
HumbugAudioProcessor::createEditor()
{
    return new HumbugAudioProcessorEditor(*this);
}

bool HumbugAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String HumbugAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HumbugAudioProcessor::acceptsMidi() const
{
    return false;
}

bool HumbugAudioProcessor::producesMidi() const
{
    return false;
}

bool HumbugAudioProcessor::isMidiEffect() const
{
    return false;
}

double HumbugAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HumbugAudioProcessor::getNumPrograms()
{
    return 1;
}

int HumbugAudioProcessor::getCurrentProgram()
{
    return 0;
}

void HumbugAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String
HumbugAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void HumbugAudioProcessor::changeProgramName(
    int index,
    const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

void HumbugAudioProcessor::getStateInformation(
    juce::MemoryBlock& destinationData
)
{
    const auto state = parameterState.copyState();
    const auto xml = state.createXml();
    if (xml != nullptr)
    {
        copyXmlToBinary(
            *xml,
            destinationData
        );
    }
}

void HumbugAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes
)
{
    const auto xml = getXmlFromBinary(
        data,
        sizeInBytes
    );

    if (xml == nullptr)
        return;

    if (!xml->hasTagName(parameterState.state.getType()))
        return;

    const auto state =
        juce::ValueTree::fromXml(*xml);

    if (!state.isValid())
        return;

    parameterState.replaceState(state);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new HumbugAudioProcessor();
}