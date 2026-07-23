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
      )
{
}

void HumbugAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock
)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void HumbugAudioProcessor::releaseResources()
{
}

bool HumbugAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts
) const
{
    const auto inputLayout =
        layouts.getMainInputChannelSet();

    const auto outputLayout =
        layouts.getMainOutputChannelSet();

    const bool isSupportedChannelCount =
        outputLayout == juce::AudioChannelSet::mono()
        || outputLayout == juce::AudioChannelSet::stereo();

    return isSupportedChannelCount
        && inputLayout == outputLayout;
}

void HumbugAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages
)
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
        ++channel
    )
    {
        buffer.clear(
            channel,
            0,
            buffer.getNumSamples()
        );
    }

    // Intentionally perform no processing.
    //
    // JUCE has already placed the incoming samples in `buffer`.
    // Leaving them unchanged creates audio pass-through.
}

juce::AudioProcessorEditor*
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
    const juce::String& newName
)
{
    juce::ignoreUnused(index, newName);
}

void HumbugAudioProcessor::getStateInformation(
    juce::MemoryBlock& destinationData
)
{
    juce::ignoreUnused(destinationData);
}

void HumbugAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes
)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HumbugAudioProcessor();
}