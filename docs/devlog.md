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