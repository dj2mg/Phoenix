# Sample-Rate-Independent DSP Filters

## Problem

Phoenix can run at 192 ksps or 176.4 ksps, selected at run time and persisted.
Most of the DSP chain derives its coefficients from `SR[SampleRate].rate`, but a
handful of stages shipped **frozen coefficient tables** designed offline for one
rate — 24 kHz audio, i.e. 192 ksps decimated by 8. Run at 22.05 kHz instead,
every corner and centre frequency in those tables scaled by
`176400 / 192000 = 0.91875`, so a filter labelled 2.0 kHz actually cut at
1.84 kHz and the "4000 Hz" equaliser cell peaked at 3675 Hz.

The stages affected were the CW audio filters, the CW decoder input filter, the
14-band equaliser (receive and transmit), the AM DC blocker, and the transmit
filter that sets the audio bandwidth.

## Approach

Each affected stage is now generated at run time from an **analog design spec**
rather than carrying a digital coefficient table. The spec is in Hz and does not
depend on the sample rate; the sample rate only enters through the bilinear
transform that discretises it. Prewarping the design frequency
(`wc = 2*Fs*tan(pi*f/Fs)`) cancels the frequency compression the bilinear
transform introduces, so each response lands on its labelled frequency at any
rate.

The filter families are unchanged. What was a 12-pole Chebyshev is still a
12-pole Chebyshev; the equaliser cells still have exactly the shape they had.

`ChangeSampleRate()` calls `InitializeSignalProcessing()`, which calls
`InitializeFilters()` and `InitializeTransmitFilters()`. Both of those now
regenerate their coefficient tables as their first step, so a rate change
retunes the whole chain with no extra plumbing.

## What each stage does now

| Stage | Design | Where |
|---|---|---|
| CW audio filters ×5 | Chebyshev type I, order 12, 0.02 dB ripple. Ripple edges 807.1 / 1038.0 / 1269.0 / 1731.5 / 1963.2 Hz | `CalcChebyshevILowpassCoeffs()`, `DSP_FIR.cpp` |
| Equaliser cells ×14 | 4 stagger-tuned analog bandpass sections per cell, peak normalised to unity | `CalcBandpassCascadeCoeffs()`, `DSP_FIR.cpp` |
| CW decoder input FIR | 64-tap Kaiser windowed sinc, −6 dB at 1749 Hz, DC gain normalised to 1 | `CalcFIRCoeffs()` |
| AM DC blocker | One pole highpass, `pole = exp(-2*pi*38/Fs)` | `InitializeFilters()`, `DSP_FFT.cpp` |
| Transmit audio bandwidth (`FIR_int3`) | 48-tap Kaiser lowpass, −6 dB at 3040 Hz (−3 dB near 2.76 kHz) | `CalcFIRCoeffs()` |
| Transmit decimate-by-2 (`coeffs12K_8K`) | 48-tap Kaiser lowpass, −6 dB at 3500 Hz | `CalcFIRCoeffs()` |

### Where the design constants came from

The Chebyshev order and ripple, and the equaliser prototypes, were recovered
from the frozen tables themselves rather than guessed:

- The CW tables' passband ripple measures 0.0200 dB exactly, and `cheby1(12,
  0.02, Wn)` fits them to 0.013 dB RMSE. The ripple edge — the natural parameter
  for the family — is the highest frequency at which the response is still at or
  above its DC level.
- Every equaliser biquad has `b1 = 0` and `b2 = -b0`, putting its zeros at
  `z = ±1`. That is the bilinear image of the analog bandpass
  `(wn/Q)s / (s^2 + (wn/Q)s + wn^2)`, so an inverse bilinear transform recovers
  the analog `(wn, Q)` of each section exactly.

`code/tools/extract_filter_prototypes.py` does this. It reads
`code/test/reference_filters.cpp` and prints the C literals that live in
`DSP_FIR.cpp`. Re-run it if a reference table ever changes.

## What deliberately was not changed

**The zoom-FFT `mag_coeffs` tables.** Their comments say "sample rate 48k" and
they are applied at the full ADC rate, which looks like a bug. It is not: their
−6 dB points measure at `1.01 × (Fs/2/zoom)` for every zoom index, i.e. they are
specified purely as a fraction of `Fs`. That is exactly right for a decimation
anti-alias filter, and they are correct at any sample rate. (Indices 8–10 are
duplicates of index 7 and genuinely wrong, but they sit beyond
`SPECTRUM_ZOOM_MAX` and are never reached.)

**The transmit decimation and interpolation FIRs** `coeffs192K_10K_LPF_FIR` and
`coeffs48K_8K_LPF_FIR`, and the ±45° Hilbert pair. These are also normalised to
`Fs` — corners at 0.057 and 0.127 `Fs`, and a 90° band spanning 0.0167 to
0.45 `Fs` — so they scale correctly by construction.

The general rule: a filter whose job is anti-aliasing or anti-imaging should
scale with `Fs` and needs no attention. A filter that defines an audio-band
response has to be anchored in Hz.

## One deliberate behaviour change

`coeffs12K_8K_LPF_FIR` feeds a decimate-by-2 stage, so its stopband has to sit
below a quarter of its input rate. The table it replaces was flat to
0.425 `Fs` and −60 dB only at 0.478 `Fs`, giving essentially no protection:
everything between roughly 6 and 9.5 kHz folded back into the transmit audio
unattenuated. The generated filter is a real lowpass, so this aliasing is gone.
Transmitted audio therefore differs from previous releases even at 192 ksps.

`TransmitChain176k.DecimatorStopsBeforeTheFoldPoint` and
`FilterDesign.TransmitDecimatorRejectsAliases` pin the new behaviour and record
the old.

## Validation

`code/test/FilterDesign_test.cpp` (target `all_FilterDesign_tests`) answers two
questions. It measures responses by correlating the filter output against a
complex exponential through a Hann window, which is accurate to well under
0.01 dB — peak picking, as the older sweep tests use, can be a decibel out when
there are few samples per cycle.

**Does the generated filter still do what the frozen table did?** Every response
is measured against the original coefficients, kept verbatim in
`code/test/reference_filters.cpp`, at 192 ksps:

| Stage | Agreement with the frozen table |
|---|---|
| CW audio filters | within 0.05 dB |
| Equaliser cells | within 0.01 dB |
| CW decode FIR | −6 dB corner within 2 % |
| Transmit audio bandwidth | −3 dB corner within 2 % |
| Flat equaliser sum | within 0.05 dB |

**Do the filters hold their frequencies across a rate change?**
`FilterDesign.GeneratedCornersHoldAcrossRates` writes
`filtercmp_rate_summary.txt`. Every ratio now reads 1.00 where it used to read
0.9187:

```
CW_audio_2000_minus6dB,fs_derived,2064.4,2063.5,0.9996,1.0000
EQ_band13_4000_peak,fs_derived,4000.0,4000.0,1.0000,1.0000
TX_audio_bandwidth_minus3dB,fs_derived,2776.1,2797.5,1.0077,1.0000
CW_decode_FIR_minus6dB,fs_derived,1748.0,1748.1,1.0000,1.0000
```

`FilterDesign.FlatEqualiserSumStaysFlat` covers the property most at risk here.
`BandEQ` sums the 14 cells with alternating sign, a reconstruction that only
comes out level if every cell keeps its shape and peak gain. It is checked
through the real `BandEQ` path against the reference tables. Note the flat sum
sits at roughly +4 dB, not 0 dB — that is how the equaliser has always behaved.

Also updated:
- `ReceiveChain176k.AllStagesHoldTheirFrequencies` (was
  `HardCodedStagesShiftFsDerivedStay`) — these assertions used to encode the
  shift and now assert its absence.
- `TransmitChain176k` — new suite, the transmit counterpart.
- `compare_176k_vs_192k.ipynb` — overlays the generated and frozen sweeps.

### Running it

```bash
cd code/test/build && cmake ../ && make -j && ctest --output-on-failure
ctest -R "FilterDesign|ReceiveChain176k|TransmitChain176k" -V
```
