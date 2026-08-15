# Humbug Design Notes

## Processing Architecture

Current conceptual processing path:

Input
  -> hum analysis / estimation
  -> hum reconstruction and subtraction
  -> output gain
  -> Output

Synthetic hum generation is a development and testing utility rather than
part of the normal production signal path.

## DSP Components

### Oscillator

Provides a deterministic sine oscillator using normalized phase.

Normalized phase convention:

- `0.0` = 0 degrees
- `0.25` = 90 degrees
- `0.5` = 180 degrees
- `0.75` = 270 degrees

### HumGenerator

Generates a synthetic mains-hum signal as a sum of harmonic sinusoids.

Each harmonic has independent:

- amplitude
- phase
- frequency derived from the fundamental

The generator currently supports eight harmonics using fixed-size storage.

For stereo buffers, each hum sample is calculated once per sample frame and
added identically to every channel. This prevents oscillator phase from
advancing independently between channels.

### HumEstimator

Estimates the amplitude and phase of known-frequency hum harmonics.

The current estimator assumes the fundamental frequency is known and estimates
all harmonics simultaneously using least squares.

### FundamentalFrequencyDetector

Detects the fundamental frequency of mains hum by searching around the two expected mains-frequency regions: **50 Hz** and **60 Hz**.

For each candidate frequency, the detector uses `HumEstimator` to fit the input as a sum of harmonically related sinusoids. The candidate that produces the lowest residual energy is treated as the best match. This makes the detector sensitive to the complete harmonic structure of the hum rather than relying only on the strength of the fundamental.

The search is performed in two stages:

1. **Coarse search** — Frequencies from 48–52 Hz and 58–62 Hz are evaluated in 0.1 Hz increments.
2. **Refinement** — The best coarse candidate is refined using parabolic interpolation of the residual-energy values around the minimum. The refined frequency is accepted only if fitting at that frequency improves the result.

After selecting the best frequency, the detector classifies whether the signal contains meaningful hum. It calculates the fraction of input energy explained by the harmonic model and counts how many harmonics have significant amplitude relative to the strongest harmonic.

Hum is currently considered detected when:

* the harmonic model explains at least 10% of the input energy, and
* at least two harmonics have amplitudes of at least 5% of the strongest fitted harmonic.

The result therefore separates **frequency estimation** from **hum detection**: a best-fit frequency may still be returned even when the evidence is not strong enough to classify the input as hum.

## Real-Time Constraints

DSP processing should avoid:

- dynamic allocation
- locks
- UI access
- unnecessary container resizing

Fixed-size arrays are preferred for the known maximum harmonic count.

## Parameters and State

`AudioProcessorValueTreeState` is the central source of truth for host-visible
parameters, editor controls, and serialized plugin state.

Synthetic hum injection is disabled by default and is currently exposed only
as a development/testing mechanism.
