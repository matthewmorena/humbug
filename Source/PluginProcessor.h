#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "DSP/HumGenerator.h"

namespace ParameterIDs
{

inline constexpr auto gain = "gain";

}

class HumbugAudioProcessor final : public juce::AudioProcessor
{

public:

HumbugAudioProcessor();

~HumbugAudioProcessor() override = default;

void prepareToPlay(

double sampleRate,

int samplesPerBlock

) override;

void releaseResources() override;

bool isBusesLayoutSupported(

const BusesLayout& layouts

) const override;

void processBlock(

juce::AudioBuffer<float>& buffer,

juce::MidiBuffer& midiMessages

) override;

juce::AudioProcessorEditor* createEditor() override;

bool hasEditor() const override;

const juce::String getName() const override;

bool acceptsMidi() const override;

bool producesMidi() const override;

bool isMidiEffect() const override;

double getTailLengthSeconds() const override;

int getNumPrograms() override;

int getCurrentProgram() override;

void setCurrentProgram(int index) override;

const juce::String getProgramName(int index) override;

void changeProgramName(

int index,

const juce::String& newName

) override;

void getStateInformation(

juce::MemoryBlock& destinationData

) override;

void setStateInformation(

const void* data,

int sizeInBytes

) override;

void setSyntheticHumEnabled(bool shouldBeEnabled) noexcept;

juce::AudioProcessorValueTreeState&
getParameterState() noexcept;

const juce::AudioProcessorValueTreeState&
getParameterState() const noexcept;

private:

static juce::AudioProcessorValueTreeState::ParameterLayout
createParameterLayout();

juce::AudioProcessorValueTreeState parameterState;

std::atomic<float>* gainParameter = nullptr;

HumGenerator humGenerator;
juce::dsp::Gain<float> gainProcessor;

bool syntheticHumEnabled = false;

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(

HumbugAudioProcessor

)

};