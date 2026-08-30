#pragma once

#include <JuceHeader.h>

#include "HumAnalyzer.h"
#include "HumTrackingController.h"
#include "LearnedHumModelMailbox.h"
#include "PendingLearnResult.h"

class HumTrackingWorker final
    : private juce::Thread
{
public:
    HumTrackingWorker(
        HumTrackingController& controllerToUse,
        LearnedHumModelMailbox& mailboxToUse
    )
        : juce::Thread(
            "Humbug Hum Tracking"
        ),
          controller(
              controllerToUse
          ),
          mailbox(
              mailboxToUse
          )
    {
    }

    ~HumTrackingWorker() override
    {
        stop();
    }

    void prepare(
        double newSampleRate,
        int newAnalysisChannel = 0
    ) noexcept
    {
        jassert(
            !isThreadRunning()
        );

        jassert(
            newSampleRate > 0.0
        );

        jassert(
            newAnalysisChannel >= 0
        );

        sampleRate =
            newSampleRate;

        analysisChannel =
            newAnalysisChannel;
    }

    bool start()
    {
        jassert(
            sampleRate > 0.0
        );

        if (isThreadRunning())
        {
            return true;
        }

        return startThread();
    }

    void stop()
    {
        if (isThreadRunning())
        {
            stopThread(
                2000
            );
        }
    }

private:
    void run() override
    {
        while (!threadShouldExit())
        {
            if (
                !controller.tryBeginAnalysis()
            )
            {
                wait(
                    5.0
                );

                continue;
            }

            const auto analysisStartSample =
                controller.getAnalysisStartSample();

            const auto model =
                analyzer.analyze(
                    controller.getAnalysisBuffer(),
                    analysisChannel,
                    sampleRate
                );

            if (threadShouldExit())
            {
                controller.abortAnalysis();
                break;
            }

            PendingLearnResult result;

            result.model =
                model;

            result.analysisStartSample =
                analysisStartSample;

            if (
                mailbox.publish(
                    result
                )
            )
            {
                controller.finishAnalysis();
            }
            else
            {
                // With one outstanding tracking
                // operation and a dedicated mailbox,
                // the mailbox should be empty here.
                jassertfalse;

                controller.abortAnalysis();
            }
        }
    }

    HumTrackingController& controller;

    LearnedHumModelMailbox& mailbox;

    HumAnalyzer analyzer;

    double sampleRate = 0.0;

    int analysisChannel = 0;
};