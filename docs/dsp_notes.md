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

### FixedHumCanceller

`FixedHumCanceller` combines the fixed Learn and cancellation stages:

```text
analysis buffer
    |
    v
fundamental-frequency detection
    |
    v
hum-presence classification
    |
    v
harmonic estimation
    |
    v
phase advancement to end of analysis window
    |
    v
continuous reconstruction
    |
    v
subtraction
```

An end-to-end test using the same 250 ms mixed-signal Learn window produced the
same approximately `-52.18 dB` attenuation as the manually connected DSP
components.

This confirms that the orchestration layer does not materially change the
behavior of the underlying detector, estimator, or reconstructor.

Additional tests verify that:

* unrelated audio does not activate cancellation
* input is passed through unchanged when no hum is detected
* resetting an active canceller disables the learned cancellation model
* the inactive state remains exact pass-through

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

### Remaining Learn Mode Integration Work

The current fixed cancellation DSP has been validated independently of the
plugin's realtime control flow.

A completed Learn window still needs to be handed off for frequency detection
and least-squares estimation without performing the expensive Learn operation
directly inside the realtime audio callback.

Audio will continue to advance while that analysis takes place, so production
integration must also account for the number of samples elapsed between the
start of the analyzed window and the eventual activation of the learned
cancellation model.

This realtime handoff and model-publication problem belongs to Learn Mode
integration rather than the fixed harmonic subtraction primitive itself.