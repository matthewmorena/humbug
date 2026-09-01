# Humbug

> Adaptive mains hum removal for guitar.

Humbug is a JUCE-based audio plugin that aims to remove 50/60 Hz mains hum from guitars—especially single-coil and P-90 pickups—using sinusoidal estimation, reconstruction, and adaptive tracking rather than traditional notch filters or noise gates.

## Vision

Build a transparent hum removal plugin suitable for both live monitoring and studio recording.

## Roadmap

* [x] Project setup
* [x] Audio pass-through
* [x] Gain processor
* [x] Hum generator
  * [x] Sine oscillator
  * [x] Harmonic synthesis
  * [x] Configurable harmonic amplitude and phase
  * [x] Buffer-level generation
  * [x] Processor integration
* [x] Learn mode
  * [x] Fixed-frequency harmonic amplitude/phase estimation
  * [x] Arbitrary-window least-squares estimation
  * [x] Basic interference robustness testing
  * [x] Fundamental-frequency detection
  * [x] Hum-presence classification
* [x] Fixed harmonic subtraction
  * [x] Reconstruct estimated hum
  * [x] Subtract reconstructed hum from input
  * [x] Measure cancellation effectiveness
* [x] Realtime Learn Mode integration
  * [x] Preallocated analysis-window buffering
  * [x] Trigger and manage Learn Mode capture
  * [x] Run frequency detection and harmonic estimation off the audio thread
  * [x] Safely publish learned cancellation models to the audio thread
  * [x] Preserve phase alignment across analysis latency
  * [x] Integrate learned cancellation into the processor signal path
  * [x] Add processor-level integration coverage
  * [x] Add initial user-facing Learn control
* [x] Adaptive tracking
  * [x] Track gradual fundamental-frequency drift
  * [x] Continuously update harmonic amplitude and phase estimates
  * [x] Run periodic tracking analysis off the audio thread
  * [x] Use narrow frequency searches around the most recently accepted model
  * [x] Smoothly transition between tracked cancellation models
  * [x] Integrate automatic tracking with manual Learn/relearn behavior
  * [x] Add end-to-end and processor-level adaptive regression coverage
* [ ] Adaptive tracking refinement
  * [ ] Add confidence or consistency gating for contaminated tracking windows
  * [ ] Improve perceptual smoothness of automatic model transitions
  * [ ] Explore cancellation beyond the current eight-harmonic model
* [ ] Stereo cancellation support
* [ ] UI refinement
* [ ] Beta testing

## Status

Realtime Learn Mode with adaptive harmonic cancellation is functional for mono processing.

Humbug can capture an initial analysis window, detect mains hum on a background thread, estimate its harmonic structure, and activate phase-aligned cancellation without performing expensive analysis inside the realtime audio callback.

After a valid manual Learn, automatic tracking periodically captures new raw-input windows and updates the cancellation model as the hum changes. Tracking uses a narrow frequency search around the most recently accepted fundamental and crossfades between reconstructed models to avoid abrupt realtime model replacement.

Automated testing shows that adaptive tracking can maintain useful cancellation during gradual changes in fundamental frequency, harmonic amplitude, and harmonic phase, including in the presence of unrelated tonal content.

Initial DAW testing with electric guitar has also shown effective adaptive cancellation across the currently modeled first eight harmonics while the instrument is idle. Current areas for further refinement include rejecting tracking updates contaminated by strong guitar playing, improving the perceptual smoothness of model transitions, and investigating higher-order harmonic cancellation.

Stereo Learn and cancellation are not yet supported.
