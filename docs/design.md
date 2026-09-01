# Humbug Design Notes

## Processing Architecture

The current realtime processor path is:

```text
Input
  |
  +----> Manual Learn capture when requested
  |
  +----> Periodic tracking capture when active
  |
  v
Adaptive hum cancellation when active
  |
  v
Output gain
  |
  v
Output
```

Manual Learn and adaptive tracking analysis are deliberately separated from realtime cancellation.

```text
                         MANUAL LEARN
                              |
raw input --------------------+
  |                           |
  |                           v
  |                    LearnModeController
  |                           |
  |                           v
  |                    HumAnalysisWorker
  |                           |
  |                           v
  |                       HumAnalyzer
  |                           |
  |                           v
  |                 LearnedHumModelMailbox
  |                           |
  |                           |
  |                    AUDIO THREAD
  |                           |
  |                           v
  |                 authoritative model
  |                           |
  |                    starts / updates
  |                      tracking prior
  |                           |
  +---------------------------+----------------------+
  |                                                  |
  v                                                  v
HumTrackingController                         AdaptiveHumCanceller
  |                                                  |
  | periodic 250 ms capture                          |
  v                                                  |
HumTrackingWorker                                    |
  |                                                  |
  v                                                  |
HumAnalyzer::analyzeNear()                           |
  |                                                  |
  v                                                  |
LearnedHumModelMailbox                               |
(tracking)                                           |
  |                                                  |
  +-------------------> AUDIO THREAD ----------------+
                        model transition
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

For automatic adaptive tracking, the detector also supports a narrow search through `detectNear()`.

Once manual Learn has established a reliable fundamental frequency, repeating the complete 48–52 Hz and 58–62 Hz search for every tracking update is unnecessarily expensive. Tracking therefore searches within a small region around the most recently accepted model: `reference frequency ± 0.3 Hz`.

The narrow search uses the same 0.1 Hz coarse grid, residual-energy scoring, parabolic refinement, and hum-classification logic as the full search.

Manual Learn continues to use the complete mains-frequency search because it cannot assume a prior frequency.

Refinement eligibility is determined from the winning coarse-grid index rather than floating-point comparisons against the search boundaries. This avoids rounding errors near a boundary incorrectly preventing parabolic refinement.

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

Provides the original fixed-model cancellation implementation and remains
useful as a regression baseline for measuring the benefit of adaptive tracking.

It is no longer the primary cancellation stage used by `PluginProcessor`.
Production realtime cancellation now uses `AdaptiveHumCanceller`.

`FixedHumCanceller` consumes one already-learned `LearnedHumModel`, initializes
a `HumReconstructor` at the requested continuation offset, and continues
subtracting that unchanged model until another model is explicitly activated
or the canceller is reset.

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

### AdaptiveHumCanceller

Owns the current production realtime cancellation path.

`AdaptiveHumCanceller` maintains two `HumReconstructor` instances:

- one reconstructing the currently active model
- one used to prepare the next accepted tracking model

Manual Learn activates a model immediately through `activateModel()`. Automatic
tracking updates use `transitionToModel()` instead.

When an automatic update is accepted, the new model is phase-continued to the
current audio timeline and the estimated hum waveform is crossfaded from the
old reconstructor to the new reconstructor over a short transition period.

The current transition duration is: `20 ms`

Conceptually:

```text
old model reconstruction ----\
                              >---- crossfaded estimate ----> subtract
new model reconstruction ----/
```

Crossfading reconstructed waveforms avoids directly interpolating wrapped
phase values. At the end of the transition, the new reconstructor becomes the
active one and continues from its existing oscillator state.

Invalid or no-hum automatic updates are rejected without disturbing the
currently active model.

Manual Learn remains authoritative: activating a manual model replaces the
current adaptive state directly rather than crossfading from an older
automatically tracked model.

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
* expensive manual Learn or adaptive tracking analysis directly inside the realtime audio callback

Fixed-size arrays are preferred for the known maximum harmonic count.

Frequency detection and least-squares model fitting are substantially more
expensive than sample-by-sample reconstruction and subtraction and are
therefore performed exclusively by background analysis workers.

The realtime audio callback is responsible only for:

- raw analysis-window capture into preallocated storage
- consuming completed fixed-size model results at block boundaries
- deciding whether automatic results are current and acceptable
- activating or transitioning realtime reconstruction models
- sample-by-sample hum subtraction

Neither manual Learn nor automatic tracking performs model fitting directly on
the audio thread.

## Parameters and State

`AudioProcessorValueTreeState` is the central source of truth for host-visible
parameters, editor controls, and serialized plugin state.

Synthetic hum injection is disabled by default and is currently exposed only
as a development/testing mechanism.

### HumAnalyzer

Coordinates non-realtime analysis of completed manual Learn and adaptive
tracking windows.

For initial Learn, `analyze()` runs the full `FundamentalFrequencyDetector`
search across the expected 50 Hz and 60 Hz mains regions.

For automatic tracking, `analyzeNear()` instead receives a reference
fundamental frequency and searches within a narrow range around that value.

After frequency detection, both paths use the same model-building process. If
the detection result is valid and convincing hum is present, `HumEstimator`
fits the harmonic amplitudes and phases at the detected fundamental and
produces a `LearnedHumModel`.

The analyzer is designed exclusively for background-thread use and does not
run inside the realtime audio callback.

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

`PluginProcessor` currently owns separate mailbox instances for manual Learn results and automatic tracking results so the two asynchronous analysis paths can remain independent.

### HumTrackingController

Coordinates recurring raw-input capture for automatic adaptive tracking.

Tracking is inactive until a valid manual Learn establishes the first
authoritative hum model.

The current lifecycle is:

```text
Inactive
   ↓
Waiting
   ↓
Collecting
   ↓
ReadyForAnalysis
   ↓
Analyzing
   ↓
ModelReady
   ↓
Waiting
```

The nominal tracking cadence is one new capture start per second.

Only one tracking operation may be outstanding at a time. If analysis or model
handoff extends beyond the next nominal capture time, no overlapping capture is
started. Once the outstanding operation is handed off, an overdue capture may
begin at the next eligible host-block boundary.

Each capture reuses the same 250 ms LearnBuffer design used by manual Learn.

The controller also stores the reference fundamental used for the next
tracking search. When a capture begins, that frequency is snapshotted for the
lifetime of the analysis window. A later manual Learn or accepted tracking
model may change the next reference frequency without changing the prior used
by an already-captured window.

Tracking can be requested to stop while work is outstanding. If the controller
is merely waiting, it becomes inactive immediately. If capture, analysis, or
model handoff is already in progress, that operation is allowed to drain before
the controller transitions to `Inactive`.

### HumTrackingWorker

Runs periodic tracking analysis on a dedicated background `juce::Thread`.

When `HumTrackingController` exposes a completed window, the worker claims the
frozen buffer and reads the reference frequency that was snapshotted when that
capture began.

Unlike initial Learn analysis, tracking uses:

```text
HumAnalyzer::analyzeNear(
    referenceFrequencyHz,
    ±0.3 Hz
)
```

This avoids repeating the complete mains-frequency search for every update.

The resulting `PendingLearnResult` contains both the learned model and the
absolute sample position at which the tracking analysis window began. It is
published through a dedicated `LearnedHumModelMailbox`.

The worker never activates or transitions the realtime canceller directly.
Model acceptance remains owned by the audio thread.

### Manual Learn and Adaptive Tracking Interaction

Manual Learn is authoritative over automatic tracking.

When a valid manual Learn model is consumed:

1. The model is activated immediately in `AdaptiveHumCanceller`.
2. Its fundamental becomes the reference frequency for future tracking.
3. Automatic tracking is started or kept active.
4. The manual activation sample becomes the minimum acceptable start time for
   future tracking results.

A tracking result is accepted only when its analysis window began at or after
the most recent authoritative manual activation. This prevents an older
tracking operation from completing late and overwriting a newer manual Learn.

When a manual Learn completes without detecting hum:

1. Adaptive cancellation becomes inactive.
2. Automatic tracking is requested to stop.
3. Any already-outstanding tracking work is allowed to drain safely.
4. Its result is ignored rather than allowing stale cancellation to return.

Automatic tracking updates are advisory rather than authoritative. Invalid,
no-hum, stale, or otherwise unaccepted tracking models leave the currently
active cancellation model unchanged.

When a valid tracking model is successfully transitioned into the realtime
canceller, its fundamental becomes the reference frequency used for the next
tracking capture.

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

The manual model is activated in `AdaptiveHumCanceller` only on the audio
thread. A valid manual Learn also establishes the reference frequency used by
subsequent automatic tracking.

### Thread Ownership

The current ownership rules are:

- the audio thread writes manual Learn and tracking buffers while collecting
- completed analysis buffers are frozen before background workers receive them
- each worker receives read-only access to its claimed analysis window
- workers publish only fixed-size `PendingLearnResult` values through their
  mailboxes
- the audio thread owns all cancellation-model activation and transition
- the audio thread owns decisions about whether tracking results are stale or
  otherwise acceptable
- tracking reference frequencies are updated by the audio thread and
  snapshotted when a tracking capture begins
- the editor communicates with the processor through thread-safe request and
  status APIs rather than accessing analysis or cancellation state directly

### Channel Policy

Realtime Learn, adaptive tracking, and cancellation are currently supported
only for mono processing.

The current adaptive cancellation architecture maintains a single
sample-by-sample reconstruction timeline. Processing multiple channels
sequentially through the same reconstruction state would advance oscillator
phase more than once per sample frame and break alignment.

Manual Learn and automatic tracking also currently analyze one channel only.

Stereo adaptive cancellation therefore requires an explicit multi-channel
design and remains intentionally deferred rather than being handled
implicitly.

The current user-facing Learn control is disabled for stereo instances.
