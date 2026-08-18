#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>

class LearnBuffer
{
public:
    static constexpr double analysisDurationSeconds =
        0.25;

    void prepare(
        double sampleRate,
        int numChannels
    )
    {
        jassert(sampleRate > 0.0);
        jassert(numChannels > 0);

        const auto targetSamples =
            static_cast<int>(
                std::round(
                    sampleRate
                    * analysisDurationSeconds
                )
            );

        buffer.setSize(
            numChannels,
            targetSamples
        );

        buffer.clear();

        reset();
    }

    void reset() noexcept
    {
        writePosition = 0;
        collecting = false;
        ready = false;
    }

    void start() noexcept
    {
        writePosition = 0;
        ready = false;

        collecting =
            buffer.getNumChannels() > 0
            && buffer.getNumSamples() > 0;
    }

    void push(
        const juce::AudioBuffer<float>& input
    ) noexcept
    {
        if (!collecting)
            return;

        const auto remainingSamples =
            buffer.getNumSamples()
            - writePosition;

        const auto samplesToCopy =
            std::min(
                remainingSamples,
                input.getNumSamples()
            );

        if (samplesToCopy <= 0)
            return;

        const auto channelsToCopy =
            std::min(
                buffer.getNumChannels(),
                input.getNumChannels()
            );

        for (
            int channel = 0;
            channel < channelsToCopy;
            ++channel
        )
        {
            buffer.copyFrom(
                channel,
                writePosition,
                input,
                channel,
                0,
                samplesToCopy
            );
        }

        // Defensive fallback if the input unexpectedly
        // contains fewer channels than were prepared.
        for (
            int channel = channelsToCopy;
            channel < buffer.getNumChannels();
            ++channel
        )
        {
            buffer.clear(
                channel,
                writePosition,
                samplesToCopy
            );
        }

        writePosition +=
            samplesToCopy;

        if (
            writePosition
            >= buffer.getNumSamples()
        )
        {
            writePosition =
                buffer.getNumSamples();

            collecting = false;
            ready = true;
        }
    }

    bool isCollecting() const noexcept
    {
        return collecting;
    }

    bool isReady() const noexcept
    {
        return ready;
    }

    const juce::AudioBuffer<float>&
    getBuffer() const noexcept
    {
        return buffer;
    }

private:
    juce::AudioBuffer<float> buffer;

    int writePosition = 0;

    bool collecting = false;
    bool ready = false;
};