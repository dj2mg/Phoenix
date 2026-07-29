---
title: Filter Hardware-in-the-Loop Test
type: decision
status: draft
created: 2026-07-29
updated: 2026-07-29
tags: [test, verification, hil, analog-discovery, filters, sample-rate, cat, agc]
source_refs: []
related: ["[[runtime-filter-design]]", "[[sample-rate-switching]]", "[[cat-control]]", "[[dsp-chain]]", "[[agc-design]]", "[[tune-frequency-control]]", "[[iq-quadrature-sampling]]"]
---

# Filter Hardware-in-the-Loop Test

`code/tools/filter_hil/` — a Python suite that measures the receive DSP filters **on the actual
radio** and checks they land on their labelled frequencies at both sample rates. It exists
because [[runtime-filter-design]] was otherwise only verified in simulation.

Full operating instructions live in `code/tools/filter_hil/README.md`. This page records the
parts worth knowing without reading it: the method, the two traps, and the result.

## Rig

An Analog Discovery 2 drives `W1`/`W2` into the radio's I/Q receive inputs as a quadrature pair
and reads the demodulated audio back off the speaker with `Ch1`. CAT on `/dev/ttyACM1`,
diagnostics on `/dev/ttyACM0`. W1/W2 order does not matter — the suite tries both quadrature
senses and keeps whichever the SSB filter passes.

## The method: differential measurement

Every response is the **difference between two captures at the same injected frequency**, one
with the filter engaged and one with it bypassed:

| Filter | Measured as |
|---|---|
| CW audio filters | `level(CF=k) − level(CF=5)` |
| Equaliser cells | one cell at 100, the rest at 0, against the flat reference |
| SSB convolution filter | `level(FW=x) − level(FW=widest)` |
| AM DC blocker | envelope level at `f_mod`, against 300 Hz |

Everything the two captures have in common cancels exactly — AWG amplitude flatness, the codec
front end, the decimation rolloff, the SSB mask, the AF volume setting, the DAC, the speaker
amplifier. **That is what makes an absolute-level measurement taken off a speaker terminal good
enough to characterise a filter.**

The **SSB filter is the control**: it always derived its coefficients from the true sample rate,
so it was never subject to the bug. If it shifts, the rig or the analysis is wrong, not the
firmware.

## Trap 1: the injection frequency

⚠️ The single most expensive thing to get wrong. `ReceiveProcessing` shifts **twice** before
decimating — by Fs/4 (`FreqShiftFs4`) and then by the fine tune plus any CW sidetone
(`FreqShiftF`). The tone that demodulates to DC therefore sits at:

```
centre = |Fs/4 + fineTuneFreq_Hz|
```

and a wanted audio frequency `f` is at `centre ± f`. Worked example from the bench, fine tune
−12250 Hz:

| Fs | Fs/4 | fine tune | centre | inject for 1 kHz |
|---|---|---|---|---|
| 176.4 ksps | 44100 | −12250 | 31850 | 32850 |
| 192 ksps | 48000 | −12250 | 35750 | 36750 |

`fineTuneFreq_Hz` is whatever the operator last tuned to and is routinely **several kilohertz** —
far wider than any passband being measured. Omitting it produces **silence at any drive level**,
which is indistinguishable from a dead rig. See [[tune-frequency-control]] for the shift
arithmetic.

The suite does not assume the relationship; test `map` **measures** it. That test also reports
how far the real demodulation centre sits from nominal — about **−180 ppm** on this radio, which
is the Teensy's fractional I²S clock, not an error.

## Trap 2: AGC must be off

⚠️ [[agc-design]]'s look-ahead AGC compresses exactly the amplitude differences the suite
measures, so every filter skirt reads flat. Worse, on a quiet band it runs the gain up and
buries the injected tone 25–30 dB under the hiss — which presents as `iq_order` reporting
negative SNR for *all four* quadrature combinations, and a couple of hundred mV RMS on the
speaker with the AWG silent.

There is **no CAT command for AGC**; it must be set from the touchscreen menu. The suite
refuses to run otherwise. (Diagnostic: if `PD;` shows the tone in the radio's own spectrum but
the audio does not, the DSP is receiving it and the problem is downstream gain.)

## The pass criterion

**Rate invariance, not absolute accuracy.** A corner measured at 192 ksps must land at the same
frequency at 176.4 ksps. The bug moves it by **−8.125 %**; the tolerance is **1.5 %** — a factor
of five between correct and broken. Absolute accuracy against the labelled frequency is checked
more loosely, because a smooth tilt anywhere in the analog path biases it without saying
anything about the firmware.

Some equaliser cells are marked *edge limited*, where the SSB filter rather than the cell shapes
what was measured: cells 0 and 1 (198, 250 Hz) sit at or below the 200 Hz low cut, and cell 13
(4000 Hz) sits in the decimation skirt — whose corner is a fraction of Fs and is *meant* to move
with it. Their absolute tolerance is relaxed; their rate invariance is not.

## Result

Measured on the bench: **95 checks passed, 0 failed.** Every CW corner and equaliser centre holds
to within **0.3 %** across the rate change where the frozen tables moved them by 8.125 %. The SSB
control reads **0.04 %**.

## The suite tests itself

`test_filter_hil.py` covers the measurement maths without hardware, **including a check that the
comparison fails when handed a simulated −8.125 % shift**. Without that, a suite that quietly
passed everything would look identical to a working radio.

## Coupling to the firmware

- It needed CAT access to settings that were previously touchscreen-only — which is why the
  `SR`, `CF`, `EQ` and `FL` commands were added ([[cat-control]]).
- ⚠️ `bandtable.py` **mirrors design constants from `DSP_FIR.cpp` and `Globals.cpp` by hand**.
  Nothing detects a firmware value edited without updating it. Re-check that file whenever the
  filter design constants change.
- Do not touch the front panel while it runs — menu interaction triggers a settings save, which
  would make the test configuration permanent in [[persistent-config]].

## Open questions
- AGC-off is enforced by refusal rather than by control. A CAT command for AGC mode would let
  the suite set and restore it like everything else it touches.
- The −180 ppm I²S clock offset is measured and tolerated; it has not been checked against the
  Teensy's spec or across temperature.
