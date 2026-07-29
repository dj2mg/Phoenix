---
title: CAT Control (Kenwood emulation)
type: module
status: draft
created: 2026-06-08
updated: 2026-07-29
tags: [cat, kenwood, ts-480, ts-2000, serial, rig-control, usb]
source_refs: []
related: ["[[overview]]", "[[main-loop]]", "[[mode-state-machine]]", "[[tune-frequency-control]]", "[[persistent-config]]", "[[sample-rate-switching]]", "[[filter-hil-test]]", "[[audio-equalizer]]", "[[cw-processing]]"]
---

# CAT Control (Kenwood emulation)

**Files:** `CAT.cpp` (~28 KB), `CAT.h`. Computer-Aided Transceiver control so standard
logging/contest/digital software (WSJT-X, fldigi, hamlib, N1MM…) can drive the radio.

## Transport
- **Port:** `SerialUSB1` — the *second* USB serial interface (requires Arduino "USB Type =
  Dual Serial"). The first port is the debug/console serial.
- **Baud:** 38400.
- **Framing:** ASCII commands terminated by `;`. Polled each pass by `CheckForCATSerialEvents()`
  from [[main-loop]], which buffers until the terminator then parses.

## Protocol ⚠️ TS-480 vs TS-2000
The implemented dialect is the **Kenwood** 2-letter command set. The two source docs
**disagree on which radio**: `CAT.cpp:21` says *"Kenwood TS-480 CAT Interface (partial)"* (and
`code/docs/ts_480_pc.pdf` is the reference), but `CAT.h` says *"Kenwood TS-2000 protocol"*. The
two share most commands so it works either way, but the docs should be reconciled — logged in
[[documentation-todos]].

## Architecture: a command-dispatch table

The core is a static table of `valid_command` entries (`NUM_SUPPORTED_COMMANDS = 29`,
`CAT.cpp:104-136`):
```c
typedef struct {
    char name[3];                       // 2-letter command + NUL
    int  set_len, read_len;             // expected payload lengths
    char* (*write_function)(char*);     // "set" handler
    char* (*read_function)(char*);      // "query" handler
} valid_command;
```
`command_parser()` matches the incoming 2-letter code against the table and dispatches to the
**write** (set) or **read** (query) handler based on the command length — a query is just the
bare command (e.g. `FA;`), a set carries a payload (`FA00014200000;`). Read handlers format
`ED`/hardware state into a Kenwood response string; write handlers apply the change.

> ⚠️ **Adding a command? `set_len` and `read_len` must differ.** `command_parser` tests the
> **write form first**, so a command declaring `set_len == read_len` has an **unreachable read
> handler** — it becomes silently write-only. This is not hypothetical: it is exactly the bug
> that made `FW;` unreadable for as long as `FW` has existed (see below).

## Implemented commands (29)

| Group | Commands |
|---|---|
| Frequency / VFO | `FA`/`FB` (VFO A/B freq), `FR`/`FT` (RX/TX VFO select), `FW` (filter high cut) |
| Mode / status | `MD` (mode), `IF` (transceiver status string), `ID` (radio id) |
| Band | `BU`/`BD` (band up/down) |
| Gain / audio | `AG` (AF gain), `MG` (mic gain), `PC` (power control) |
| DSP | `NR` (noise reduction), `NT` (notch), `VX` (VOX) |
| Keyer | `KS` (keyer speed / WPM) |
| PTT | `TX` (transmit), `RX` (receive) |
| Misc | `AI` (auto-info), `PS` (power on/off), `PD`, `DB` |
| **Custom (non-Kenwood)** | `ED` (dump the `ED` config), `PR` (dump the `hardwareRegister`), **`SR`**, **`CF`**, **`EQ`**, **`FL`** |

`ED` and `PR` are Phoenix extensions explicitly marked "NOT a Kenwood keyword" — handy debug
dumps of [[persistent-config]] and the [[filter-boards]] `hardwareRegister`.

### DSP-settings commands (added 2026-07, `CAT.cpp:132-135`)

Added so [[filter-hil-test]] could drive settings that were previously **touchscreen-only**.

| Cmd | Set | Read | Meaning |
|---|---|---|---|
| `SR` | `SRn;` | `SR;` | Sample rate: `0` = 176.4 ksps, `1` = 192 ksps ([[sample-rate-switching]]). **Rejected while transmitting** — it reconfigures the I²S clock and rebuilds the whole DSP chain (`CAT.cpp:804-827`). |
| `CF` | `CFn;` | `CF;` | Receive CW audio filter index 0–5 ([[cw-processing]]). |
| `EQ` | `EQbbvvv;` | `EQbb;` | Receive equaliser cell `bb` = 00–13, level `vvv` = 000–100 ([[audio-equalizer]]). Note set/read lengths differ by design. |
| `FL` | `FL####;` | `FL;` | DSP filter **low** cut — the mirror of `FW`'s high cut. |

⚠️ **Still no CAT command for AGC mode.** That is the one setting [[filter-hil-test]] needs and
cannot have, so the suite refuses to run rather than setting it.

## Three fixes to pre-existing commands

All three affected CAT users on earlier releases:

- ⚠️ **`MD` inverted the receive filter.** `MD_write` set `bands[].mode` but never
  `ED.modulation[]`. `Demodulate()` switches on the *latter*, while `InitFilterMask()` *compares
  the two* and treats a difference as a deliberate departure from the band default — by
  **mirroring the passband**. So `MD` did not merely fail to change the demodulator; it flipped
  the sideband of the receive filter. (Same normalization the display had to learn — see
  `EffectivePassbandEdges_Hz` in [[display-subsystem]].)
- **`MD1`/`MD2` could not escape CW.** They now dispatch `TO_SSB_MODE` when in CW receive. That
  transition previously existed **only on the front-panel button**, so CAT could enter CW mode
  and never leave it ([[mode-state-machine]]).
- **`FW` was write-only.** It declared `set_len == read_len`, making `FW;` unreachable per the
  parser rule above. Also `MD4` now selects **AM**, which was previously unreachable (`MD5`
  gives SAM).

## How commands apply
CAT does **not** have a private path to the hardware — it feeds the **same channels as the
front panel**:
- **Event dispatch:** writes enqueue `InterruptType` events into the [[main-loop]] FIFO — e.g.
  a frequency set raises `iUPDATE_TUNE` (`CAT.cpp:221`), band up/down raise `iBUTTON_PRESSED`
  (`CAT.cpp:174,183`). So CAT tuning flows through [[tune-frequency-control]] exactly like the
  encoder.
- **Direct `ED` writes:** simple settings write straight to [[persistent-config]] (e.g.
  `KS_write` → `ED.currentWPM`, `CAT.cpp:475`).
- **PTT:** `TX_write` dispatches into [[mode-state-machine]] (switches on `modeSM.state_id`),
  so CAT PTT obeys the same TX interlocks/keying as physical PTT.

Read handlers serialize state back: `IF_read` builds the long TS-480 status string (VFO freq,
mode, RX/TX, etc.); `FA_read`/`MD_read`/`KS_read`/… each return one field.

## Open questions
- Reconcile TS-480 (`CAT.cpp`) vs TS-2000 (`CAT.h`) and verify the `IF` status-string field
  layout against `code/docs/ts_480_pc.pdf`.
- A CAT command for **AGC mode** — the remaining gap for [[filter-hil-test]], which currently
  has to demand the operator set it by hand.
- `SR` is interlocked against transmit, but the **menu path is not** ([[sample-rate-switching]]).
  Whether that asymmetry is deliberate.
- The set/read-length constraint is a parser property enforced only by convention. Nothing
  fails at build time if a new command violates it — a static assertion over the table would
  catch the next `FW`.
- Which `unsupported_cmd` stubs exist and how the radio replies to unknown commands (the `?`
  error response?).
- Whether `AI` (auto-information) actually pushes unsolicited updates or is a no-op.
