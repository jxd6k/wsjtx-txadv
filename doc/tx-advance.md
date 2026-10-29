# TX advance for network audio

Settings → Advanced → **TX advance** accepts 0–2000 ms in 50 ms steps.
The default is zero. A positive value sends scheduled audio earlier; it does
not alter RX sample windows or the computer clock. Set RX timing using the
existing external clock setup, then adjust TX independently. Start the network
latency test profile at 900 ms.

The main scheduler, sound-device modulator, and native TCI modulator share the
same per-transmission advance. The value is captured before requesting PTT;
accepted settings edits apply to the next transmission. Tune is immediate.
At normal completion, local audio stops early but PTT retains its original
release deadline to allow queued audio to drain. Halt Tx cancels the added hold.
The existing Tx delay (PTT-to-audio sequencing) is unchanged.

For standard FT8, a 900 ms advance places the nominal audio start 400 ms before
the assigned slot boundary *on the computer clock*. Network latency and an
external clock adjustment determine when it actually reaches the air. Neither
TX advance nor a clock adjustment can recover reply time already lost to
network transport and decoding.

## Local macOS build

With CMake, GNU Fortran, Qt 5, Boost, FFTW, Hamlib, libusb, and pkg-config
installed through Homebrew:

```sh
bash .github/scripts/build-local-macos.sh
```

The script builds `cmake-build-release/wsjtx.app`, including the decoder,
helper programs, shipped data, and sounds. This is a local development bundle
using installed Homebrew libraries, not a portable signed release. It skips
MAP65 and documentation generation. The standard macOS shared-memory setup is
still required (see `Darwin/ReadMe.txt`).

## Verification

`test_tx_timing` checks period and even/odd boundaries (including midnight),
FT4's fractional period, zero-advance alignment, late starts, captured settings,
normal versus explicit-stop PTT delays, and Tune. It renders two complete real
FT8 modulator waveforms and compares advanced output with baseline output.
It uses no audio output device or radio.

To export 12 kHz WAV recordings with a simulated 900 ms transport delay:

```sh
mkdir -p /tmp/wsjtx-tx-recordings
WSJT_TX_TEST_RECORDINGS=/tmp/wsjtx-tx-recordings \
  cmake-build-release/tests/test_tx_timing
cmake-build-release/jt9 --ft8 -a /tmp/wsjtx-tx-recordings \
  -t /tmp/wsjtx-tx-recordings -r "$PWD" \
  /tmp/wsjtx-tx-recordings/000000_000015.wav \
  /tmp/wsjtx-tx-recordings/000000_000030.wav
```

Expected messages: `CQ K1ABC FN42` and `K1ABC W9XYZ -10`, with DT near zero.
These recordings test waveform integrity and receive placement, not the full
GUI automatic QSO sequence. Live automatic replies, CAT/PTT final-symbol
delivery, and native TCI need testing with the actual remote setup. The new
test is registered with CTest and is included in the existing Windows build
workflow's test run; a Windows runner is required to validate that build.

## Windows test installer on GitHub

Push the `custom` branch to your fork. The **Windows TX-advance test build**
workflow runs automatically on each push, reusing the existing Windows build,
CTest, runtime dependency audit, and installer packaging jobs. Installer names
include `txadv` and the workflow run number so test builds are distinguishable.

In the fork's Actions tab, enable workflows if GitHub asks, then open the run.
After it succeeds, download the artifact whose name ends in
`windows-x86_64-installer`, extract it, and use the included `.exe` installer.
The other executable-only artifact is not a complete installation.

Manual runs use `workflow_dispatch`; GitHub requires that workflow file to
also exist on the fork's default branch before displaying its Run workflow
button. Push-triggered builds do not require changing the default branch.
The installer uses the upstream workflow's self-signed development signature.
The Windows build and remote hardware timing must pass testing before treating
this as a validated release.
