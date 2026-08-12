#include <JuceHeader.h>

int main()
{
    juce::UnitTestRunner runner;

    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(true);

    runner.runAllTests();

    int totalFailures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult(i))
            totalFailures += result->failures;
    }

    return totalFailures == 0 ? 0 : 1;
}