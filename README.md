# VitaPlug Gainer80

VitaPlug Gainer80 is an audio effect developed in C++17 with JUCE 9.0.3. On
macOS it produces AU v2 (`.component`) and VST3 (`.vst3`); Windows builds are
explicitly VST3-only. Every format shares one processor, parameter model and
JUCE editor. The DSP layer is deliberately independent of host and UI code so
it can later be reused by VitaDAW and other formats.

## v0.1.0 — development foundation

This first tagged development version freezes the approved G80 interface and
the following behaviour:

| Control | Published parameter | Range / behaviour |
| --- | --- | --- |
| GAIN | `gain` | −24.0 to +24.0 dB; applied after the shelf |
| FATTY | `bass` | 0.0 to +12.0 dB low-shelf gain |
| LOWER / UPPER | `fattyMode` (`Fatty Frequency`) | LOWER = 80 Hz (default); UPPER = 160 Hz |

All three parameters are automatable and saved with sessions. `fattyMode`
defaults to LOWER when loading an older saved state that did not contain it.
The shelf paths are crossfaded over 20 ms when the mode changes. With FATTY at
0.0 dB, both modes are neutral. GAIN and FATTY are smoothed to avoid clicks.

The editor has RMS INPUT and OUTPUT VU meters calibrated at **0 VU = −18 dBFS
RMS**, with a 300 ms VU-style response and 30 Hz visual update. INPUT is before
the DSP; OUTPUT is after the shelf and GAIN. Stereo meter energy is averaged,
not summed, so anti-phase channels cannot cancel the reading.

## Stable plug-in identity

| Field | Value |
| --- | --- |
| Bundle identifier | `com.vitaplug.gainer80` |
| AU type | `aufx` |
| Manufacturer code | `Vtpl` |
| Plug-in code | `Gn80` |
| Version | `0.1.0` |

Do not change these identifiers in future releases: DAW projects rely on them
to restore the plug-in and its parameters.

## Prerequisites

- macOS with Apple Clang, an installed macOS SDK, CMake 3.22+ and Unix
  Makefiles (`make`)
- Git
- JUCE **9.0.3**, fixed at commit
  `be29c81492b6151c8ea8d14c840e1311963b3a83`

Fetch the JUCE dependency exactly once after cloning this repository:

```sh
git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git external/JUCE
git -C external/JUCE rev-parse HEAD
# Expected: be29c81492b6151c8ea8d14c840e1311963b3a83
```

## Build and test

Build reproducible universal Release artifacts with Unix Makefiles:

```sh
./scripts/build-universal.sh
./scripts/verify-artifacts.sh
```

The script creates independent slices and then merges corresponding Mach-O
files with `lipo`:

- `x86_64`, `CMAKE_OSX_DEPLOYMENT_TARGET=10.15`
- `arm64`, `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`

Both trees use C++17. Final AU and VST3 bundles are under `dist/Release/` and
are ad-hoc signed for local validation.

For the active Apple-Silicon development slice:

```sh
cmake -S . -B build/arm64 -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build/arm64 --config Release --target VitaPlugGainer80DSPTests VitaPlugGainer80_AU
ctest --test-dir build/arm64 --output-on-failure
```

The DSP test covers neutral paths, GAIN, the 80/160 Hz shelves, the 20 ms mode
crossfade, state migration, and the VU visual-map anchors and monotonicity.

## Compatibility and validation status

The universal build encodes macOS 10.15 for Intel and macOS 11.0 for Apple
Silicon. These are binary deployment targets, not a claim that every host or
macOS release has been exercised.

- AU `auval` and final Logic smoke testing were completed locally on the
  development Apple-Silicon setup.
- The approved v0.1.0 Logic smoke covered the G80 visual interface, control
  interaction and audio behaviour.
- A native Intel host smoke and a VST3 host validation remain required before
  claiming broad production compatibility.

Logic loads the AU format; VST3 validation needs a compatible VST3 host.

## Experimental Windows VST3 beta

The GitHub Actions workflow **Windows VST3 x64** is started manually from the
repository's Actions tab. It checks out JUCE 9.0.3 at the fixed commit, builds
Release x64 with Visual Studio, runs CTest, validates the VST3 bundle layout
and uploads a ZIP containing only `VitaPlug Gainer80.vst3` and its internal
structure.

In this MIGUELOMENER Edition worktree, the workflow uploads the
`VitaPlug-Gainer80-MIGUELOMENER-Edition-v0.1.0-windows-x64-vst3`
artifact, extract it without changing the bundle structure, and copy the
resulting `VitaPlug Gainer80.vst3` folder to one of the VST3 locations accepted
by the target host, commonly `%LOCALAPPDATA%\Programs\Common\VST3` for the
current user. Then rescan plug-ins in a VST3-capable host.

This is an experimental x64 beta until a successful Windows workflow run and a
host smoke test are recorded. It is **not** a native Pro Tools/AAX build; Pro
Tools requires a VST3-capable bridge/host path or a future AAX implementation.

## Licensing and distribution

JUCE 9 is dual-licensed under AGPLv3 and JUCE's commercial licence. Publishing
the source/binaries under AGPL has corresponding-source obligations; closed
source distribution requires an appropriate JUCE commercial licence. This is a
technical summary, not legal advice; see [JUCE's licence information](https://juce.com/legal/juce-9-licence/).

The artifacts produced here use **ad-hoc** signatures only. They are suitable
for local development validation, not public distribution. Distribution to
other musicians requires Developer ID signing and notarization.

## Repository scope

The repository tracks sources, tests, CMake files, scripts and the G80 assets
compiled into the plug-in. It intentionally excludes build trees, local
backups, generated bundles and the JUCE checkout; the exact JUCE revision and
recovery commands above make the dependency reproducible.
