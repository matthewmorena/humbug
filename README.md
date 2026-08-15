# Humbug

> Adaptive mains hum removal for guitar.

Humbug is a JUCE-based audio plugin that aims to remove 50/60 Hz mains hum from guitars—especially single-coil and P-90 pickups—using adaptive sinusoidal estimation rather than traditional notch filters or noise gates.

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
- [ ] Fixed harmonic subtraction
  - [ ] Reconstruct estimated hum
  - [ ] Subtract reconstructed hum from input
  - [ ] Measure cancellation effectiveness
- [ ] Adaptive tracking
  - [ ] Track frequency drift
  - [ ] Continuously update amplitude and phase estimates
  - [ ] Smooth parameter changes during live processing
- [ ] UI refinement
- [ ] Beta testing

## Status

Early development.
