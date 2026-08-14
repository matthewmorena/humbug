#include <JuceHeader.h>

#include <iostream>

class ConsoleUnitTestRunner final : public juce::UnitTestRunner
{
protected:
    void logMessage(const juce::String& message) override
    {
        std::cout << message.toStdString() << '\n';
    }
};

int main()
{
    ConsoleUnitTestRunner runner;

    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(false);

    runner.runAllTests();

    int totalFailures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult(i))
            totalFailures += result->failures;
    }

    std::cout
        << "\nTotal failures: "
        << totalFailures
        << '\n';

    return totalFailures == 0 ? 0 : 1;
}