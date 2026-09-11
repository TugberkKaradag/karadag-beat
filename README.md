# Karadag Beat

A tempo-synced effect for chopping up audio: stutters, half-time, tape stops, reverse,
gates, sidechain pumps and filter sweeps. You draw them as envelopes over a 1, 2 or 4 bar loop.

VST3 and standalone app for 64-bit Windows.

![Karadag Beat](docs/screenshot.png)

## Install

Download `KaradagBeat-x.y.z-Setup.exe` from
[Releases](https://github.com/TugberkKaradag/karadag-beat/releases/latest) and run it.
In FL Studio, open Options > Manage plugins and click Find more plugins.

The installer isn't code-signed, so Windows may say it's from an unknown publisher.
Click More info > Run anyway.

## Features

- Time, volume and filter lanes
- 21 factory patterns, 27 slots for your own
- Trigger patterns from MIDI notes (C4 and up), with latch and retrigger
- Chain patterns across loops, swing
- Share patterns as `.kbeat` files

## Build

Visual Studio 2022 or newer and CMake 3.22+. JUCE 8.0.10 is downloaded automatically.

```
cmake -S . -B build -A x64
cmake --build build --config Release --target KaradagBeat_VST3
```

`build.ps1` builds, runs the tests and installs the plugin.
