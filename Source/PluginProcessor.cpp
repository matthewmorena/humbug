#include "PluginProcessor.h"
#include "PluginEditor.h"

HumbugAudioProcessor::HumbugAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput(
                  "Input",
                  juce::AudioChannelSet::stereo(),
                  true)

              .withOutput(
                  "Output",
                  juce::AudioChannelSet::stereo(),
                  true)),
      parameterState(
          *this,
          nullptr,
          "Parameters",
          createParameterLayout())
{
    gainParameter = parameterState.getRawParameterValue(
        ParameterIDs::gain);
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
}

void HumbugAudioProcessor::releaseResources()
{
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
    gainProcessor.setGainDecibels(
        gainParameter->load());
    auto audioBlock =
        juce::dsp::AudioBlock<float>(buffer);
    auto context =
        juce::dsp::ProcessContextReplacing<float>(
            audioBlock);
    gainProcessor.process(context);
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