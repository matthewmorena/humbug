#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

class HumbugAudioProcessorEditor final
    : public juce::AudioProcessorEditor
{
public:
    explicit HumbugAudioProcessorEditor(
        HumbugAudioProcessor& processor
    );

    ~HumbugAudioProcessorEditor() override = default;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    HumbugAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label statusLabel;

    juce::Slider gainSlider;
    juce::Label gainLabel;

    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment
    > gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        HumbugAudioProcessorEditor
    )
};