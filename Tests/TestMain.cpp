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

namespace
{
struct CommandLineOptions
{
    bool showHelp = false;
    bool listTests = false;
    bool logPasses = false;
    bool exactMatch = false;

    juce::StringArray testFilters;
    juce::StringArray categories;
};

void printUsage(const char* executableName)
{
    const juce::String exe = executableName != nullptr
        ? juce::String::fromUTF8(executableName)
        : "Tests";

    std::cout
        << "Usage:\n"
        << "  " << exe.toStdString() << " [options] [test-filter ...]\n\n"
        << "Options:\n"
        << "  -l, --list                 List registered test suites and exit\n"
        << "  -t, --test <filter>        Run suites matching a name filter; may repeat\n"
        << "  -c, --category <name>      Restrict runs to a category; may repeat\n"
        << "  -e, --exact                Treat test filters as exact names\n"
        << "  -p, --passes               Log successful assertions too\n"
        << "  -h, --help                 Show this help\n\n"
        << "Matching:\n"
        << "  Filters are case-insensitive substrings by default.\n"
        << "  '*' and '?' may be used as wildcards. Multiple filters are ORed.\n"
        << "  Categories are exact, case-insensitive matches.\n\n"
        << "Examples:\n"
        << "  " << exe.toStdString() << "\n"
        << "  " << exe.toStdString() << " --list\n"
        << "  " << exe.toStdString() << " \"Hum Tracking Worker\"\n"
        << "  " << exe.toStdString() << " tracking\n"
        << "  " << exe.toStdString() << " \"*Integration*\"\n"
        << "  " << exe.toStdString() << " --category DSP oscillator\n";
}

bool readOptionValue(
    int& index,
    int argc,
    char* argv[],
    const juce::String& option,
    juce::String& value)
{
    if (index + 1 >= argc)
    {
        std::cerr
            << "Missing value after "
            << option.toStdString()
            << ".\n";
        return false;
    }

    value = juce::String::fromUTF8(argv[++index]);
    return true;
}

bool parseCommandLine(
    int argc,
    char* argv[],
    CommandLineOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const auto arg = juce::String::fromUTF8(argv[i]);

        if (arg == "-h" || arg == "--help")
        {
            options.showHelp = true;
        }
        else if (arg == "-l" || arg == "--list")
        {
            options.listTests = true;
        }
        else if (arg == "-p" || arg == "--passes")
        {
            options.logPasses = true;
        }
        else if (arg == "-e" || arg == "--exact")
        {
            options.exactMatch = true;
        }
        else if (arg == "-t" || arg == "--test")
        {
            juce::String value;

            if (!readOptionValue(i, argc, argv, arg, value))
                return false;

            options.testFilters.add(value);
        }
        else if (arg == "-c" || arg == "--category")
        {
            juce::String value;

            if (!readOptionValue(i, argc, argv, arg, value))
                return false;

            options.categories.add(value);
        }
        else if (arg.startsWithChar('-'))
        {
            std::cerr
                << "Unknown option: "
                << arg.toStdString()
                << "\n";
            return false;
        }
        else
        {
            // Positional arguments are test-name filters, which keeps the
            // common case short:  Tests tracking
            options.testFilters.add(arg);
        }
    }

    return true;
}

bool matchesCategory(
    const juce::UnitTest& test,
    const juce::StringArray& categories)
{
    if (categories.isEmpty())
        return true;

    for (const auto& category : categories)
    {
        if (test.getCategory().equalsIgnoreCase(category))
            return true;
    }

    return false;
}

bool matchesNameFilter(
    const juce::String& testName,
    const juce::String& filter,
    bool exactMatch)
{
    if (exactMatch)
        return testName.equalsIgnoreCase(filter);

    if (filter.containsChar('*') || filter.containsChar('?'))
        return testName.matchesWildcard(filter, true);

    return testName.containsIgnoreCase(filter);
}

bool matchesAnyNameFilter(
    const juce::UnitTest& test,
    const CommandLineOptions& options)
{
    if (options.testFilters.isEmpty())
        return true;

    for (const auto& filter : options.testFilters)
    {
        if (matchesNameFilter(test.getName(), filter, options.exactMatch))
            return true;
    }

    return false;
}

juce::Array<juce::UnitTest*> selectTests(
    const CommandLineOptions& options)
{
    juce::Array<juce::UnitTest*> selected;

    for (auto* test : juce::UnitTest::getAllTests())
    {
        if (test != nullptr
            && matchesCategory(*test, options.categories)
            && matchesAnyNameFilter(*test, options))
        {
            selected.add(test);
        }
    }

    return selected;
}

void printTestList()
{
    const auto& tests = juce::UnitTest::getAllTests();

    std::cout
        << "Registered test suites ("
        << tests.size()
        << "):\n";

    for (const auto* test : tests)
    {
        if (test == nullptr)
            continue;

        std::cout
            << "  ["
            << test->getCategory().toStdString()
            << "] "
            << test->getName().toStdString()
            << '\n';
    }
}

void printSelection(const juce::Array<juce::UnitTest*>& tests)
{
    std::cout
        << "Running "
        << tests.size()
        << " selected test suite"
        << (tests.size() == 1 ? "" : "s")
        << ":\n";

    for (const auto* test : tests)
    {
        if (test != nullptr)
        {
            std::cout
                << "  ["
                << test->getCategory().toStdString()
                << "] "
                << test->getName().toStdString()
                << '\n';
        }
    }

    std::cout << '\n';
}

int countFailures(const ConsoleUnitTestRunner& runner)
{
    int totalFailures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult(i))
            totalFailures += result->failures;
    }

    return totalFailures;
}
} // namespace

int main(int argc, char* argv[])
{
    CommandLineOptions options;

    if (!parseCommandLine(argc, argv, options))
    {
        std::cerr << '\n';
        printUsage(argc > 0 ? argv[0] : nullptr);
        return 2;
    }

    if (options.showHelp)
    {
        printUsage(argc > 0 ? argv[0] : nullptr);
        return 0;
    }

    if (options.listTests)
    {
        printTestList();
        return 0;
    }

    auto selectedTests = selectTests(options);

    if (selectedTests.isEmpty())
    {
        std::cerr << "No registered test suites matched the selection.\n\n";
        printTestList();
        return 2;
    }

    ConsoleUnitTestRunner runner;

    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(options.logPasses);

    const bool hasSelection =
        !options.testFilters.isEmpty()
        || !options.categories.isEmpty();

    if (hasSelection)
        printSelection(selectedTests);

    // Using runTests() here, rather than runAllTests(), lets the command line
    // select any subset of the UnitTest objects registered by the test files.
    runner.runTests(selectedTests);

    const int totalFailures = countFailures(runner);

    std::cout
        << "\nTotal failures: "
        << totalFailures
        << '\n';

    return totalFailures == 0 ? 0 : 1;
}
