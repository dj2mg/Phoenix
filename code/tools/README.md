# Phoenix SDR Bench Tools

Host-side Python tools for testing and diagnosing the radio. None of them are
part of the firmware build; they drive the radio over its USB serial ports and
read it back through test instruments on the bench.

| Tool | What it is for | Hardware needed |
|---|---|---|
| [`filter_hil/`](filter_hil/README.md) | Receive filter rate-independence test suite | AD2 + radio |
| [`tx_filter_hil/`](tx_filter_hil/README.md) | Transmit filter rate-independence test suite | AD2 + radio |
| [`flag_timing.py`](#flag_timingpy) | Measure firmware execution timing from `Flag()` pin transitions | AD2 + radio |
| [`plot_flag_timing.py`](#plot_flag_timingpy) | Plot distributions from a `flag_timing.py` capture | none |
| [`serial_diag.py`](#serial_diagpy) | Interactive CAT control + live diagnostic monitor | radio |
| [`usb_audio_test.py`](#usb_audio_testpy) | Send a test tone into the radio's USB audio input | radio |
| [`siglent_capture.py`](#siglent_capturepy) | Bare two-channel capture from a networked SIGLENT scope | SIGLENT scope |
| [`extract_filter_prototypes.py`](#extract_filter_prototypespy) | Recover analog specs from the old frozen filter tables | none |

"AD2" is a Digilent Analog Discovery 2.

## Setup

Everything runs from a virtual environment in this directory. It is not
versioned — create it once:

```bash
cd code/tools
python3 -m venv venv
./venv/bin/pip install numpy scipy matplotlib pyserial sounddevice pyvisa pyvisa-py
```

The HIL suites and the project Skills invoke `code/tools/venv/bin/python`
explicitly, so use that interpreter rather than activating the environment.

Per-tool dependencies, if you would rather install selectively:

| Package | Needed by |
|---|---|
| `numpy` | everything except `serial_diag.py`'s CAT-only mode |
| `matplotlib` | the plotting tools and both HIL suites |
| `scipy` | `extract_filter_prototypes.py` |
| `pyserial` | anything that talks CAT: `serial_diag.py`, both HIL suites |
| `sounddevice` | `usb_audio_test.py`, and the audio commands in `serial_diag.py` |
| `pyvisa`, `pyvisa-py` | `siglent_capture.py` |

### The Digilent WaveForms SDK

The AD2 tools bind `libdwf` through `ctypes`, so the WaveForms runtime must be
installed system-wide (`libdwf.so` on Linux, the `dwf` framework on macOS, the
`dwf` DLL on Windows). It is not a pip package. If a tool exits with
"libdwf.so not found", install WaveForms from Digilent rather than looking for
a Python package.

### Serial ports

With the Teensy configured for **Dual Serial** the radio presents two ports:

| Port | Teensy name | Purpose | Baud |
|---|---|---|---|
| `/dev/ttyACM0`, `COM3` | `Serial` / `SerialUSB` | Diagnostic output | 115200 |
| `/dev/ttyACM1`, `COM4` | `SerialUSB1` | CAT commands | 38400 |

With the **serial+midi+audio** USB type there is only one port, carrying both;
`serial_diag.py --port` covers that case.

On Linux, add yourself to the `dialout` group once:

```bash
sudo usermod -a -G dialout $USER   # log out and back in
```

### Outputs

PNGs and CSVs written into this directory are gitignored, as are the
`results/` directories of the two HIL suites. Everything needed to regenerate
a HIL report lives in its JSON, so commit a run artifact only when you
deliberately want it as a reference.

---

## Hardware-in-the-loop filter test suites

Two full test suites, each with its own README. They verify on the real radio
that the DSP filters land on their labelled frequencies at **every** supported
sample rate — the regression they exist to catch scales every corner by
`176400/192000`, i.e. −8.125 %.

- **[`filter_hil/`](filter_hil/README.md)** — receive chain. An AD2 injects a
  quadrature tone into the I/Q inputs and reads demodulated audio off the
  speaker output. Requires **AGC off**; the suite refuses to run otherwise.
- **[`tx_filter_hil/`](tx_filter_hil/README.md)** — transmit chain. An AD2
  drives the microphone input and captures both exciter outputs synchronously,
  giving the two-sided spectrum and with it opposite-sideband suppression. Keys
  the radio continuously for minutes at a time, so fit a dummy load.

Both take the same shape of command line:

```bash
cd code/tools
./venv/bin/python filter_hil/filter_hil_test.py
./venv/bin/python tx_filter_hil/tx_filter_hil_test.py

# check arguments without touching hardware
./venv/bin/python filter_hil/filter_hil_test.py --dry-run

# redraw figures from a previous run
./venv/bin/python filter_hil/plot_filter_hil.py filter_hil/results/filter_hil_*.json

# self-tests for the measurement maths, no hardware
./venv/bin/python filter_hil/test_filter_hil.py
./venv/bin/python tx_filter_hil/test_tx_filter_hil.py
```

Exit codes for both: `0` pass, `1` fail, `2` error (no hardware, or preflight
refused), `3` aborted.

Read the suite's own README before running one — the wiring, the preflight
conditions, and the failure table are all there. Background and results are in
`wiki/firmware/filter-hil-test.md` and `wiki/firmware/tx-filter-hil-test.md`.

---

## `flag_timing.py`

Measures how long firmware code takes to run. `Flag(val)` in `Globals.cpp`
writes a 4-bit value across Teensy pins 28–31; this tool captures those pins on
the AD2's digital inputs, decodes them back into flag values, and reports how
long the code spent in each one.

Wiring (overridable with `--dio-map`):

```
Teensy 28 -> AD2 DIO1     (flag bit 3, MSB)
Teensy 29 -> AD2 DIO2     (flag bit 2)
Teensy 30 -> AD2 DIO3     (flag bit 1)
Teensy 31 -> AD2 DIO4     (flag bit 0, LSB)
```

Output is a single JSON document on **stdout**; progress goes to stderr. On
error, stdout still holds JSON — `{"error": "..."}` — so a caller can parse it
uniformly. Exit code is non-zero on any error.

```bash
# 100 ms capture, streamed over USB
./venv/bin/python flag_timing.py --duration 0.1 --mode record > profile.json

# time from Flag(2) to the next Flag(3), triggered on entry to Flag(2)
./venv/bin/python flag_timing.py --duration 0.5 --trigger flag --trigger-flag 2 \
    --measure 2 3 > profile.json
```

Key options:

| Option | Default | Notes |
|---|---|---|
| `--sample-rate` | 10 MHz | Rounded to the nearest AD2 clock divider |
| `--duration` | — | Seconds; overrides `--buffer-size` |
| `--buffer-size` | 8192 | Clamped to the device maximum |
| `--mode` | `auto` | `single` uses the on-board buffer (~8K samples, fastest); `record` streams over USB for 1 s+ captures |
| `--debounce-us` | 2.0 | `Flag()` writes its four pins sequentially, so brief intermediate values appear during a transition. Discards segments shorter than this |
| `--max-lost-frac` | 0.001 | Record mode: fail if USB dropped more than this fraction of samples |
| `--trigger` | `none` | `change` on any pin transition, `flag` on a specific value |
| `--measure FROM TO` | — | Adds a `measurement` block: intervals from `Flag(FROM)` to the next `Flag(TO)` |
| `--dio-map` | `28:1,29:2,30:3,31:4` | Teensy pin to AD2 DIO line |

The JSON contains `metadata`, `transitions`, `segments`, a `summary` with
per-flag statistics and transition counts, and `measurement` when `--measure`
was given. `--include-raw` adds the decoded sample array (large).

The `timing-measurement` Skill wraps the whole workflow: instrument the
firmware, flash it, capture, plot.

## `plot_flag_timing.py`

Turns a `flag_timing.py` JSON into PNGs. No hardware.

For each flag value with enough events it writes a two-panel figure — a
duration histogram with mean/median/p95/p99 marked, which makes bimodal
distributions obvious, and a duration-vs-iteration scatter, which reveals drift
or clustering of slow events. It also writes a timeline strip of the whole
capture coloured by flag.

```bash
./venv/bin/python plot_flag_timing.py profile.json

./venv/bin/python plot_flag_timing.py profile.json --out-dir docs/ --prefix baseline \
    --flag-labels 1=DrawDisplay-other,2=SignalProcessing,3=DrawSpectrumPane
```

Options: `--out-dir` (defaults to beside the JSON), `--prefix`, `--flag-labels`,
`--min-count` (skip flags with fewer events, default 2), `--no-timeline`,
`--quiet`. PNG paths are printed to stdout.

---

## `serial_diag.py`

Interactive CAT control of the radio with the diagnostic stream displayed live
in the same window, plus USB audio tone generation so a transmit test needs
only this one tool.

```bash
# list ports
./venv/bin/python serial_diag.py --list

# dual-serial firmware: separate diagnostic and CAT ports
./venv/bin/python serial_diag.py --diag /dev/ttyACM0 --cat /dev/ttyACM1

# serial+midi+audio firmware: one port for both
./venv/bin/python serial_diag.py --port /dev/ttyACM0

# monitor only, or control only
./venv/bin/python serial_diag.py --diag /dev/ttyACM0
./venv/bin/python serial_diag.py --cat /dev/ttyACM1
```

Baud rates default to 115200 for diagnostics and 38400 for CAT
(`--diag-baud`, `--cat-baud`, `--baud` for single-port mode).

### Interactive commands

| Command | Description |
|---|---|
| `tx` | Key the transmitter and start the test tone |
| `rx` | Unkey and stop the tone |
| `tone <Hz>` | Set tone frequency (default 1000) |
| `monitor` | Toggle RX audio monitoring, showing RMS level |
| `devices` | List host audio devices |
| `audio <n>` | Select audio device by index |
| `usb` | Switch to USB audio mode (PC audio input) — **`UM` CAT command, not in this firmware** |
| `ssb` | Switch to SSB / microphone mode — **`UM` CAT command, not in this firmware** |
| `lsb` / `usbsb` | Select lower / upper sideband |
| `cw` | Switch to CW mode |
| `freq <kHz>` | Set frequency, e.g. `freq 14074` |
| `id` | Query radio ID |
| `if` | Query full radio status |
| `stats` | Show the last USB_RX statistics block |
| `cat <cmd>` | Send a raw CAT command, e.g. `cat FA;` |
| `help` | Command help |
| `quit` / `exit` | Exit (auto-unkeys TX) |

The audio commands need `sounddevice`; without it the tool still runs and says
so in the help text.

`usb` and `ssb` send `UM1;`/`UM0;`, which only exist on the unmerged `usb_audio`
branch. On this branch the radio answers `?;` and the tool now says so rather
than reporting success. Every other command above maps to a handler that is
present in `CAT.cpp`.

Note that an accepted write frequently produces **no** reply — `TX_write` and
`RX_write` both return an empty string — so silence means success and `?;` is
the only failure signal. `tx` and `rx` therefore take about half a second to
return, waiting out the response timeout.

### Diagnostic output

Whatever the firmware prints on the diagnostic port is shown with a host
timestamp. Two line formats are additionally parsed and accumulated for the
`stats` command:

```
[14:32:15.123] USB_RX: RUNNING reads=94 underruns=0 zeros=0 | buf=2048/2052/2056 | ratio=1.0000/1.0001/1.0002 | rms=0.1234
[14:32:15.123] USB_TX: cb=48 samples=6144 zeros=0(0%) overruns=0 | buf=2048/2052/2056 | samp/cb=128/128 | rms=0.1234
```

`USB_RX` is the ASRC consumer side, `USB_TX` the PC-to-radio USB callback side.
Lines with `underruns > 0` are highlighted red; `WARMUP` lines yellow.

| Field | Meaning | Expected |
|---|---|---|
| `state` | ASRC state | `RUNNING`; `WARMUP` while initialising |
| `reads` | Successful read operations | Incrementing |
| `underruns` / `overruns` | Buffer underflow / overflow | 0 |
| `zeros` | Blocks with near-zero signal | Low while transmitting |
| `buf=min/avg/max` | Ring buffer level, samples | ~2048 (half of 4096) |
| `ratio=min/avg/max` | ASRC resampling ratio | ~1.0000, limits at 0.97/1.03 |
| `rms` | Signal RMS | Non-zero while transmitting |

**No current firmware emits these lines.** They came from
`usb_audio_48k.cpp` behind a `USB_AUDIO_RX_DIAGNOSTICS` `#define`; that file was
replaced by `MainBoard_AudioIO.cpp` and neither the macro nor the instrumentation
survives in the tree. The parser is documented here because it is what the tool
still expects — restore an equivalent one-per-second print in the audio path and
`stats` works again. Everything else in the tool (CAT control, tone generation,
RX monitoring, raw line display) is unaffected.

Typical USB-audio TX investigation: start the tool, `usb`, `if` to confirm
state, run the transmitting application (WSJT-X or similar), and watch for
`underruns` above zero, `buf` falling below 1024, `ratio` pinned at a limit, or
`rms` stuck at zero. `stats` prints the accumulated totals.

If nothing appears: check that the instrumentation exists in the firmware you
flashed, that you are on the diagnostic port rather than the CAT port, and that
the radio is actually in USB transmit. If CAT does not respond: check the port
and the 38400 baud rate, and try `id`.

## `usb_audio_test.py`

Sends a sine wave to a host audio output — normally the Teensy's USB audio
device — to exercise the PC-to-radio audio path on its own.

```bash
./venv/bin/python usb_audio_test.py -l              # list output devices
./venv/bin/python usb_audio_test.py -d 5            # device index 5, 1 kHz
./venv/bin/python usb_audio_test.py -f 800 -a 0.5   # 800 Hz at 50 %
./venv/bin/python usb_audio_test.py -t 10 -s        # 10 s, stereo
```

Options: `-d/--device`, `-l/--list`, `-f/--frequency` (1000), `-a/--amplitude`
(0.3), `-r/--rate` (48000), `-t/--duration` (continuous until Ctrl+C),
`-s/--stereo`. Requires `sounddevice`.

---

## `siglent_capture.py`

Pulls the current traces off a networked SIGLENT oscilloscope over VISA/LAN
without changing any of its settings — useful when the scope is already set up
by hand and you want the data on the host.

```bash
./venv/bin/python siglent_capture.py
```

Prints point count, time span, voltage range and Vpp for each channel, and
writes `channel1_data.csv` / `channel2_data.csv`. The scope address is the
`SCOPE_IP` constant at the top of the file — edit it for your bench. Requires
`pyvisa` and `pyvisa-py`.

For AD2 captures there is no equivalent standalone script: use the `ad2.py`
module inside whichever HIL suite matches the measurement
(`filter_hil.ad2.Ad2` for single-channel scope plus quadrature AWG,
`tx_filter_hil.ad2.TxAd2` for synchronous two-channel I/Q).

---

## `extract_filter_prototypes.py`

A one-shot developer tool, not a bench instrument, and it needs no hardware.

The receive chain used to ship coefficient tables designed offline at a single
sample rate (24 ksps audio, i.e. 192 ksps / 8), so running at any other rate
scaled every corner by `actual_rate / 24000`. Those tables are now generated at
run time from an analog spec instead. This script is what recovered those
specs: it reads the reference tables in `code/test/reference_filters.cpp` and
prints the C literals belonging in `InitializeReceiveAudioFilterCoeffs()` and
`InitializeTransmitFilterCoeffs()`.

```bash
./venv/bin/python extract_filter_prototypes.py
```

Its output is pasted into `DSP_FIR.cpp`; it is not part of the build. Re-run it
if a reference table ever changes. Requires `numpy` and `scipy`. See
`wiki/firmware/runtime-filter-design.md` for the background.
