# Changelog

All notable changes to **Khaen** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.1] — 2026-11-08

Stability release. Fixes a hard crash on the first played note for users whose CPU does not
implement AVX2, plus two latent audio-thread hazards found while investigating it.

### Fixed

- **Host crash on the first note on CPUs without AVX2** — `STATUS_ILLEGAL_INSTRUCTION`
  (`0xC000001D`). The distributed binaries were compiled with `/arch:AVX2`, so every build
  contained unconditional AVX2 instructions (`vpaddd ymm`, `vpmulld`, `vpbroadcastd ymm`,
  `vzeroupper`). On Intel CPUs older than Haswell, AMD CPUs older than Excavator, and most
  Atom / Celeron / Pentium parts, the plugin editor opened normally but the host died the first
  time DSP code ran — i.e. exactly on the first note. Release builds now target the x86-64
  baseline (SSE2). `/arch:AVX2` and `-march=native` are gated behind the opt-in
  `KHAEN_NATIVE_OPT` CMake option, which is `OFF` by default and must never be enabled for a
  distributed build.
- **Infinite loop in the respiratory phase wrap** (`FreeReedModel::processSample`). The lung
  phase was wrapped with `while (diff < -pi) diff += twoPi;`, which never terminates when `diff`
  is `±inf` (`inf + twoPi == inf`), hanging the audio thread forever the moment a note started.
  Replaced with a bounded `wrapPi()` helper that also absorbs `NaN` and `inf`.
- **Out-of-bounds read in the granular pitch shifter** (`GranularCrystallizer::pitchShift`). A
  non-finite read position became a negative index after the `int` cast. The position is now
  validated and must be wrapped into the circular buffer before indexing.

### Added

- **Test 14 — Host configuration robustness** (`Tests/DspTest.cpp`): renders every factory preset
  at 44.1 / 48 / 96 / 192 kHz, with block sizes of 64 / 512 / 2048 samples, in mono and stereo,
  failing on any non-finite output sample or on a block taking longer than 2000 ms (hang
  detector). This is the regression guard for the class of bugs fixed above.
- **"CPU Compatibility" section in the README**, documenting the illegal-instruction failure
  mode, the affected CPU families and the `KHAEN_NATIVE_OPT` policy.
- This changelog.

### Changed

- Project version bumped to **1.0.1** in `CMakeLists.txt`; the VST3 `moduleinfo.json` and the
  plugin version reported to hosts now read `1.0.1`.

### Checksums

- `Khaen_v1.0.1_Windows.zip` — SHA256 `f8e44a5dfa71271b933504e3fce5eb6cad5dcb0c7b54b1e10a9cb37b4ecf7766`

---

## [1.0.0] — initial release

First public release.

- 16-voice MPE polyphonic free-reed physical model: Hunt-Crossley tongue contact, continuous
  respiratory cycle (exhale / inhale), vocal-tract formants, chiff noise.
- Effects chain: Tape Delay → Harmonizer → Granular Crystallizer → Reverb engine with five Laos
  location presets.
- Spectral analyzer with **Aurora** RTA and 2D vector modes (Aero-Vectorial, Reed Matrix) plus a
  3D pipe/reed visualiser.
- HELP user-manual overlay, MIDI Learn / MIDI mapping dialog, factory preset manager.
- Formats: VST3, AU, Standalone (Windows builds published as a ready-to-run ZIP).
