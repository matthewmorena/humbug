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
