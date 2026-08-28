# Humbug Design Notes

## Processing Architecture

The current realtime processor path is:

```text
Input
  |
  +----> Learn capture while collecting
  |
  v
Fixed hum cancellation when active
  |
  v
Output gain
  |
  v
Output
```

Learn analysis is deliberately separated from realtime cancellation.

```text
AUDIO THREAD

raw input
    |
    v
LearnModeController / LearnBuffer
    |
    | completed analysis window
    v

ANALYSIS WORKER

HumAnalyzer
    |
    +--> FundamentalFrequencyDetector
    |
    +--> HumEstimator
    |
    v
LearnedHumModel
    |
    v
LearnedHumModelMailbox
    |
    v

AUDIO THREAD

consume result at block boundary
    |
    v
FixedHumCanceller
```

Synthetic hum generation remains a development and testing utility rather than part of the normal production signal path.

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

Owns the realtime fixed-cancellation path.

`FixedHumCanceller` no longer performs frequency detection or harmonic estimation. It consumes an already-learned `LearnedHumModel` produced by the background analysis path.

Its responsibilities are:

1. Accept a learned harmonic model and elapsed sample offset.
2. Initialize `HumReconstructor` at the correct continuation phase.
3. Subtract the reconstructed hum from subsequent realtime input samples.
4. Remain in pass-through mode when no valid hum model is active.

Conceptual flow:

```text
LearnedHumModel
      |
      v
activateModel(model, sampleOffset)
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

Activating a model first clears the previous cancellation state. If the new Learn result is invalid or reports that no hum was detected, the canceller remains inactive and input passes through unchanged.

Calling `reset()` also clears the active model and returns the canceller to pass-through behavior.

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

Once the buffer is full, `LearnModeController` freezes it for read-only access 
by the background analysis worker. The audio thread does not reuse the buffer 
until the current Learn operation has completed its model handoff.

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

### HumAnalyzer

Coordinates the non-realtime analysis of a completed Learn window.

`HumAnalyzer` first runs `FundamentalFrequencyDetector`. If the analysis is valid and convincing hum is detected, it then runs `HumEstimator` at the detected fundamental frequency and produces a `LearnedHumModel`.

The analyzer is designed to run outside the realtime audio callback.

### LearnModeController

Coordinates Learn capture and the lifetime of the shared analysis buffer.

The current lifecycle is:

```text
Idle
  ↓
Collecting
  ↓
ReadyForAnalysis
  ↓
Analyzing
  ↓
ModelReady
  ↓
Idle
```

Only one Learn operation may be outstanding at a time.

The controller also owns the monotonically increasing audio-sample timeline used to preserve reconstruction phase across asynchronous analysis.

### HumAnalysisWorker

Runs `HumAnalyzer` on a background `juce::Thread`.

The worker polls for a completed analysis window, claims it through `LearnModeController`, performs detection and estimation, and publishes the resulting model through `LearnedHumModelMailbox`.

The worker never activates or modifies the realtime canceller.

### LearnedHumModelMailbox

Provides a fixed-size single-slot handoff between the analysis worker and audio thread.

The worker is the producer and the audio thread is the consumer.

The mailbox uses atomic state transitions rather than locks or dynamic allocation. A result must be consumed before another result can be published.

### Realtime Learn Mode

User Learn requests are submitted through an atomic request flag.

The message thread does not call `LearnModeController::startLearn()` directly. Instead, the next audio block accepts the request and begins capture at a defined block boundary.

When Learn begins:

```text
analysisStartSample =
    current audio sample position
```

Audio continues processing while the 250 ms analysis window is collected and while background analysis runs.

When a completed model is later consumed at the beginning of a host block:

```text
sampleOffset =
    activationSample
    - analysisStartSample
```

The offset includes:

* the analysis window itself
* unused samples in the host block that completed capture
* audio processed while the worker performs analysis
* any additional block-boundary delay before activation

The model is activated only on the audio thread.

### Thread Ownership

The current ownership rules are:

* the audio thread writes the Learn buffer while collecting
* the analysis buffer is frozen after capture completes
* the worker receives read-only access while analyzing
* the worker publishes only a fixed-size learned result
* the audio thread owns cancellation-model activation
* the editor communicates with the processor through thread-safe request/status APIs rather than accessing Learn DSP state directly

### Channel Policy

Realtime Learn and fixed cancellation are currently supported only for mono processing.

`FixedHumCanceller` owns one reconstruction timeline. Calling the same canceller sequentially for multiple channels would advance that timeline more than once per sample frame and break phase alignment.

Stereo cancellation therefore requires an explicit multi-channel design and is intentionally deferred rather than being handled implicitly.

The current user-facing Learn control is disabled for stereo instances.
