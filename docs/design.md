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

### HumReconstructor

Reconstructs the estimated hum waveform from the harmonic model produced by
`HumEstimator`.

For each estimated harmonic, the reconstructor stores:

* frequency
* amplitude
* normalized phase

Each harmonic is synthesized using an `Oscillator`, and the harmonic outputs
are summed to produce the estimated hum sample.

The reconstructor can also begin synthesis at a sample offset relative to the
start of the analysis window. This allows a model whose phase was estimated at
analysis sample 0 to remain phase-aligned when cancellation begins after the
analysis window has completed.

The sample offset advances each harmonic according to its own frequency before
reconstruction begins.

### FixedHumCanceller

Coordinates the fixed cancellation DSP path.

Its current responsibilities are:

1. Run `FundamentalFrequencyDetector` on a supplied analysis buffer.
2. Reject the Learn result if the detector does not classify the signal as hum.
3. Estimate harmonic amplitudes and phases using the detected fundamental.
4. Initialize `HumReconstructor` at the sample immediately following the
   analysis window.
5. Subtract reconstructed hum from subsequent input samples.

Conceptual flow:

```text
analysis buffer
    |
    v
FundamentalFrequencyDetector
    |
    v
hum detected?
    |
    +---- no ---> cancellation remains inactive
    |
    +---- yes
            |
            v
      HumEstimator
            |
            v
      harmonic model
            |
            v
      HumReconstructor
            |
            v
input sample - reconstructed hum
            |
            v
       output sample
```

`FixedHumCanceller` keeps mathematical detection validity separate from hum
classification. If Learn Mode completes successfully but no convincing hum is
found, cancellation remains inactive and input samples pass through unchanged.

Calling `reset()` clears the active cancellation state and returns the
canceller to pass-through behavior.

### LearnBuffer

Collects the fixed-duration analysis window used by Learn Mode.

The current analysis duration is:

```text
250 ms
```

At 48 kHz this corresponds to:

```text
12000 samples
```

The buffer size is calculated from the actual sample rate rather than
hard-coded to 12000 samples.

`LearnBuffer` is prepared ahead of time and then collects samples across
arbitrary host block boundaries. Collection stops exactly when the configured
analysis window is full, even if the final host block contains more samples
than are required.

The current design keeps allocation out of the collection path:

* `prepare()` allocates the analysis buffer.
* `start()` begins a new capture using the existing allocation.
* `push()` copies samples into the preallocated buffer.
* `reset()` clears collection state without reallocating.

Once full, further calls to `push()` do not modify the captured analysis
window.

`LearnBuffer` currently handles capture only. The future handoff of a completed
analysis window to non-realtime detection and estimation is intentionally a
separate architectural concern.

## Real-Time Constraints

DSP processing should avoid:

* dynamic allocation
* locks
* UI access
* unnecessary container resizing
* expensive Learn Mode analysis directly inside the realtime audio callback

Fixed-size arrays are preferred for the known maximum harmonic count.

Learn Mode capture should use memory allocated ahead of time. Frequency
detection and least-squares model fitting are substantially more expensive than
sample-by-sample reconstruction and subtraction and should eventually be
performed outside the realtime audio callback.

The realtime processing path should consume an already-learned cancellation
model rather than perform model search or fitting for every block.

## Parameters and State

`AudioProcessorValueTreeState` is the central source of truth for host-visible
parameters, editor controls, and serialized plugin state.

Synthetic hum injection is disabled by default and is currently exposed only
as a development/testing mechanism.
