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

Fundamental-frequency detection will be implemented separately.

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
