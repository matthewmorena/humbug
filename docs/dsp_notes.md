# DSP Notes

## Hum Signal Model

Hum is modeled as a sum of harmonically related sinusoids:

```text
x(t) =
    A1 sin(wt + phi1)
  + A2 sin(2wt + phi2)
  + ...
  + AN sin(Nwt + phiN)
```

where:

- `A` is the amplitude of each harmonic
- `phi` is its phase
- `w` is determined by the mains fundamental frequency

The current implementation supports eight harmonics.

## Sine/Cosine Representation

For estimation, each harmonic is represented as:

```text
x_k(t) =
    a_k sin(k w t)
  + b_k cos(k w t)
```

This representation is linear in `a_k` and `b_k`.

Amplitude and phase can then be recovered using:

```text
A_k = sqrt(a_k^2 + b_k^2)

phi_k = atan2(b_k, a_k)
```

Phase is stored as normalized cycles in the range `[0, 1)`.

## Initial Correlation Estimator

The first estimator calculated sine and cosine correlations independently for each harmonic.

This worked accurately when the analysis window contained an integer number of fundamental cycles.

Testing revealed a strong dependency on window alignment:

- 800 samples at 48 kHz / 60 Hz = 1 cycle -> passed
- 1200 samples = 1.5 cycles -> failed
- 1600 samples = 2 cycles -> passed
- 2000 samples = 2.5 cycles -> failed
- 2400 samples = 3 cycles -> passed

The problem occurred even with clean synthetic multi-harmonic signals, demonstrating that it was caused by harmonic cross-correlation rather than noise.

## Simultaneous Least-Squares Estimator

The estimator was changed to fit all harmonic sine/cosine components simultaneously.

For eight harmonics, the model has sixteen coefficients:

```text
[a1, b1, a2, b2, ..., a8, b8]
```

Rather than constructing the full analysis matrix `A`, the implementation accumulates:

```text
A^T A
```

and:

```text
A^T x
```

sample-by-sample.

The resulting 16x16 system:

```text
(A^T A)c = A^T x
```

is solved using Gaussian elimination with partial pivoting.

This removes the assumption that the harmonic basis functions are orthogonal over the analysis window.

Regression tests confirm accurate estimation for non-integer-cycle windows that failed with the original correlation method.

## Interference Experiments

The estimator has been tested with:

- clean multi-harmonic hum
- additive deterministic white noise
- an unrelated 440 Hz sinusoid

The least-squares estimator successfully recovered hum from arbitrary window lengths in the clean and white-noise cases.

A 1200-sample (25 ms) window showed small estimation errors when a strong 440 Hz tone was present:

120 Hz amplitude:

- expected: 0.080
- estimated: 0.085438

120 Hz phase:

- expected: 0.410
- estimated: 0.395937

The same interference test passed at 2000 and 2200 samples.

This suggests a practical tradeoff between analysis-window duration and separation of coherent out-of-model tonal interference.

The test tolerance should not simply be loosened to hide this behavior; it represents a real limitation of short observation windows.

## Fundamental Frequency Detection

Fundamental-frequency detection reuses the simultaneous least-squares hum model. Rather than applying a general-purpose pitch detector, Humbug evaluates candidate mains frequencies and selects the candidate whose harmonic model produces the smallest residual energy.

For a least-squares solution, residual energy can be calculated as:

```text
E_residual = x^T x - c^T A^T x
```

where `c` is the solved coefficient vector. `HumEstimator::fit()` exposes this residual alongside the harmonic amplitude and phase estimates.

### Frequency Search

The detector currently searches two constrained mains-frequency regions:

- 48-52 Hz
- 58-62 Hz

Candidates are evaluated in 0.1 Hz increments.

The search originally advanced the candidate frequency using repeated floating-point addition. Testing showed that this could cause the nominal upper boundary of a search range to be skipped due to accumulated floating-point error. Candidate frequencies are now generated from integer step indices instead.

### Narrow Frequency Search for Adaptive Tracking

The full mains-frequency search is appropriate for initial Learn because no reliable prior frequency is available.

Repeating that complete search for every automatic tracking window proved too expensive for realtime adaptive operation. The initial processor integration used the full search for tracking and produced an analysis-to-activation delay of roughly:

```text
145000 samples
≈ 3.0 seconds at 48 kHz
```

During a controlled 60.0 -> 60.2 Hz drift, only one tracking update was applied during the five-second test and late cancellation degraded to approximately:

```text
+4.1 dB
```

Automatic tracking now uses a constrained search centered on the most recently accepted fundamental:

```text
referenceFrequency ± 0.3 Hz
```

The narrow search keeps the same:

- 0.1 Hz coarse grid
- least-squares residual-energy objective
- quadratic sub-grid refinement
- hum-presence classification

but reduces the number of candidate fits substantially.

In the same processor-level drift test, the optimized tracking path reduced analysis-to-activation delay to roughly:

```text
29000 samples
≈ 0.6 seconds at 48 kHz
```

Four tracking updates were applied during the five-second drift, following the source through approximately:

```text
60.045 Hz
60.084 Hz
60.125 Hz
60.165 Hz
```

Late attenuation improved to approximately:

```text
-19.9 dB
```

This confirms that analysis latency is itself a DSP concern for adaptive tracking: an accurate model can still be ineffective if it becomes stale before activation.

### Sub-Grid Refinement

A 0.1 Hz search step is sufficient to locate the neighborhood of the minimum but is too coarse for the desired frequency estimate.

After the lowest-residual candidate is found, the detector evaluates the residual at the candidate and its two neighboring grid points. Quadratic interpolation is then used to estimate the position of the minimum between search steps.

For neighboring residuals `E-`, `E0`, and `E+`, the offset in grid steps is:

```text
delta =
    0.5 * (E- - E+)
    / (E- - 2E0 + E+)
```

The refined frequency is:

```text
f_refined =
    f0 + delta * searchStep
```

For a synthetic 59.73 Hz hum, the 0.1 Hz grid initially selected 59.7 Hz. Quadratic refinement recovered approximately 59.73 Hz.

The same approach was verified around both the 50 Hz and 60 Hz mains regions.

A second floating-point boundary issue appeared after narrow tracking search was introduced.

A synthetic 60.18 Hz signal searched over:

```text
59.7-60.3 Hz
```

correctly selected 60.2 Hz as the best coarse point, but refinement was skipped.

The previous implementation tested whether:

```text
coarseFrequency ± searchStep
```

remained inside the search range using floating-point comparisons. Values such as `60.2 + 0.1` can be represented slightly above `60.3`, causing an interior grid point to be misclassified as a boundary point.

Refinement eligibility is now determined using the integer index of the winning coarse-search step. Quadratic refinement is allowed whenever the winning index has both a left and right grid neighbor.

The 60.18 Hz regression then refined correctly instead of remaining at the coarse 60.2 Hz estimate.

### Frequency-Detection Window Length

Frequency detection proved more sensitive to analysis-window duration than estimation performed at an already-known fundamental frequency.

With a 2000-sample window at 48 kHz, approximately 41.7 ms, white noise shifted a 59.73 Hz estimate to approximately 59.81 Hz. A strong unrelated 440 Hz sinusoid caused a more severe error and pulled the detector to the upper edge of the 60 Hz search range.

Increasing the observation window to 12000 samples, or 250 ms at 48 kHz, resolved both cases.

Representative results using the 250 ms window were:

- 59.73 Hz with white noise -> approximately 59.7288 Hz
- 59.73 Hz with a 440 Hz interfering tone -> approximately 59.7316 Hz
- weak 59.73 Hz fundamental with stronger upper harmonics and a 440 Hz interfering tone -> approximately 59.73 Hz

The weak-fundamental/interference test was repeated with multiple interference phases and remained within a few thousandths of a hertz of the true fundamental.

A 250 ms analysis window is therefore a useful initial value for Learn Mode, but it should be reevaluated using recorded audio.

### Hum Presence Classification

A lowest-residual frequency does not by itself prove that mains hum is present. A best candidate will exist even for silence, white noise, or unrelated tonal content.

The detector therefore distinguishes between:

- a mathematically valid frequency fit
- sufficient evidence that the fitted signal represents mains hum

One useful measure is the fraction of input energy explained by the harmonic model:

```text
explainedFraction =
    1 - residualEnergy / inputEnergy
```

Testing produced the following approximate values:

- clean synthetic hum: 1.000
- hum with white noise: 0.985
- hum with unrelated 440 Hz tone: 0.827
- weak fundamental with unrelated tone: 0.842
- white noise only: 0.002
- unrelated 440 Hz tone only: 0.045

Explained energy alone is not sufficient. A pure 420 Hz tone can be represented perfectly as the seventh harmonic of a 60 Hz fundamental:

```text
420 Hz = 7 * 60 Hz
```

The model therefore explains essentially 100% of that signal despite there being no broader evidence of a 60 Hz harmonic structure.

To reduce this type of false positive, the detector also requires support from multiple fitted harmonics.

A harmonic is currently considered supported when its amplitude is at least 5% of the strongest fitted harmonic. Hum is reported only when:

- the harmonic model explains at least 10% of the input energy
- at least two harmonics are meaningfully supported

The current empirical thresholds are:

```cpp
minimumExplainedFraction = 0.1;
minimumRelativeHarmonicAmplitude = 0.05;
minimumSupportedHarmonics = 2;
```

These values are initial engineering thresholds rather than theoretically derived constants and should be reevaluated with recorded audio.

Regression tests confirm that the current classification rejects:

- silence
- deterministic white noise
- an unrelated 440 Hz sinusoid
- an isolated 420 Hz sinusoid that exactly matches a 60 Hz harmonic

while accepting a synthetic hum containing only two supported harmonics.

The detector result keeps mathematical validity separate from hum classification. A candidate frequency can therefore represent a valid model fit while `humDetected` remains false.

## Fixed Harmonic Subtraction

Once the mains fundamental has been detected and the harmonic amplitudes and phases have been estimated, the learned hum model can be reconstructed and subtracted from subsequent input samples.

If the estimated hum is:

```text
h_hat(t) =
    sum_k A_k sin(2 pi f_k t + phi_k)
```

then fixed subtraction produces:

```text
y(t) =
    x(t) - h_hat(t)
```

where:

- `x(t)` is the input containing desired signal plus hum
- `h_hat(t)` is the reconstructed hum estimate
- `y(t)` is the cancellation output

### Hum Reconstruction

`HumReconstructor` consumes the harmonic model returned by `HumEstimator`.

For each harmonic it uses the estimated:

- frequency
- amplitude
- normalized phase

and synthesizes:

```text
h_hat_k(t) =
    A_k sin(
        2 pi (
            f_k t + phi_k
        )
    )
```

The reconstructed hum sample is the sum of all supported harmonic components.

Initial tests verified reconstruction in two stages.

First, a manually specified harmonic model was reconstructed directly and compared sample-by-sample with its analytical waveform.

Second, synthetic hum was generated using `HumGenerator`, estimated using `HumEstimator`, reconstructed using `HumReconstructor`, and compared with the original generated signal.

Both tests passed within the expected floating-point tolerance.

### Phase Continuation After the Analysis Window

The phase returned by `HumEstimator` is referenced to sample 0 of the analyzed buffer.

If reconstruction begins later in the signal timeline, starting directly from the stored phase would restart the learned waveform at the beginning of the analysis window rather than continuing it.

For a harmonic with frequency `f`, estimated normalized phase `phi`, sample offset `N`, and sample rate `Fs`, the reconstruction start phase is advanced by:

```text
phi_start =
    phi
    + f * N / Fs
```

and wrapped into the normalized phase range `[0, 1)`.

Equivalently, the elapsed time is:

```text
t_elapsed =
    N / Fs
```

and:

```text
phi_start =
    phi
    + f * t_elapsed
```

modulo one cycle.

A regression test was created using a 2000-sample analysis window followed by a separate cancellation window.

At 48 kHz, 2000 samples corresponds to approximately 41.67 ms. For a 60 Hz fundamental this advances the first three harmonics by:

```text
60 Hz  -> 2.5 cycles
120 Hz -> 5.0 cycles
180 Hz -> 7.5 cycles
```

Without phase advancement, the test failed because the fundamental and third harmonic restarted half a cycle out of phase.

After adding sample-offset phase continuation to `HumReconstructor`, the test passed and reconstruction remained aligned with the continuous source signal.

### Ideal Fixed-Subtraction Experiment

The first subtraction experiment used:

```text
known clean signal
+
known synthetic hum
```

The hum was estimated from a hum-only analysis buffer, reconstructed, and then subtracted from the mixture.

Cancellation error was measured relative to the known clean signal.

Before subtraction:

```text
error_before =
    mixed - clean
```

which is equivalent to the injected hum.

After subtraction:

```text
error_after =
    output - clean
```

which represents the remaining hum reconstruction error.

The RMS values were:

```text
Hum RMS before: 0.2373241749
Hum RMS after:  0.0000000086
```

The attenuation was calculated as:

```text
attenuation_dB =
    20 log10(
        RMS_after / RMS_before
    )
```

which produced approximately:

```text
-148.86 dB
```

This result represents an ideal synthetic case with exact model compatibility, known fundamental frequency, stationary hum, and perfect timeline alignment.

It should be treated as validation of the implementation rather than an expected real-world cancellation level.

The regression test currently requires at least 60 dB of attenuation rather than encoding the much larger observed value.

### Cancellation Learned From Mixed Signal

A more realistic experiment allowed the desired signal to be present during the Learn window.

The test signal contained:

```text
60 / 120 / 180 Hz synthetic hum
+
997 Hz unrelated sinusoid
```

The use of 997 Hz avoids an artificially favorable case where the desired tone contains an integer number of cycles within the analysis window.

A short 2000-sample analysis window produced approximately:

```text
-35.63 dB
```

of cancellation.

This showed that unrelated coherent signal content can bias the fitted hum coefficients over a short finite observation window.

The analysis duration was then increased to the 250 ms Learn Mode window identified during the frequency-detection experiments.

At 48 kHz:

```text
analysis window = 12000 samples
```

Using the 250 ms mixed-signal analysis window produced:

```text
Hum RMS before: 0.2373241739
Hum RMS after:  0.0005836854
Attenuation:    -52.18 dB
```

The current regression test requires at least:

```text
-40 dB
```

of attenuation in this synthetic mixed-signal case.

This preserves useful margin rather than treating the exact observed value as a production requirement.

The experiment reinforces the earlier finding that analysis-window duration affects not only fundamental-frequency detection but also the accuracy of harmonic parameter estimation in the presence of coherent out-of-model content.

### Learn Mode Analysis Buffer

`LearnBuffer` collects the initial fixed-duration Learn Mode observation window across arbitrary host audio block sizes.

For the current 250 ms target:

```text
numAnalysisSamples =
    round(
        sampleRate * 0.25
    )
```

At 48 kHz this is 12000 samples.

Testing verifies that:

- samples remain continuous across arbitrary block boundaries
- the final host block may exceed the remaining Learn window capacity
- only the required portion of the final block is copied
- samples arriving after collection completes do not overwrite the captured analysis window
- the preallocated buffer can be reused for another Learn pass

The 250 ms duration remains an empirical starting point. It provided robust frequency detection and substantially improved mixed-signal harmonic estimation in the current synthetic experiments, but it should be reevaluated using recorded audio.

## Fixed Harmonic Cancellation

> Note: `FixedHumCanceller` now serves primarily as a fixed-model regression baseline. The production processor uses `AdaptiveHumCanceller`, described later in this document.

Once a valid hum model has been estimated, Humbug reconstructs the fitted harmonics and subtracts them from the input:

```text
output[n] =
    input[n] - estimatedHum[n]
```

The realtime `FixedHumCanceller` does not perform frequency detection or least-squares estimation itself. Analysis is handled separately by `HumAnalyzer`, while `FixedHumCanceller` owns only the active reconstruction state used by the audio thread.

This separation allows expensive Learn analysis to occur on a worker thread without concurrently modifying realtime reconstruction state.

### Phase Continuation

The phase returned by `HumEstimator` describes the fitted signal relative to the beginning of the Learn analysis window.

Cancellation generally begins substantially later:

```text
analysis begins
    ↓
250 ms captured
    ↓
background analysis
    ↓
model publication
    ↓
next audio block boundary
    ↓
cancellation activates
```

The reconstruction model must therefore be advanced by the complete number of samples elapsed since the beginning of analysis.

The current phase offset is:

```text
sampleOffset =
    activationSample
    - analysisStartSample
```

This automatically accounts for the analysis window, host-block overshoot, worker latency, and any additional audio blocks processed before activation.

Synthetic regression tests intentionally introduce delayed activation and confirm that the model remains phase aligned.

This continuation is exact when the learned harmonic frequencies remain stationary during the delay. If the physical source continues drifting while analysis runs, advancing an older constant-frequency model cannot recover the unobserved frequency evolution. This is one reason low tracking-analysis latency is important.

### Realtime Learn Analysis

Learn capture uses a preallocated 250 ms buffer.

Once capture completes, the buffer is frozen and transferred logically to a background `HumAnalysisWorker`. The worker performs:

```text
FundamentalFrequencyDetector
+
HumEstimator
```

and produces a fixed-size `LearnedHumModel`.

A single-slot lock-free mailbox publishes the completed result to the audio thread. The audio thread consumes and activates the model at a host-block boundary.

The worker never modifies the active realtime canceller.

### Processor-Level Cancellation Tests

The complete production path has been tested through `HumbugAudioProcessor::processBlock()` rather than only through isolated DSP components.

An ideal processor-generated harmonic hum test produced approximately:

```text
baseline RMS: 0.0387299
post-cancellation RMS: ~7e-10
attenuation: -154 dB
```

This is an intentionally ideal, model-compatible synthetic case. Permanent regression thresholds remain much more conservative and should not be tightened to the observed numerical floor.

Processor tests also verify relearning behavior:

```text
active hum model
    ↓
source hum disappears
    ↓
new Learn captures raw no-hum input
    ↓
analysis reports humDetected == false
    ↓
previous cancellation model is disabled
    ↓
exact pass-through
```

Capturing raw input before cancellation is important. Otherwise a relearn while an old model is active could analyze the cancellation signal generated by the plugin rather than the actual source.

## Initial DAW and Real-World Signal Testing

Realtime Learn Mode was tested manually in Studio One 5.

A generated signal containing independent 60 Hz, 120 Hz, and 180 Hz components was routed to a mono bus containing Humbug.

A representative Learn result was:

```text
frequency:           59.99992245 Hz
explainedFraction:   0.99999996
supportedHarmonics:  3
humDetected:         true
```

The generated hum was initially cancelled very strongly.

Electric-guitar testing also produced successful positive hum detections.

Representative consecutive Learn results were:

```text
59.98289918 Hz
59.97788728 Hz
```

Relearning generally restored or improved cancellation after the original model began to lose effectiveness.

### Fixed-Frequency Phase Drift

Manual testing exposed an important limitation of fixed reconstruction.

If the learned fundamental is:

```text
f_est
```

and the actual source is:

```text
f_actual
```

then the frequency error is:

```text
deltaF =
    f_est - f_actual
```

The phase error grows continuously with time:

```text
phaseErrorCycles(t) =
    deltaF * t
```

For harmonic `k`, the error grows `k` times faster:

```text
phaseErrorCycles_k(t) =
    k * deltaF * t
```

Therefore even a very small fundamental-frequency error eventually causes the reconstructed signal to move out of phase with the source, with the highest modeled harmonics degrading first.

The generated 60/120/180 Hz test signal was estimated at approximately `59.999922 Hz`. Although this differs from nominal 60 Hz by less than `0.0001 Hz`, cancellation still degrades eventually because that error accumulates indefinitely.

Real guitar hum showed substantially larger variation between consecutive Learn estimates and correspondingly lost cancellation more quickly.

These results are consistent with the realtime phase-offset mechanism functioning as intended: the highly stable synthetic signal remains aligned much longer than the less stationary real-world guitar hum.

## Adaptive Harmonic Cancellation

Fixed reconstruction is fundamentally limited by model drift. Even small changes in fundamental frequency, harmonic amplitude, or harmonic phase cause the learned waveform to diverge from the physical source over time.

Adaptive tracking addresses this by periodically capturing new raw-input windows and estimating updated harmonic models.

The current tracking configuration is:

```text
capture duration:     250 ms
nominal cadence:      1 second
frequency search:     previous accepted frequency ± 0.3 Hz
model transition:     20 ms crossfade
```

Manual Learn remains responsible for establishing the initial authoritative model. Automatic tracking then follows gradual changes in the source.

### Fixed-Model Drift Regression

A controlled regression was created to quantify the limitation of fixed cancellation.

The source fundamental drifted linearly from:

```text
60.0 Hz -> 60.2 Hz
```

over five seconds while the harmonic amplitudes and phases remained fixed.

The initial model was learned from a stationary 60 Hz window.

Measured attenuation was approximately:

```text
early drift: -45.8 dB
late drift:   +5.1 dB
```

This represents roughly 51 dB of degradation over the five-second interval and provides a baseline for evaluating adaptive tracking.

### Adaptive Frequency-Drift Regression

Using periodic tracking on the same drift signal produced approximately:

```text
fixed late attenuation:     +5.1 dB
adaptive late attenuation: -20.3 dB
adaptive improvement:       ~25 dB
```

A second version added a 997 Hz desired tone to both the initial Learn and tracking input. The desired tone was subtracted from the measurement reference so that damage to wanted signal would also count as cancellation error.

Representative result:

```text
fixed late attenuation:     +5.1 dB
adaptive late attenuation: -20.2 dB
```

The adaptive improvement therefore remained substantial in the presence of coherent out-of-model tonal content.

### Harmonic-Amplitude Evolution

A separate regression held the fundamental at 60 Hz while changing harmonic amplitudes over five seconds:

```text
H1: 0.30 -> 0.18
H2: 0.15 -> 0.25
H3: 0.08 -> 0.04
```

Representative late attenuation:

```text
fixed:    -5.9 dB
adaptive: -22.3 dB
```

This confirms that periodic re-estimation is useful even when the fundamental frequency remains stationary.

### Harmonic-Phase Evolution

Phase changes require special handling in synthetic tests because calling `HumGenerator::setHarmonicPhase()` resets the oscillator phase.

To model a physical change without repeatedly resetting phase, the regression uses one discrete midpoint phase change and then holds the new phases fixed.

The tested phases changed from:

```text
H1: 0.18 -> 0.43
H2: 0.25 -> 0.05
H3: 0.41 -> 0.66
```

A post-change adaptive tracking update restored cancellation to approximately the numerical floor, while the stale fixed model degraded to roughly:

```text
+2.8 dB
```

This verifies that the adaptive system can recover from changes in the full harmonic phase model, not only fundamental-frequency drift.

### Combined Adaptive Stress Test

A combined regression includes:

- 60.0 -> 60.2 Hz fundamental drift
- changing harmonic amplitudes
- a midpoint harmonic-phase change
- a 997 Hz desired tone

Representative late attenuation:

```text
fixed:     +5.9 dB
adaptive: -17.1 dB
```

The result is weaker than the simpler isolated cases but still demonstrates a substantial adaptive improvement when several model parameters evolve at once.

### Adaptive Model Transitions

Replacing one reconstructed hum model instantaneously can create discontinuities when the old and new estimates differ in phase or amplitude.

`AdaptiveHumCanceller` therefore maintains two `HumReconstructor` instances.

During an automatic model update:

```text
old estimate ----\
                  >---- crossfade ----> subtraction estimate
new estimate ----/
```

The current transition duration is:

```text
20 ms
```

The crossfade occurs between reconstructed waveforms rather than directly interpolating phase values. This avoids interpolation problems caused by phase wrapping at one cycle.

The incoming reconstructor is first phase-continued to the actual activation sample using the model's analysis start timestamp. After the crossfade completes, that reconstructor becomes the active one and continues from its existing oscillator state.

Manual Learn is treated differently: it is authoritative and activates a fresh model directly.

### Processor-Level Adaptive Tracking

A processor-level regression exercises the complete production path rather than only the isolated adaptive components:

```text
manual Learn
    ↓
automatic tracking starts
    ↓
periodic raw-input capture
    ↓
background narrow analysis
    ↓
tracking model publication
    ↓
audio-thread model transition
```

The production-path 60.0 -> 60.2 Hz drift test originally exposed the latency problem described in the narrow-search section. After tracking switched to `analyzeNear()`, four automatic updates were applied during the five-second drift and late attenuation improved to approximately:

```text
-19.9 dB
```

The permanent processor regression requires useful margin rather than encoding the exact observed result.

### Adaptive DAW Observations

Initial DAW testing with real electric-guitar hum showed effective adaptive cancellation across the currently modeled first eight harmonics while the instrument was idle.

Three practical limitations were observed.

First, some tracking updates caused an audible change in residual hum level. The current 20 ms waveform crossfade prevents abrupt discontinuities, but it may not fully hide larger differences between successive amplitude or phase estimates.

Second, strong guitar playing can contaminate a tracking window. After loud strumming stops, cancellation may temporarily worsen until a subsequent cleaner tracking window restores the hum model. This suggests that future automatic updates should incorporate model-confidence or consistency gating rather than accepting every valid hum fit.

Third, substantial hum energy can remain above the current eight-harmonic model. Extending harmonic count is mechanically straightforward, but higher harmonics overlap increasingly with desired guitar content and therefore require greater care in estimation and update acceptance.

These observations do not invalidate the current adaptive approach, but they identify the next areas where recorded-signal testing should guide further DSP refinement.
