#include "PluginEditor.h"

HumbugAudioProcessorEditor::HumbugAudioProcessorEditor(
    HumbugAudioProcessor& processor
)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor)
{
    titleLabel.setText(
        "Humbug",
        juce::dontSendNotification
    );

    titleLabel.setJustificationType(
        juce::Justification::centred
    );

    titleLabel.setFont(
        juce::FontOptions(32.0f)
    );

    statusLabel.setText(
        "Gain processor",
        juce::dontSendNotification
    );

    statusLabel.setJustificationType(
        juce::Justification::centred
    );

    gainSlider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag
    );

    gainSlider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        80,
        24
    );

    gainSlider.setTextValueSuffix(" dB");

    gainSlider.setDoubleClickReturnValue(
        true,
        0.0
    );

    gainLabel.setText(
        "Gain",
        juce::dontSendNotification
    );

    gainLabel.setJustificationType(
        juce::Justification::centred
    );

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(gainSlider);
    addAndMakeVisible(gainLabel);

    gainAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment
    >(
        audioProcessor.getParameterState(),
        ParameterIDs::gain,
        gainSlider
    );

    setSize(500, 300);
}

void HumbugAudioProcessorEditor::paint(
    juce::Graphics& graphics
)
{
    graphics.fillAll(
        getLookAndFeel()
            .findColour(
                juce::ResizableWindow::backgroundColourId
            )
    );

    graphics.setColour(
        juce::Colours::white.withAlpha(0.12f)
    );

    graphics.drawRoundedRectangle(
        getLocalBounds()
            .toFloat()
            .reduced(16.0f),
        8.0f,
        1.0f
    );
}

void HumbugAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);

    titleLabel.setBounds(
        bounds.removeFromTop(60)
    );

    statusLabel.setBounds(
        bounds.removeFromTop(30)
    );

    bounds.removeFromTop(10);

    auto gainArea = bounds.withSizeKeepingCentre(
        140,
        150
    );

    gainLabel.setBounds(
        gainArea.removeFromTop(24)
    );

    gainSlider.setBounds(gainArea);
}