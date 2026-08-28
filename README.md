# Humbug

> Adaptive mains hum removal for guitar.

Humbug is a JUCE-based audio plugin that aims to remove 50/60 Hz mains hum from guitars—especially single-coil and P-90 pickups—using sinusoidal estimation and reconstruction rather than traditional notch filters or noise gates. Adaptive tracking is planned for a later stage of development.

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
  * [x] Integrate fixed cancellation into the processor signal path
  * [x] Add processor-level integration coverage
  * [x] Add initial user-facing Learn control
* [ ] Adaptive tracking
  * [ ] Track frequency drift
  * [ ] Continuously update amplitude and phase estimates
  * [ ] Smooth parameter changes during live processing
* [ ] Stereo cancellation support
* [ ] UI refinement
* [ ] Beta testing

## Status

Realtime Learn Mode with fixed harmonic cancellation is functional for mono processing.

Humbug can capture an analysis window, detect mains hum on a background thread, estimate its harmonic structure, and activate phase-aligned cancellation without performing expensive analysis inside the realtime audio callback.

Initial DAW testing has shown strong cancellation of generated harmonic hum and meaningful reduction of real guitar hum. Because the learned cancellation model is currently fixed after activation, cancellation gradually loses effectiveness as the source frequency or harmonic characteristics drift over time.

Adaptive tracking is the next major DSP milestone.
