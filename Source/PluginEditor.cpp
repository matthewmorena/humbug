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
        "Audio pass-through",
        juce::dontSendNotification
    );

    statusLabel.setJustificationType(
        juce::Justification::centred
    );

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(statusLabel);

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
        bounds.removeFromTop(70)
    );

    statusLabel.setBounds(
        bounds.removeFromTop(40)
    );
}