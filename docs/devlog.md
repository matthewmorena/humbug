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

## 8/19/26 — Realtime Learn Mode Integration

Began the Realtime Learn Mode Integration milestone by separating expensive Learn analysis from realtime cancellation state.

### Analysis / cancellation separation

Added a fixed-size `LearnedHumModel` value type containing:

* estimated harmonic parameters
* detected fundamental frequency
* `valid` analysis status
* `humDetected` classification status

Added `HumAnalyzer` to own the non-realtime Learn analysis path:

```text
analysis buffer
    ↓
FundamentalFrequencyDetector
    ↓
HumEstimator
    ↓
LearnedHumModel
```

Added tests verifying that `HumAnalyzer`:

* correctly detects and estimates a 60 Hz harmonic hum from a mixed 997 Hz + hum signal
* preserves the distinction between a valid analysis and actual hum detection
* returns an empty harmonic model when no hum is classified

### FixedHumCanceller refactor

Refactored `FixedHumCanceller` so it no longer performs detection or harmonic estimation.

The canceller now consumes a previously learned model through an explicit activation step:

```text
LearnedHumModel
    ↓
FixedHumCanceller::activateModel()
    ↓
HumReconstructor
    ↓
reconstruct + subtract
```

This separates worker-side analysis from the realtime reconstruction state that will eventually be owned exclusively by the audio thread.

Updated `HumReconstructor` phase-offset handling to use a 64-bit sample offset in preparation for long-running absolute sample tracking.

Refactored the existing `FixedHumCanceller` tests to use the new `HumAnalyzer` → `LearnedHumModel` → `activateModel()` flow. Existing cancellation, no-hum pass-through, and reset behavior remain unchanged.

### Delayed model activation

Added a regression test for delayed model activation.

The test:

```text
captures a 12000-sample / 250 ms Learn window
→ introduces an additional 317-sample simulated analysis delay
→ activates the learned model at sample 12317
→ begins cancellation from that point
```

The 317-sample delay intentionally represents a non-integer fraction of a 60 Hz cycle.

Measured cancellation remained phase-aligned:

```text
Hum RMS before: 0.2392327508
Hum RMS after:  0.0005151873
Attenuation:    -53.34 dB
```

This verifies that cancellation no longer needs to begin immediately after the analysis window. An explicit elapsed-sample offset can correctly advance the learned harmonic phases to a later point on the audio timeline.

### LearnModeController / absolute sample timeline

Added `LearnModeController` as the first coordinator for realtime Learn capture.

The controller currently owns:

```text
LearnBuffer
absolute processed-sample count
analysis-start sample position
```

It does not yet contain a worker thread, analyzer, canceller, or cross-thread synchronization.

Added tests verifying:

* absolute sample position advances across arbitrary host blocks
* Learn start records the current absolute sample position
* final-block overshoot remains part of the realtime timeline even though `LearnBuffer` stops exactly at 12000 captured samples
* the sample timeline continues advancing while a completed Learn buffer waits through simulated analysis latency
* resetting Learn capture preserves the monotonically increasing absolute sample position

A representative overshoot case now distinguishes:

```text
Learn samples captured: 12000
Realtime samples elapsed: 12288
```

and additional simulated worker delay continues to increase the eventual activation offset.

This establishes the timing relationship needed for future model activation:

```text
sampleOffset
    =
activationSample
    -
analysisStartSample
```

All tests pass.

### Next

The next step is to design ownership and handoff of the completed analysis buffer so that:

* the audio thread can continue processing without blocking
* a worker can safely read a completed Learn capture
* a new Learn operation cannot overwrite memory still being analyzed
* allocation and locking remain outside the realtime audio path

The initial design will likely use separate preallocated capture and analysis storage with only one outstanding Learn operation allowed at a time.

## 8/20/26 — Realtime Learn Mode Buffer Handoff and Worker Analysis

Continued Realtime Learn Mode integration by establishing safe ownership boundaries between the audio and analysis sides.

Expanded `LearnModeController` with an explicit lifecycle:

```text
Idle
→ Collecting
→ ReadyForAnalysis
→ Analyzing
→ ModelReady
→ Idle
```

The completed `LearnBuffer` remains frozen while analysis owns it, allowing realtime audio and the absolute sample timeline to continue without overwriting the captured Learn window. Tests now verify exclusive buffer claiming, rejection of new Learn requests while busy, frozen-buffer behavior during continued audio processing, and safe reuse after model handoff.

Added a fixed-size `PendingLearnResult` and lock-free `LearnedHumModelMailbox` for publishing learned models back toward the audio thread. The mailbox uses explicit ownership states to prevent unread results from being overwritten or producer/consumer access from overlapping.

Added `HumAnalysisWorker`, which runs `HumAnalyzer` on a JUCE background thread. The audio thread only publishes the completed capture state; the worker periodically claims ready captures, performs frequency detection and harmonic estimation, publishes the resulting model and original `analysisStartSample`, and transitions the controller to `ModelReady`.

Worker tests now verify:

* successful background analysis of a hum-containing Learn capture
* publication of valid no-hum results
* preservation of Learn-start timeline metadata
* multiple consecutive Learn operations through the same controller, worker, and mailbox

All tests pass.

Next step is to connect mailbox consumption to `FixedHumCanceller::activateModel()` and verify phase-correct cancellation using the real elapsed sample offset produced while background analysis is running.

## 8/21/26–8/23/26 — Realtime Learn Integration Tests

Added end-to-end integration coverage for the Realtime Learn Mode architecture.

The first integration test now exercises the complete asynchronous Learn path:

```text
continuous mixed signal
→ LearnBuffer capture
→ background HumAnalysisWorker
→ LearnedHumModelMailbox
→ block-boundary model consumption
→ absolute sample-offset calculation
→ FixedHumCanceller activation
```

The test keeps the synthetic hum phase continuous across pre-roll, Learn capture, background-analysis latency, and cancellation. Audio continues advancing while the worker runs, so the eventual activation point is intentionally nondeterministic.

The learned model is activated using:

```text
sampleOffset
    =
activationSample
    -
analysisStartSample
```

and remains phase aligned even after a large simulated realtime delay. Repeated runs produced roughly `-48 dB` of cancellation while the activation sample varied, confirming that cancellation does not depend on a fixed analysis delay.

Added a second integration test covering relearning while cancellation is already active:

```text
learn hum
→ activate cancellation
→ hum disappears
→ relearn from raw input
→ worker reports valid/no-hum result
→ old cancellation model disabled
→ exact pass-through
```

The test also preserves the intended processor ordering by capturing raw input before applying the currently active canceller.

Together, these tests now validate both major end-to-end Realtime Learn behaviors:

* asynchronous Learn followed by phase-correct cancellation
* asynchronous relearn capable of disabling a stale cancellation model

All tests pass.

### Next

Begin integrating the tested Learn controller, analysis worker, model mailbox, and fixed canceller into `PluginProcessor`, with learned models consumed and activated at host block boundaries.

## 8/24/26–8/28/26 — Realtime Learn Mode Processor Integration

Completed the initial Realtime Learn Mode integration and connected the previously isolated Learn architecture to the production `PluginProcessor` path.

`PluginProcessor` now owns the Learn controller, background analysis worker, learned-model mailbox, and fixed hum canceller. Learn requests are submitted through an atomic request flag and accepted by the audio thread at a host-block boundary rather than directly from the UI/message thread.

The realtime processing order is now:

```text
consume completed Learn result
→ handle pending Learn request
→ capture raw input
→ apply fixed hum cancellation
→ apply output gain
```

Completed learned models are consumed from the mailbox at block boundaries and activated using the absolute elapsed sample offset:

```text
sampleOffset =
    activationSample
    - analysisStartSample
```

This preserves reconstruction phase across the Learn window, worker-thread analysis time, host-block overshoot, and any additional audio processed before activation.

Added processor-level integration tests covering the complete production path. Tests verify that:

* a user Learn request starts capture through `PluginProcessor`
* background analysis publishes and activates a model automatically
* fixed cancellation is phase aligned when activated later
* relearning from a no-hum signal disables a previously active cancellation model
* raw input is captured before the active canceller modifies the output

The ideal processor-generated synthetic hum test produced approximately `-154 dB` attenuation, while regression requirements remain intentionally more conservative.

Added an initial user-facing **Learn** button. The editor polls processor status rather than receiving callbacks from the audio thread. Learn is currently available only for mono processing; stereo instances display the control as unavailable until a stereo cancellation policy is implemented.

Performed the first manual DAW validation in Studio One 5 using both electric-guitar hum and generated 60/120/180 Hz test tones.

A generated three-harmonic test signal was detected at approximately:

```text
59.999922 Hz
explainedFraction ≈ 1.0
supportedHarmonics = 3
```

and was initially cancelled very strongly.

Real guitar hum was also successfully detected and reduced, with repeated Learn passes producing estimates near:

```text
59.9829 Hz
59.9779 Hz
```

Listening tests exposed the expected limitation of the current fixed model: cancellation gradually loses effectiveness as the reconstructed oscillator drifts in phase relative to the source. The effect occurs much more slowly with the stable generated signal than with real guitar hum.

This provides the first real-world evidence motivating the upcoming adaptive-tracking milestone rather than indicating a failure of the realtime Learn handoff itself.

### Next

Document the completed Realtime Learn Mode milestone and begin designing adaptive tracking for gradual frequency, amplitude, and phase changes.
