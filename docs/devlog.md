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

## 2026-08-13 — Hum Generation and Initial Estimation

### Goal

Build a deterministic synthetic hum source that can be used to develop and validate Humbug's detection and cancellation algorithms, then begin estimating the harmonic content of that signal.

### Decisions

* Implemented a reusable sine oscillator using normalized phase, where one full cycle is represented by values from `0.0` to `1.0`.
* Added configurable initial phase so generated harmonics can begin at arbitrary phase offsets.
* Implemented `HumGenerator` as a fixed-size bank of eight harmonic oscillators.
* Each harmonic supports independent amplitude and phase while its frequency is derived from the mains fundamental.
* Added support for both 50 Hz and 60 Hz fundamentals.
* Added buffer-level hum generation.
* For stereo buffers, the hum sample is calculated once per sample frame and applied identically to each channel so oscillator phase does not advance separately between channels.
* Integrated synthetic hum generation into the processor behind an explicit enable flag. Synthetic hum remains disabled by default so existing DSP tests and normal plugin behavior remain isolated from the test signal.
* Implemented the first `HumEstimator`, initially assuming that the fundamental frequency is already known.
* Represented each harmonic using sine and cosine components so both amplitude and phase can be recovered.
* Added estimator tests using clean hum, deterministic white noise, and unrelated tonal interference.

### What I Learned

* Normalized phase provides a convenient representation for oscillator state and makes harmonic phase configuration straightforward.
* Generating one hum sample per sample frame is important for stereo processing. Calling the oscillator separately for each channel would cause the channels to drift by one sample of phase.
* A known-frequency sinusoid can be represented as a weighted combination of sine and cosine components. Those coefficients can then be converted back into amplitude and phase.
* Sine/cosine correlation accurately recovered harmonic parameters when the analysis window contained an integer number of fundamental cycles.
* The estimator remained accurate in the presence of moderate white noise and an unrelated 440 Hz tone when given a sufficiently long analysis window.
* Shorter analysis windows exposed an unexpected dependency on window alignment.
* At 48 kHz with a 60 Hz fundamental, clean multi-harmonic tests produced the following pattern:

  * 800 samples / 1 cycle: passed
  * 1200 samples / 1.5 cycles: failed
  * 1600 samples / 2 cycles: passed
  * 2000 samples / 2.5 cycles: failed
  * 2400 samples / 3 cycles: passed
* Because the same behavior occurred with a clean synthetic signal, the failure was traced to the estimator rather than noise or insufficient averaging.
* The initial estimator implicitly assumed that the harmonic sine/cosine basis functions were orthogonal over the analysis window. That assumption is valid for integer-cycle windows but not for arbitrary real-time buffer lengths.

### Next Step

Replace the independent-correlation estimator with a simultaneous least-squares model that accounts for correlation between all harmonic sine/cosine components and works with arbitrary analysis-window lengths.

---

## 2026-08-14 — Least-Squares Harmonic Estimation

### Goal

Remove the estimator's dependency on integer-cycle analysis windows and make harmonic amplitude/phase estimation reliable for arbitrary buffer lengths.

### Decisions

* Model the complete hum signal as the simultaneous sum of sine and cosine components for all eight harmonics.
* Use two coefficients per harmonic, producing a 16-parameter linear model.
* Implemented a fixed-size linear-system solver using Gaussian elimination with partial pivoting.
* Added independent tests for:

  * 2x2 systems
  * 3x3 systems
  * singular-system detection
* Avoid constructing the full analysis matrix in memory.
* Instead, accumulate the normal-equation terms sample-by-sample:

  * `A^T A`
  * `A^T x`
* Solve the resulting 16x16 system:

  * `(A^T A)c = A^T x`
* Convert each solved sine/cosine coefficient pair back into harmonic amplitude and normalized phase.
* Added regression tests using deliberately non-integer-cycle analysis windows.

### What I Learned

* The original correlation estimator effectively assumed that the off-diagonal terms of `A^T A` were zero.
* For arbitrary window lengths, different harmonic basis functions can have nonzero cross-correlation. Solving all harmonic coefficients simultaneously allows the estimator to account for those relationships instead of treating each harmonic independently.
* The full analysis matrix does not need to be stored. Only the 16x16 normal matrix and 16-element right-hand-side vector are required, which keeps the implementation small and fixed-size.
* The least-squares implementation successfully fixed the previously failing non-integer-cycle cases.
* Clean arbitrary-window estimation now works at window sizes that failed under the original correlation method.
* Additional tests were run at 1200, 2000, and 2200 samples using:

  * clean multi-harmonic hum
  * hum with deterministic white noise
  * hum with unrelated 440 Hz tonal interference
* All three conditions passed at 2000 and 2200 samples.
* At 1200 samples, the clean and white-noise cases passed, while the unrelated-tone test showed small errors in the 120 Hz estimate:

  * amplitude expected: `0.080`
  * amplitude estimated: `0.085438`
  * phase expected: `0.410`
  * phase estimated: `0.395937`
* This remaining error appears to represent a genuine short-window signal-separation limitation rather than the mathematical defect present in the original estimator.
* Coherent tonal interference can be more difficult to reject over a short observation window than broadband white noise because the interfering sinusoid maintains structured correlation with the modeled basis functions.
* The test tolerance should not be loosened simply to hide this behavior; the result is useful information about the latency-versus-frequency-separation tradeoff that will matter in the real-time implementation.

### Next Step

Complete the current hum-generation/estimation milestone and move into Learn Mode's remaining major task: detecting the mains fundamental frequency rather than supplying it to the estimator in advance. Once the fundamental can be identified automatically, the estimated harmonic amplitudes and phases can be used by the fixed harmonic subtraction stage.

## 8/15/26

Worked on fundamental-frequency detection for Learn Mode.

* Extended `HumEstimator` to report least-squares residual energy so candidate fundamentals can be scored by fit quality.
* Added `FundamentalFrequencyDetector` with constrained searches around 50 Hz and 60 Hz.
* Added quadratic refinement to improve frequency estimates between the 0.1 Hz search steps.
* Fixed a floating-point stepping issue that could skip the upper edge of a search range.
* Tested detection with non-grid frequencies, weak fundamentals, white noise, unrelated tonal interference, and different interference phases.
* Found that a longer analysis window was important for robust detection under interference; 250 ms worked well in the current synthetic tests.
* Added basic hum-presence classification using explained signal energy and support from multiple harmonics, allowing the detector to reject silence, noise, unrelated tones, and isolated single harmonics.
* Updated DSP notes with the frequency-detection approach, current thresholds, and test findings.

## 8/16/26

Started work on Fixed Harmonic Subtraction.

* Added `HumReconstructor` to synthesize a hum waveform from the frequencies, amplitudes, and phases returned by `HumEstimator`.
* Verified reconstruction first against a manually defined harmonic model, then against a model estimated from synthetic hum generated by `HumGenerator`.
* Added the first fixed-subtraction test by combining a known clean signal with synthetic hum, reconstructing the estimated hum, and subtracting it from the mixture.
* Added a quantitative cancellation test using RMS error before and after subtraction. In the ideal synthetic case, the reconstructed model reduced the hum from about `0.237` RMS to roughly `8.6e-9` RMS, or about `-149 dB` attenuation.
* Identified an important phase-alignment issue when cancellation begins after the analysis window rather than at its first sample.
* Updated `HumReconstructor` so a learned model can be advanced by an elapsed sample offset before reconstruction begins.
* Added a regression test using a 2000-sample analysis window followed by a separate cancellation window, confirming that phase continuation remains aligned with the original continuous hum signal.

## 8/17/26

Completed the standalone Fixed Harmonic Subtraction path and began laying the groundwork for Learn Mode integration.

* Added a more realistic cancellation test where `HumEstimator` learns the hum from a mixture containing both hum and an unrelated clean tone.
* Reused the 250 ms / 12000-sample Learn Mode window identified during frequency-detection testing and used a 997 Hz clean tone to avoid an artificially favorable integer-cycle case.
* The mixed-signal test reduced hum-related RMS error from about `0.2373` to `0.000584`, corresponding to roughly `-52.2 dB` attenuation.
* Added `FixedHumCanceller` to orchestrate fundamental detection, harmonic estimation, phase-aligned reconstruction, and sample-by-sample subtraction behind a small DSP API.
* Verified that `FixedHumCanceller` reproduces the same approximately `-52.2 dB` cancellation result as the manually wired DSP chain.
* Added negative-path coverage confirming that unrelated audio does not activate cancellation and is passed through unchanged.
* Added reset/state tests confirming that an active learned cancellation model can be disabled cleanly.
* Added `LearnBuffer` as the first piece of Learn Mode integration. It preallocates a 250 ms analysis buffer and collects incoming audio across arbitrary host block sizes without resizing during collection.
* Added tests covering arbitrary block boundaries, final-block overshoot, protection against writes after collection completes, and reuse for subsequent Learn passes.
* Ran the full test suite successfully after the Fixed Harmonic Subtraction work.

Fixed Harmonic Subtraction is now functionally complete as a standalone DSP milestone. The next major task is realtime Learn Mode integration: collecting the analysis window in the plugin, performing the more expensive detection/estimation work outside the realtime audio callback, and safely activating the resulting cancellation model while audio continues processing.
