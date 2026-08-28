#include <JuceHeader.h>

#include "../Source/DSP/LearnedHumModelMailbox.h"

#include <cstdint>

class LearnedHumModelMailboxTests final
    : public juce::UnitTest
{
public:
    LearnedHumModelMailboxTests()
        : juce::UnitTest(
            "Learned Hum Model Mailbox",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testEmptyMailboxCannotBeConsumed();
        testPublishedResultCanBeConsumed();
        testPendingResultCannotBeOverwritten();
        testMailboxCanBeReusedAfterConsumption();
    }

private:
    static constexpr double sampleRate =
        48000.0;

    void testEmptyMailboxCannotBeConsumed()
    {
        beginTest(
            "Empty mailbox cannot be consumed"
        );

        LearnedHumModelMailbox mailbox;

        PendingLearnResult result;

        expect(
            !mailbox.hasPendingResult()
        );

        expect(
            !mailbox.tryConsume(
                result
            )
        );
    }

    void testPublishedResultCanBeConsumed()
    {
        beginTest(
            "Published Learn result can be consumed"
        );

        LearnedHumModelMailbox mailbox;

        PendingLearnResult input;

        input.analysisStartSample =
            12345;

        input.model.frequencyHz =
            60.01;

        input.model.valid =
            true;

        input.model.humDetected =
            true;

        input.model.harmonics[0].frequencyHz =
            60.01;

        input.model.harmonics[0].amplitude =
            0.25f;

        input.model.harmonics[0].phase =
            0.37;

        expect(
            mailbox.publish(
                input
            )
        );

        expect(
            mailbox.hasPendingResult()
        );

        PendingLearnResult output;

        expect(
            mailbox.tryConsume(
                output
            )
        );

        expect(
            !mailbox.hasPendingResult()
        );

        expect(
            output.analysisStartSample
                == static_cast<std::uint64_t>(
                    12345
                )
        );

        expectWithinAbsoluteError(
            output.model.frequencyHz,
            60.01,
            0.000001
        );

        expect(
            output.model.valid
        );

        expect(
            output.model.humDetected
        );

        expectWithinAbsoluteError(
            output.model.harmonics[0].frequencyHz,
            60.01,
            0.000001
        );

        expectWithinAbsoluteError(
            output.model.harmonics[0].amplitude,
            0.25f,
            0.000001f
        );

        expectWithinAbsoluteError(
            output.model.harmonics[0].phase,
            0.37,
            0.000001
        );
    }

    void testPendingResultCannotBeOverwritten()
    {
        beginTest(
            "Pending result cannot be overwritten"
        );

        LearnedHumModelMailbox mailbox;

        PendingLearnResult first;

        first.analysisStartSample =
            1000;

        first.model.frequencyHz =
            60.0;

        PendingLearnResult second;

        second.analysisStartSample =
            2000;

        second.model.frequencyHz =
            50.0;

        expect(
            mailbox.publish(
                first
            )
        );

        expect(
            !mailbox.publish(
                second
            )
        );

        PendingLearnResult output;

        expect(
            mailbox.tryConsume(
                output
            )
        );

        expect(
            output.analysisStartSample
                == static_cast<std::uint64_t>(
                    1000
                )
        );

        expectWithinAbsoluteError(
            output.model.frequencyHz,
            60.0,
            0.000001
        );
    }

    void testMailboxCanBeReusedAfterConsumption()
    {
        beginTest(
            "Mailbox can be reused after consumption"
        );

        LearnedHumModelMailbox mailbox;

        PendingLearnResult first;

        first.model.frequencyHz =
            60.0;

        expect(
            mailbox.publish(
                first
            )
        );

        PendingLearnResult output;

        expect(
            mailbox.tryConsume(
                output
            )
        );

        PendingLearnResult second;

        second.model.frequencyHz =
            50.0;

        expect(
            mailbox.publish(
                second
            )
        );

        expect(
            mailbox.tryConsume(
                output
            )
        );

        expectWithinAbsoluteError(
            output.model.frequencyHz,
            50.0,
            0.000001
        );
    }
};

static LearnedHumModelMailboxTests
    learnedHumModelMailboxTests;