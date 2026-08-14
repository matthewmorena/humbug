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