cmake_minimum_required(VERSION 3.22)

project(
    Humbug
    VERSION 0.1.0
    DESCRIPTION "Adaptive mains hum removal for guitar"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(JUCE)

juce_add_plugin(Humbug
    COMPANY_NAME "Humbug Audio"
    BUNDLE_ID "com.humbugaudio.humbug"

    PLUGIN_NAME "Humbug"
    PLUGIN_MANUFACTURER_CODE Hmbg
    PLUGIN_CODE Hmb1

    FORMATS
        Standalone
        VST3

    PRODUCT_NAME "Humbug"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Humbug)

target_sources(Humbug
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(Humbug
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Humbug
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

enable_testing()

juce_add_console_app(HumbugTests
    PRODUCT_NAME "Humbug Tests"
)

juce_generate_juce_header(HumbugTests)

target_sources(HumbugTests
    PRIVATE
        Tests/TestMain.cpp
        Tests/GainProcessorTests.cpp
        Tests/OscillatorTests.cpp
        Tests/HumGeneratorTests.cpp
        Tests/HumEstimatorTests.cpp
        Tests/FundamentalFrequencyDetectorTests.cpp
        Tests/ProcessorIntegrationTests.cpp
        Tests/LinearSystemSolverTests.cpp
        Tests/HumReconstructorTests.cpp

        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(HumbugTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_Name="Humbug"
)

target_link_libraries(HumbugTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_test(
    NAME HumbugTests
    COMMAND HumbugTests
)