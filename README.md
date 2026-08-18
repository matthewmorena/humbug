# Humbug

> Adaptive mains hum removal for guitar.

Humbug is a JUCE-based audio plugin that aims to remove 50/60 Hz mains hum from guitars—especially single-coil and P-90 pickups—using sinusoidal estimation and reconstruction rather than traditional notch filters or noise gates. Adaptive tracking is planned for a later stage of development.

## Vision

Build a transparent hum removal plugin suitable for both live monitoring and studio recording.

## Roadmap

- [x] Project setup
- [x] Audio pass-through
- [x] Gain processor
- [x] Hum generator
  - [x] Sine oscillator
  - [x] Harmonic synthesis
  - [x] Configurable harmonic amplitude and phase
  - [x] Buffer-level generation
  - [x] Processor integration
- [x] Learn mode
  - [x] Fixed-frequency harmonic amplitude/phase estimation
  - [x] Arbitrary-window least-squares estimation
  - [x] Basic interference robustness testing
  - [x] Fundamental-frequency detection
- [x] Fixed harmonic subtraction
  - [x] Reconstruct estimated hum
  - [x] Subtract reconstructed hum from input
  - [x] Measure cancellation effectiveness
- [ ] Realtime Learn Mode integration
  - [x] Preallocated analysis-window buffering
  - [ ] Trigger and manage Learn Mode capture
  - [ ] Run frequency detection and harmonic estimation off the audio thread
  - [ ] Safely publish the learned cancellation model to the audio thread
  - [ ] Preserve phase alignment while analysis is in progress
  - [ ] Integrate fixed cancellation into the processor signal path
- [ ] Adaptive tracking
  - [ ] Track frequency drift
  - [ ] Continuously update amplitude and phase estimates
  - [ ] Smooth parameter changes during live processing
- [ ] UI refinement
- [ ] Beta testing

## Status

Early development. Core hum detection, harmonic estimation, fixed reconstruction/subtraction, and synthetic cancellation testing are implemented. Realtime Learn Mode integration, adaptive tracking, UI refinement, and testing with recorded audio remain in progress.
