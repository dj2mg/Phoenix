# Receive DSP Chain — Step-by-Step Parameter Sourcing

Analysis of the receive audio processing chain: each step, its location in the
code, and whether its DSP parameters are **derived from the sample rate (Fs)** or
**hard-coded** (baked-in coefficient tables with an implicit Fs assumption).

## Global context

- Input rate = `SR[SampleRate].rate`, default **192 kHz** (`Globals.cpp:106`).
- Decimation factor `DF = DF1*DF2 = 4*2 = 8`, a compile-time constant (`SDT.h:409-410`).
- So the audio-rate back half of the chain (ConvolutionFilter onward) runs at
  **Fs/8 = 24 ksps** when Fs = 192 kHz.
- Pipeline defined in `ReceiveProcessing()`, `DSP.cpp:839`.

## Chain steps

- **Read IQ input** — `ReadIQInputBuffer`, `DSP.cpp:133`
  - Sets `data->sampleRate_Hz = SR[SampleRate].rate`. **Fs-derived** (source of truth).

- **RF gain** — `ApplyRFGain`, `DSP.cpp:115`
  - Pure dB→linear scaling; no frequency parameters. N/A.

- **IQ amplitude/phase correction** — `ApplyIQCorrection`, `DSP.cpp:206`
  - Per-band calibration factors; no frequency parameters. N/A.

- **Zoom FFT (spectrum display)** — `ZoomFFTExe`, `DSP_FFT.cpp:574`
  - Decimation biquads `mag_coeffs[]`, `DSP_FIR.cpp:690`. **Hard-coded.**
  - Elliptic biquads designed for **Fs = 48 kHz**, 60 dB stopband. Corners:
    2×=12 kHz, 4×=6 kHz, 8×=3 kHz, 16×=1.5 kHz, 32×=750 Hz.
  - Note: runs on full-rate (192 kHz) data before DecimateBy8, so at the default
    input rate the effective corners land 4× higher than the labels (design-rate
    vs runtime-rate mismatch).

- **Frequency translation +Fs/4** — `FreqShiftFs4`, `DSP.cpp:879`
  - Multiplication-free ±Fs/4 shift. **Fs-derived** (shift = Fs/4 by construction).

- **Fine-tune frequency shift** — `FreqShiftF`, `DSP.cpp:899`
  - NCO increment `= 2π·shift/sampleRate_Hz`. **Fs-derived.**

- **Decimate by 8 (÷4 then ÷2)** — `DecimateBy8`, `DSP_FFT.cpp:662`
  - FIR coeffs via `InitializeDecimationFilter` / `CalcFIRCoeffs`,
    `DSP_FFT.cpp:365`, `DSP_FIR.cpp:1284`. **Fs-derived.**
  - Normalized cutoff = `n_desired_BW_Hz/Fs`, tap count from `n_att_dB`.
  - Targets hard-fixed: `n_desired_BW_Hz = 9000`, `n_att_dB = 90` (`SDT.h:412-413`),
    but normalization scales with Fs.

- **Volume scale (bandwidth compensation)** — `VolumeScale`, `DSP.cpp:219`
  - `volScaleFactor = 7.0874·Fcut_kHz^-1.232`. **Hard-coded** empirical curve fit
    (keyed off the band filter cutoff in kHz, not Fs).

- **Convolution bandpass filter** — `ConvolutionFilter`, `DSP_FFT.cpp:678`
  - Filter mask via `CalcCplxFIRCoeffs`, `DSP_FIR.cpp:1357`. **Fs-derived.**
  - Passband edges from per-band `FLoCut_Hz`/`FHiCut_Hz` (default 200–3000 Hz,
    `Globals.cpp:39+`), normalized by `Fs/DF`. Data-driven edges, adaptive Fs.

- **AGC** — `AGC` / `InitializeAGC`, `DSP.cpp:313` / `DSP.cpp:238`
  - All attack/decay/hang multipliers = `f(sample_rate = Fs/DF)`. **Fs-derived.**
  - Time constants (tau) and hangtimes are hard-coded seconds, converted to
    per-sample multipliers using Fs.

- **Demodulate** — `Demodulate`, `DSP.cpp:629`
  - SSB (LSB/USB): copy only, no parameters. N/A.
  - AM: DC-blocker `w = audiotmp + wold*0.99` (`DSP.cpp:654`) is **hard-coded**
    (pole 0.99 ⇒ ~38 Hz corner at 24 ksps; comment says "below 200 Hz").
    Followed by AM audio lowpass biquad `biquad_lowpass1` (`DSP_FFT.cpp:358`):
    **Fs-derived** (`SetIIRCoeffs`, corner = band FHiCut, Q=1.3 hard-coded, Fs/DF).
  - SAM: `AMDecodeSAM`, `DSP.cpp:567` — PLL gains all from `sampleRate_Hz`
    (**Fs-derived**); PLL physical constants (`pll_fmax=4000`, `omegaN=200`,
    `zeta=0.65`, `tauR=0.02`, `tauI=1.4`, `DSP.cpp:551-556`) are hard-coded.

- **Receive band EQ (14-band graphic)** — `BandEQ`, `DSP_FFT.cpp:819`
  - `EQ_Band1..14Coeffs`, `DSP_FIR.cpp` (~line 400+). **Hard-coded.**
  - 4-pole Gaussian ⅓-octave biquads; centers 400/500/630/793/1000/1259/1587/
    2000/2500/3150/4000 Hz. Implied design rate **24 ksps** (post-decimation).

- **Noise reduction** — `NoiseReduction`, `DSP.cpp:672`
  - Kim / Spectral / LMS algorithms; internal constants are algorithm tuning
    parameters (not Fs-normalized). Mostly hard-coded, block-size dependent.

- **Notch filter (LMS)** — `Xanr`, called at `DSP.cpp:933`
  - Adaptive LMS; coefficients learned at runtime. Neither fixed table nor
    Fs-normalized (adapts to signal, implicitly block/rate dependent).

- **CW decode** — `DoCWReceiveProcessing`, `DSP_CWProcessing.cpp:86`
  - Decode FIR `CW_Filter_Coeffs2[64]`, `DSP_FIR.cpp:95`. **Hard-coded** — 64-tap
    Parks-McClellan, **24 ksps**, Fc = 1560 Hz.
  - Reference sine / Goertzel: `DSP_CWProcessing.cpp:65, 106` — **Fs-derived**
    (`phs = 2π·f_tone/(Fs/DF)`; Goertzel uses `data->sampleRate_Hz`).

- **CW audio bandpass** — `CWAudioFilter`, `DSP_CWProcessing.cpp:538`
  - `CW_AudioFilterCoeffs1..5`, `DSP_FIR.cpp:31-81`. **Hard-coded.**
  - 12-pole Chebyshev LPFs, **24 KSPS**; cutoffs 840 Hz / 1.08 / 1.32 / 1.80 /
    2.0 kHz (UI: 0.8/1.0/1.3/1.8/2.0 kHz).

- **Interpolate back to Fs** — `InterpolateReceiveData`, `DSP.cpp:697`
  - Interpolation FIRs `FIR_int1`/`FIR_int2` via `CalcFIRCoeffs`,
    `DSP_FFT.cpp:394-397`. **Fs-derived** (computed at `Fs/DF1` and `Fs`).

- **Volume adjust (audio level)** — `AdjustVolume`, `DSP.cpp:721`
  - `VolumeToAmplification` — pure user-level gain curve; no frequency
    parameters. N/A.

## Summary

- **Fs-derived (follow sample-rate changes correctly):** decimation/interpolation
  filters, convolution bandpass mask, AM audio lowpass, AGC, SAM PLL, all
  frequency shifts, CW reference sine / Goertzel.
- **Hard-coded coefficient tables (implicit Fs assumption):**
  - Zoom-FFT `mag_coeffs` — design **Fs = 48 kHz**.
  - CW audio filters, CW decode FIR, 14-band EQ, AM DC-blocker — design **Fs = 24 ksps**
    (i.e. correct only for 192 kHz input).
- **Empirical / algorithm-tuned (not Fs-normalized):** VolumeScale curve, SAM PLL
  physical constants, noise-reduction internals.
- The audible passband edges are **data-driven** from the band table
  (`FLoCut_Hz`/`FHiCut_Hz`, default 200–3000 Hz), feeding the *computed* convolution
  mask — not baked into any coefficient table.
