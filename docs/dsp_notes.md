# DSP Notes

## Hum Signal Model

Hum is modeled as a sum of harmonically related sinusoids:

x(t) =
    A1 sin(wt + phi1)
  + A2 sin(2wt + phi2)
  + ...
  + AN sin(Nwt + phiN)

where:

- `A` is the amplitude of each harmonic
- `phi` is its phase
- `w` is determined by the mains fundamental frequency

The current implementation supports eight harmonics.

## Sine/Cosine Representation

For estimation, each harmonic is represented as:

x_k(t) =
    a_k sin(k w t)
  + b_k cos(k w t)

This representation is linear in `a_k` and `b_k`.

Amplitude and phase can then be recovered using:

A_k = sqrt(a_k^2 + b_k^2)

phi_k = atan2(b_k, a_k)

Phase is stored as normalized cycles in the range `[0, 1)`.

## Initial Correlation Estimator

The first estimator calculated sine and cosine correlations independently
for each harmonic.

This worked accurately when the analysis window contained an integer number
of fundamental cycles.

Testing revealed a strong dependency on window alignment:

- 800 samples at 48 kHz / 60 Hz = 1 cycle -> passed
- 1200 samples = 1.5 cycles -> failed
- 1600 samples = 2 cycles -> passed
- 2000 samples = 2.5 cycles -> failed
- 2400 samples = 3 cycles -> passed

The problem occurred even with clean synthetic multi-harmonic signals,
demonstrating that it was caused by harmonic cross-correlation rather than
noise.

## Simultaneous Least-Squares Estimator

The estimator was changed to fit all harmonic sine/cosine components
simultaneously.

For eight harmonics, the model has sixteen coefficients:

[a1, b1, a2, b2, ..., a8, b8]

Rather than constructing the full analysis matrix `A`, the implementation
accumulates:

A^T A

and:

A^T x

sample-by-sample.

The resulting 16x16 system:

(A^T A)c = A^T x

is solved using Gaussian elimination with partial pivoting.

This removes the assumption that the harmonic basis functions are orthogonal
over the analysis window.

Regression tests confirm accurate estimation for non-integer-cycle windows
that failed with the original correlation method.

## Interference Experiments

The estimator has been tested with:

- clean multi-harmonic hum
- additive deterministic white noise
- an unrelated 440 Hz sinusoid

The least-squares estimator successfully recovered hum from arbitrary window
lengths in the clean and white-noise cases.

A 1200-sample (25 ms) window showed small estimation errors when a strong
440 Hz tone was present:

120 Hz amplitude:
- expected: 0.080
- estimated: 0.085438

120 Hz phase:
- expected: 0.410
- estimated: 0.395937

The same interference test passed at 2000 and 2200 samples.

This suggests a practical tradeoff between analysis-window duration and
separation of coherent out-of-model tonal interference.

The test tolerance should not simply be loosened to hide this behavior;
it represents a real limitation of short observation windows.

## Fundamental Frequency Detection

Fundamental-frequency detection reuses the simultaneous least-squares hum model. Rather than applying a general-purpose pitch detector, Humbug evaluates candidate mains frequencies and selects the candidate whose harmonic model produces the smallest residual energy.

For a least-squares solution, residual energy can be calculated as:

E_residual = x^T x - c^T A^T x

where `c` is the solved coefficient vector. `HumEstimator::fit()` exposes this residual alongside the harmonic amplitude and phase estimates.

### Frequency Search

The detector currently searches two constrained mains-frequency regions:

* 48-52 Hz
* 58-62 Hz

Candidates are evaluated in 0.1 Hz increments.

The search originally advanced the candidate frequency using repeated floating-point addition. Testing showed that this could cause the nominal upper boundary of a search range to be skipped due to accumulated floating-point error. Candidate frequencies are now generated from integer step indices instead.

### Sub-Grid Refinement

A 0.1 Hz search step is sufficient to locate the neighborhood of the minimum but is too coarse for the desired frequency estimate.

After the lowest-residual candidate is found, the detector evaluates the residual at the candidate and its two neighboring grid points. Quadratic interpolation is then used to estimate the position of the minimum between search steps.

For neighboring residuals `E-`, `E0`, and `E+`, the offset in grid steps is:

delta =
0.5 * (E- - E+)
/ (E- - 2E0 + E+)

The refined frequency is:

f_refined =
f0 + delta * searchStep

For a synthetic 59.73 Hz hum, the 0.1 Hz grid initially selected 59.7 Hz. Quadratic refinement recovered approximately 59.73 Hz.

The same approach was verified around both the 50 Hz and 60 Hz mains regions.

### Frequency-Detection Window Length

Frequency detection proved more sensitive to analysis-window duration than estimation performed at an already-known fundamental frequency.

With a 2000-sample window at 48 kHz, approximately 41.7 ms, white noise shifted a 59.73 Hz estimate to approximately 59.81 Hz. A strong unrelated 440 Hz sinusoid caused a more severe error and pulled the detector to the upper edge of the 60 Hz search range.

Increasing the observation window to 12000 samples, or 250 ms at 48 kHz, resolved both cases.

Representative results using the 250 ms window were:

* 59.73 Hz with white noise -> approximately 59.7288 Hz
* 59.73 Hz with a 440 Hz interfering tone -> approximately 59.7316 Hz
* weak 59.73 Hz fundamental with stronger upper harmonics and a 440 Hz interfering tone -> approximately 59.73 Hz

The weak-fundamental/interference test was repeated with multiple interference phases and remained within a few thousandths of a hertz of the true fundamental.

A 250 ms analysis window is therefore a useful initial value for Learn Mode, but it should be reevaluated using recorded audio.

### Hum Presence Classification

A lowest-residual frequency does not by itself prove that mains hum is present. A best candidate will exist even for silence, white noise, or unrelated tonal content.

The detector therefore distinguishes between:

* a mathematically valid frequency fit
* sufficient evidence that the fitted signal represents mains hum

One useful measure is the fraction of input energy explained by the harmonic model:

explainedFraction =
1 - residualEnergy / inputEnergy

Testing produced the following approximate values:

* clean synthetic hum: 1.000
* hum with white noise: 0.985
* hum with unrelated 440 Hz tone: 0.827
* weak fundamental with unrelated tone: 0.842
* white noise only: 0.002
* unrelated 440 Hz tone only: 0.045

Explained energy alone is not sufficient. A pure 420 Hz tone can be represented perfectly as the seventh harmonic of a 60 Hz fundamental:

420 Hz = 7 * 60 Hz

The model therefore explains essentially 100% of that signal despite there being no broader evidence of a 60 Hz harmonic structure.

To reduce this type of false positive, the detector also requires support from multiple fitted harmonics.

A harmonic is currently considered supported when its amplitude is at least 5% of the strongest fitted harmonic. Hum is reported only when:

* the harmonic model explains at least 10% of the input energy
* at least two harmonics are meaningfully supported

The current empirical thresholds are:

```cpp
minimumExplainedFraction = 0.1;
minimumRelativeHarmonicAmplitude = 0.05;
minimumSupportedHarmonics = 2;
```

These values are initial engineering thresholds rather than theoretically derived constants and should be reevaluated with recorded audio.

Regression tests confirm that the current classification rejects:

* silence
* deterministic white noise
* an unrelated 440 Hz sinusoid
* an isolated 420 Hz sinusoid that exactly matches a 60 Hz harmonic

while accepting a synthetic hum containing only two supported harmonics.

The detector result keeps mathematical validity separate from hum classification. A candidate frequency can therefore represent a valid model fit while `humDetected` remains false.

## Fixed Harmonic Subtraction

Once the mains fundamental has been detected and the harmonic amplitudes and
phases have been estimated, the learned hum model can be reconstructed and
subtracted from subsequent input samples.

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

* `x(t)` is the input containing desired signal plus hum
* `h_hat(t)` is the reconstructed hum estimate
* `y(t)` is the cancellation output

### Hum Reconstruction

`HumReconstructor` consumes the harmonic model returned by `HumEstimator`.

For each harmonic it uses the estimated:

* frequency
* amplitude
* normalized phase

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

First, a manually specified harmonic model was reconstructed directly and
compared sample-by-sample with its analytical waveform.

Second, synthetic hum was generated using `HumGenerator`, estimated using
`HumEstimator`, reconstructed using `HumReconstructor`, and compared with the
original generated signal.

Both tests passed within the expected floating-point tolerance.

### Phase Continuation After the Analysis Window

The phase returned by `HumEstimator` is referenced to sample 0 of the analyzed
buffer.

If reconstruction begins later in the signal timeline, starting directly from
the stored phase would restart the learned waveform at the beginning of the
analysis window rather than continuing it.

For a harmonic with frequency `f`, estimated normalized phase `phi`, sample
offset `N`, and sample rate `Fs`, the reconstruction start phase is advanced by:

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

A regression test was created using a 2000-sample analysis window followed by
a separate cancellation window.

At 48 kHz, 2000 samples corresponds to approximately 41.67 ms. For a 60 Hz
fundamental this advances the first three harmonics by:

```text
60 Hz  -> 2.5 cycles
120 Hz -> 5.0 cycles
180 Hz -> 7.5 cycles
```

Without phase advancement, the test failed because the fundamental and third
harmonic restarted half a cycle out of phase.

After adding sample-offset phase continuation to `HumReconstructor`, the test
passed and reconstruction remained aligned with the continuous source signal.

### Ideal Fixed-Subtraction Experiment

The first subtraction experiment used:

```text
known clean signal
+
known synthetic hum
```

The hum was estimated from a hum-only analysis buffer, reconstructed, and then
subtracted from the mixture.

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

This result represents an ideal synthetic case with exact model compatibility,
known fundamental frequency, stationary hum, and perfect timeline alignment.

It should be treated as validation of the implementation rather than an
expected real-world cancellation level.

The regression test currently requires at least 60 dB of attenuation rather
than encoding the much larger observed value.

### Cancellation Learned From Mixed Signal

A more realistic experiment allowed the desired signal to be present during
the Learn window.

The test signal contained:

```text
60 / 120 / 180 Hz synthetic hum
+
997 Hz unrelated sinusoid
```

The use of 997 Hz avoids an artificially favorable case where the desired tone
contains an integer number of cycles within the analysis window.

A short 2000-sample analysis window produced approximately:

```text
-35.63 dB
```

of cancellation.

This showed that unrelated coherent signal content can bias the fitted hum
coefficients over a short finite observation window.

The analysis duration was then increased to the 250 ms Learn Mode window
identified during the frequency-detection experiments.

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

This preserves useful margin rather than treating the exact observed value as
a production requirement.

The experiment reinforces the earlier finding that analysis-window duration
affects not only fundamental-frequency detection but also the accuracy of
harmonic parameter estimation in the presence of coherent out-of-model
content.

### Learn Mode Analysis Buffer

`LearnBuffer` collects the initial fixed-duration Learn Mode observation
window across arbitrary host audio block sizes.

For the current 250 ms target:

```text
numAnalysisSamples =
    round(
        sampleRate * 0.25
    )
```

At 48 kHz this is 12000 samples.

Testing verifies that:

* samples remain continuous across arbitrary block boundaries
* the final host block may exceed the remaining Learn window capacity
* only the required portion of the final block is copied
* samples arriving after collection completes do not overwrite the captured
  analysis window
* the preallocated buffer can be reused for another Learn pass

The 250 ms duration remains an empirical starting point. It provided robust
frequency detection and substantially improved mixed-signal harmonic
estimation in the current synthetic experiments, but it should be reevaluated
using recorded audio.

## Fixed Harmonic Cancellation

Once a valid hum model has been estimated, Humbug reconstructs the fitted
harmonics and subtracts them from the input:

```text
output[n] =
    input[n] - estimatedHum[n]
```

The realtime `FixedHumCanceller` does not perform frequency detection or
least-squares estimation itself. Analysis is handled separately by
`HumAnalyzer`, while `FixedHumCanceller` owns only the active reconstruction
state used by the audio thread.

This separation allows expensive Learn analysis to occur on a worker thread
without concurrently modifying realtime reconstruction state.

### Phase Continuation

The phase returned by `HumEstimator` describes the fitted signal relative to
the beginning of the Learn analysis window.

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

The reconstruction model must therefore be advanced by the complete number of
samples elapsed since the beginning of analysis.

The current phase offset is:

```text
sampleOffset =
    activationSample
    - analysisStartSample
```

This automatically accounts for the analysis window, host-block overshoot,
worker latency, and any additional audio blocks processed before activation.

Synthetic regression tests intentionally introduce delayed activation and
confirm that the model remains phase aligned.

### Realtime Learn Analysis

Learn capture uses a preallocated 250 ms buffer.

Once capture completes, the buffer is frozen and transferred logically to a
background `HumAnalysisWorker`. The worker performs:

```text
FundamentalFrequencyDetector
+
HumEstimator
```

and produces a fixed-size `LearnedHumModel`.

A single-slot lock-free mailbox publishes the completed result to the audio
thread. The audio thread consumes and activates the model at a host-block
boundary.

The worker never modifies the active realtime canceller.

### Processor-Level Cancellation Tests

The complete production path has been tested through
`HumbugAudioProcessor::processBlock()` rather than only through isolated DSP
components.

An ideal processor-generated harmonic hum test produced approximately:

```text
baseline RMS: 0.0387299
post-cancellation RMS: ~7e-10
attenuation: -154 dB
```

This is an intentionally ideal, model-compatible synthetic case. Permanent
regression thresholds remain much more conservative and should not be tightened
to the observed numerical floor.

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

Capturing raw input before cancellation is important. Otherwise a relearn
while an old model is active could analyze the cancellation signal generated
by the plugin rather than the actual source.

## Initial DAW and Real-World Signal Testing

Realtime Learn Mode was tested manually in Studio One 5.

A generated signal containing independent 60 Hz, 120 Hz, and 180 Hz components
was routed to a mono bus containing Humbug.

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

Relearning generally restored or improved cancellation after the original
model began to lose effectiveness.

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

Therefore even a very small fundamental-frequency error eventually causes the
reconstructed signal to move out of phase with the source, with the highest
modeled harmonics degrading first.

The generated 60/120/180 Hz test signal was estimated at approximately
`59.999922 Hz`. Although this differs from nominal 60 Hz by less than
`0.0001 Hz`, cancellation still degrades eventually because that error
accumulates indefinitely.

Real guitar hum showed substantially larger variation between consecutive
Learn estimates and correspondingly lost cancellation more quickly.

These results are consistent with the realtime phase-offset mechanism functioning 
as intended: the highly stable synthetic signal remains aligned much longer than 
the less stationary real-world guitar hum.

### Implication for Adaptive Tracking

The next DSP milestone should not simply snap detected frequencies to exactly
50 or 60 Hz. Real mains-related interference may legitimately differ from its
nominal frequency, and forcing a nominal value could increase phase drift.

Instead, future adaptive processing should track gradual changes in:

* fundamental frequency
* harmonic amplitude
* harmonic phase

while smoothly updating realtime reconstruction state without producing
audible discontinuities.

The current fixed Learn Mode provides the initialization model for that future
tracking process.
