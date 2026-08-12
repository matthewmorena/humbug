# Development Log

## 2026-07-22 — JUCE Plugin Skeleton

### Goal

Create a minimal JUCE plugin that builds as both a VST3 and a
standalone application and passes audio through unchanged.

### Decisions

- Use JUCE 8.0.15 as a Git submodule.
- Use CMake rather than Projucer.
- Use C++20.
- Build VST3 and Standalone formats initially.
- Support matching mono and stereo input/output layouts.
- Keep DSP classes out of the audio path until pass-through is verified.

### What I Learned

- JUCE receives audio in blocks through `processBlock`.
- The input and output use the same `AudioBuffer`.
- Leaving the samples unchanged creates pass-through.
- `prepareToPlay` is called before processing and provides the sample
  rate and expected block size.
- Plugin code must support the channel layouts requested by the host.

### Next Step

Add automated tests and a controllable gain stage before implementing
the first oscillator.

## 2026-08-11 — Gain Processor and Test Infrastructure

### Goal

Add a simple controllable gain stage to validate Humbug's parameter, DSP, UI, state-management, and testing architecture before implementing hum-specific processing.

### Decisions

* Use `AudioProcessorValueTreeState` to manage plugin parameters and state.
* Add a Gain parameter ranging from `-24 dB` to `+12 dB`, with `0 dB` as the default.
* Use `juce::dsp::Gain<float>` with a 20 ms smoothing ramp.
* Connect the editor knob to the Gain parameter using `SliderAttachment`.
* Serialize the APVTS state through `getStateInformation` and `setStateInformation`.
* Use JUCE's built-in unit testing framework with a separate CMake/CTest test executable.
* Keep the gain implementation inside the main processor for now rather than introducing an additional DSP abstraction.

### What I Learned

* APVTS provides a useful central source of truth for DSP parameters, host automation, UI controls, and saved plugin state.
* Host parameter values are normalized from `0.0` to `1.0`, even when the parameter itself represents values such as decibels.
* `SliderAttachment` keeps parameter changes synchronized in both directions between the plugin UI and the host.
* DSP smoothing needs to be considered when writing sample-level tests.
* The first gain tests exposed an unintended startup fade: the gain processor initially ramped from silence to `0 dB` over 20 ms.
* Initializing the gain value before configuring the smoothing ramp prevents that startup fade while preserving smoothing for later parameter changes.
* Automated DSP tests can catch subtle behavior that is easy to miss during manual listening tests.
* CTest can run the JUCE test executable, while JUCE's `UnitTestRunner` handles the individual test cases inside it.

### Tests Added

The initial automated suite verifies that:

* `0 dB` leaves samples unchanged from the first processed sample.
* `-6 dB` produces the expected linear amplitude.
* Gain processing affects both stereo channels correctly.
* Serialized Gain state can be restored into a new processor instance.

The suite passes in both Debug and Release builds.

### Next Step

Implement a deterministic hum generator that can produce a known mains-frequency fundamental and harmonics. This will provide a controlled signal for developing and testing the later hum detection and cancellation algorithms.