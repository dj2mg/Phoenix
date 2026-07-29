---
title: "Dev: Bi-directional USB Audio (#13)"
type: roadmap
status: draft
created: 2026-06-09
updated: 2026-07-29
tags: [development, feature, usb, audio, digital-modes, openaudio, sample-rate]
source_refs: []
related: ["[[development-backlog]]", "[[dsp-chain]]", "[[openaudio-library]]", "[[cat-control]]", "[[sample-rate-switching]]"]
---

# Dev: Bi-directional USB Audio (#13)

**GitHub issue:** [KI3P/Phoenix#13](https://github.com/KI3P/Phoenix/issues/13) — *"Feature
request: bi-directional USB audio"*

> Goal (from the issue): add the ability to **send and receive an audio stream over USB** — so a
> PC can pipe RX audio in and TX audio out (e.g. for digital modes / WSJT-X), complementing the
> existing CAT control ([[cat-control]]).
> Ref: <https://groups.io/g/SoftwareControlledHamRadio/topic/116487848#msg35710>

Scoping note — not a solution design.

## Current state — in active debugging, not yet a feature

- The audio codec interface is `MainBoard_AudioIO` ([[dsp-chain]]); a grep finds **no USB-audio
  endpoints** (`AudioInputUSB`/`AudioOutputUSB`) wired in there yet.
- There is substantial **investigation already underway** — several debug docs sit in the
  working tree (`code/docs/USB_Audio_Debug_Guide.md`, `USB_Audio_RX_Diagnostics.md`,
  `USB_Audio_TX_Debug_Report.md`, `USB_Audio_TX_Diagnostic_Plan.md`, `usb_audio_debug_18Jan26.md`,
  `Transmit_Audio_Purity_Investigation.md`). **Start here** for context before designing.
- Teensy USB audio shares the USB device with serial; CAT already uses `SerialUSB1` (Dual
  Serial). USB Audio + Dual Serial composite descriptor interactions are likely part of the
  difficulty being debugged.

## What remains to be done (to scope)
- Consolidate the findings from the in-tree debug docs into a concrete plan (those docs are the
  primary source — read them first).
- Decide the USB device composition (audio + serial coexistence) and sample-rate/format.
- Route USB audio into/out of the DSP chain (RX audio → USB in; USB out → TX modulator),
  including resampling to/from the internal rates. ⚠️ **Those rates are no longer fixed**
  ([[sample-rate-switching]]): [[dsp-chain]] runs RX audio at **24 kHz @192 ksps / 22.05 kHz
  @176.4 ksps**, TX audio at **12 / 11.025 kHz**. Any resampler has to be rebuilt on a rate
  change, the same way the filters are ([[runtime-filter-design]]).
- **Possible shortcut:** at 176.4 ksps the RX audio rate is 22.05 kHz — an exact 1:2 of 44.1 kHz,
  where 24 kHz needs a 147:160 ratio to reach 44.1. Whether that is *why* 176.4 ksps was added is
  unrecorded and an open question for the owner ([[sample-rate-switching]]); if so, this page is
  where the connection belongs.
- Relationship to [[openaudio-library]] (#14): a library change may change the USB-audio path,
  so sequence the two.

## Relevant code & docs
`MainBoard_AudioIO.cpp/.h`; the `code/docs/USB_Audio_*` reports; [[dsp-chain]],
[[openaudio-library]].
