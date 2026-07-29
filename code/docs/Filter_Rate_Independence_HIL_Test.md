# Measuring filter rate-independence on the bench

Companion to `Sample_Rate_Independence_Plan.md`. That document describes how the
receive filters are generated; this one describes how we confirm it on the
actual radio, and why each measurement proves what it claims to.

The tool is `code/tools/filter_hil/`. Its README covers wiring and operation;
this is the reasoning behind it.

## The problem with measuring a filter through a whole radio

The only accessible output is the speaker, which sits at the end of everything:
the codec's analog front end, the decimation chain, the SSB convolution mask,
the demodulator, the equaliser, the volume control, the DAC and an audio power
amplifier. A raw amplitude sweep of that path characterises all of it at once,
and none of it individually. Worse, several of those stages have deliberately
rate-dependent behaviour — the decimation filters *should* scale with Fs — so a
naive sweep at two sample rates would differ for entirely legitimate reasons.

The fix is to never look at an absolute level. Every response is measured as the
difference between two captures **at the same injected frequency**, one with the
filter under test engaged and one with it out of circuit:

| Filter | Engaged | Reference |
|---|---|---|
| CW audio filters | `CF0`..`CF4` | `CF5` (bypassed) |
| Equaliser cells | one cell at 100, rest at 0 | all cells at 100 |
| SSB convolution filter | `FW` at the width under test | `FW` at its widest |
| AM DC blocker | envelope at `f_mod` | envelope at 300 Hz |

Everything common to the two captures divides out exactly. What is left is the
filter's own magnitude response, measured through the real signal path, with no
model of the rest of the radio required.

This works because of where these filters sit. `CWAudioFilter` is a plain series
stage that becomes a no-op at index 5 (`DSP.cpp:944`, `DSP_CWProcessing.cpp:538`),
so bypassing it removes exactly one thing. The equaliser cells are summed in
parallel with alternating signs (`DSP_FFT.cpp:780-830`), so zeroing thirteen of
them leaves precisely one.

## Getting a signal in

W1 and W2 drive the I and Q receive inputs directly, at baseband, bypassing the
RF front end. `ReceiveProcessing` then shifts **twice** before decimating: by
Fs/4 (`FreqShiftFs4`, `DSP.cpp:883`) and again by the fine tune plus any CW
sidetone (`FreqShiftF`, `DSP.cpp:895-903`). So the injection that demodulates to
zero audio sits at

```
centre   = |Fs/4 + fineTuneFreq_Hz|
f_inject = centre +/- (f_audio + sidetone_shift) + mapping_correction
```

Worked example, measured on the bench with the radio's fine tune at -12250 Hz:

| Sample rate | Fs/4 | centre | inject for 1 kHz of audio |
|---|---|---|---|
| 192 ksps | 48000 | 35750 | 36750 |
| 176.4 ksps | 44100 | 31850 | 32850 |

**Recomputing Fs/4 per rate is the point of the whole exercise**, and a test that
injected the same frequency at both rates would report nonsense.

**Forgetting the fine tune is worse: it produces silence.** It is whatever the
operator last tuned to and is routinely several kilohertz - far more than the
3 kHz passband - so every injection lands outside the filter and the radio never
responds, at any drive level. The visible symptom is that the injected tone
appears on the radio's spectrum display nowhere near the blue fine-tune bar.

In CW receive the sidetone offset (`DSP.cpp:894-901`, default 750 Hz) adds to the
same shift, which is why the CW tests re-run the mapping calibration after
entering CW mode - and bypass the CW filter first, so the calibration probes are
not measured through the filter under test.

The `map` test also reports how far the real centre sits from nominal. On this
radio it is about **-180 ppm at both rates**, which is the Teensy synthesising
its I2S clock with a fractional divider rather than any error; the residual is
folded into `mapping_correction` and the check is deliberately loose enough
(150 Hz) not to trip on it while still catching a rate that was never
reconfigured, which would be kilohertz out.

Test `map` injects three tones and fits `f_audio = slope * f_inject + offset`.
If that fit disagrees with the expected centre, the firmware failed to
reconfigure its frequency shift for the rate and nothing measured afterwards
would be interpretable — so that rate is skipped rather than reported.

## Quadrature, and which wire is which

W1 and W2 must be phase-locked and started atomically: `FDwfAnalogOutMasterSet`
to share a timebase, then a single `FDwfAnalogOutConfigure(hdwf, -1, 1)`.

Note the `1`. In the WaveForms API **`fStart = 3` means "apply settings to a
channel that is already running"**, and passing it to a stopped channel silently
leaves it stopped — the device reports the frequency and amplitude it was given
while generating nothing at all. When an injection appears to do nothing, read
`FDwfAnalogOutStatus` back and check it says `Running` rather than `Ready`.

Two unknowns are resolved by experiment rather than by modelling the chain,
because getting either wrong produces silence rather than a subtly wrong answer:

- **Which side of the demodulation centre to inject.** This is the one that has
  to be unambiguous, and it is: the SSB convolution filter's entire job is to
  reject the other sideband, and it does so by around 40 dB on this rig. The
  suite gates on that number.
- **Which AWG channel drives I.** Much weaker, and deliberately not gated on. An
  amplitude imbalance between W1 and W2 at the radio's inputs is normal and
  leaves an image at the mirror frequency, but the SSB filter rejects that image
  anyway, so both phase senses give a usable tone and only the amplitude differs
  — a few decibels in practice. The suite picks the better one and reports the
  margin.

Nothing needs rewiring either way.

## Why AGC must be off

AGC compresses exactly the amplitude differences being measured. With it engaged,
a filter skirt reads as a straight line and every corner measurement fails to
bracket. There is no CAT command for AGC, so the suite reads it from the `ED;`
dump on the diagnostic port and refuses to run when it is on. There is a second
guard downstream: if the reference sweep's dynamic range across the passband is
implausibly small, that is reported too.

## Pass criteria

**Primary: rate invariance.** Each measured frequency must land in the same place
at both sample rates, within 1.5 %. The frozen-table bug produces exactly
`176400/192000 - 1 = -8.125 %`, so the tolerance sits a factor of five inside the
failure it is looking for. The comparison also flags when a measured shift
matches that signature, and says so in the failure message.

**Secondary: absolute accuracy**, within 4 % of the labelled frequency. Looser on
purpose. A smooth amplitude tilt anywhere in the analog path — the AWG, the
codec, the speaker amplifier — biases absolute measurements without saying
anything about the firmware. The rate comparison is immune to it, because the
same tilt applies at both rates and divides out.

**Shape checks.** A corner in the right place is not proof of a working filter:
a broken cascade can still cross -3 dB somewhere plausible. So CW filters are
also checked for passband ripple under 1 dB and stopband beyond -25 dB, and
equaliser cells for Q within 25 % of the design value.

## The control

The SSB convolution filter derives its coefficients from the true sample rate
and always has — `InitFilterMask` divides by `SR[SampleRate].rate / DF`
(`DSP_FIR.cpp`). It was never affected by the bug. Measuring it is therefore a
check on the *method*: if it shows an 8 % shift, the rig or the analysis is
wrong, not the firmware. The report says this explicitly so nobody misreads a
control failure as a firmware regression.

## What the rig can actually resolve

Measured on this bench, driving 0.89 V into the I/Q inputs with the AF volume at
30 %:

- The recovered tone sits **15-20 dB above the receiver's own noise**. That noise
  is the limit, not the scope: it passes through the same DSP chain as the
  signal, so more volume does not help and more drive is the only lever.
- Consequently a response can be followed about **5 dB below the passband**
  before the tone disappears into the noise. That is why the SSB filter edge is
  called at -3 dB rather than the more usual -6 dB, and why corner searches
  record NaN probes rather than pretending to have measured something.
- The AF volume control is steep: 54 % clipped a 5 V scope range outright while
  30 % gives about 30 mV of noise. The suite sets it and restores it.
- Reported THD tracks that noise floor rather than any real distortion, so it is
  recorded but does not invalidate a measurement; auto-levelling is the one place
  it is used as a limit.

## Known measurement limits

**Equaliser cells 0, 1 and 13.** Cells 0 (198.425 Hz) and 1 (250 Hz) sit at or
below the SSB filter's 200 Hz low cut, so their measured shape is the product of
the cell and the skirt. Cell 13 (4000 Hz) sits in the decimation filter's skirt,
whose corner is a fraction of Fs and is *meant* to move with the rate. All three
are flagged `edge_limited` and judged loosely on absolute accuracy. Their
rate-invariance check is still applied at full strength, because the same skirt
applies at both rates and cannot hide a shift. The `FL` CAT command can move the
low cut out of the way if cells 0 and 1 need a cleaner measurement.

**The AM DC blocker** is only reachable because `MD4;` was added to select AM;
`MD5;` selects SAM, which routes to a different demodulator without the blocker.
It is off by default because it is slower and noisier than the rest — envelope
measurements at 10-40 Hz need long captures.

**Scope sample rate.** The AD2's record mode drops samples above roughly
250 kHz. The default of 96 kHz is far below that; every capture records its lost
and corrupted counts, and any point with drops is excluded from fits rather than
silently averaged in.

## CAT additions this required

Most of the radio state the suite drives was previously touchscreen-only. Four
commands were added and three existing ones fixed:

| Command | Form | Purpose |
|---|---|---|
| `SR` | `SRn;` / `SR;` | Sample rate, 0 = 176.4 k, 1 = 192 k |
| `CF` | `CFn;` / `CF;` | CW filter index 0-5 |
| `EQ` | `EQbbvvv;` / `EQbb;` | Equaliser cell `bb` = 00-13, level 000-100 |
| `FL` | `FL####;` / `FL;` | Filter low cut, the mirror of `FW` |

Fixes:

- `MD_write` now sets `ED.modulation[]` as well as `bands[].mode`. Setting only
  the latter left `InitFilterMask` treating the difference as a deliberate
  departure from the band default and **mirroring the passband** — so `MD` did
  not just fail to change the demodulator, it inverted the receive filter.
- `MD1`/`MD2` now dispatch `TO_SSB_MODE` when in CW receive. Previously that
  transition existed only on the front-panel button, so CAT could enter CW and
  never leave.
- `FW`'s `read_len` no longer equals its `set_len`. `command_parser` tests the
  write form first, so the two being equal made `FW;` unreachable and the filter
  bandwidth write-only.

Note the parser rule for anyone adding more: **`set_len` and `read_len` must
differ**, or the read function can never be called.

## Regenerating figures

The JSON keeps every raw sweep point, so figures and the Markdown report can be
rebuilt from a stored result without the hardware:

```bash
cd code/tools
./venv/bin/python filter_hil/plot_filter_hil.py filter_hil/results/<run>.json
```

## Trusting the tool

`code/tools/filter_hil/test_filter_hil.py` covers the measurement maths against
synthetic signals: amplitude accurate to 0.02 dB on and off bin, corner finding
on analytic filters, Q extraction, and the `ED;` parser against a captured
fixture including interleaved debug output.

The test that matters most feeds the comparison a synthetic **-8.125 % shift** and
asserts that every CW and equaliser check fails and is identified as the known
frozen-table signature. Without it, a suite that quietly passed everything would
look identical to a working radio.
