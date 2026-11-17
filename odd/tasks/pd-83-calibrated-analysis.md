# PD-83 — Calibrated analysis data pipeline

## Objective
Implement the smallest deterministic, in-memory analysis pipeline that produces calibrated time-frame and frequency-bin data for known-tone fixtures without decoding files, touching source bytes, or introducing UI/worker behavior.

## Scope
- Add a versioned C ABI for synchronous spectral analysis over an existing borrowed AudioForgeAudioBuffer.
- Use explicit rectangular-window DFT settings: caller supplies FFT size and hop size; output bins are `0..fftSize/2`, frequency is `bin * sampleRate / fftSize`, and frame starts are `frameIndex * hopSize`.
- Use the selected interleaved channel only; output caller-owned frame-major magnitudes and coordinate arrays.
- Add known-tone assertions, invalid-parameter checks, and input immutability checks to the existing Engine smoke test.

## Constraints
- Maximum three implementation files: `API/audioforge/engine.h`, `Engine/src/engine.cpp`, `Tests/Engine/engine_api_smoke_test.c`.
- No decoder, persistence, Bridge, UI, worker, allocation, or source-file I/O.
- Strict TDD: RED → GREEN → REFACTOR.
- Conventional commit; no AI attribution.

## Acceptance criteria
- Known tone appears at the expected calibrated frequency bin and frame coordinates.
- Invalid FFT/hop/output capacity/channel inputs fail without writing samples or metadata beyond documented reset fields.
- Input audio remains byte-for-byte unchanged.
- Existing API smoke coverage remains green in Debug and Release.

## Checklist
- [ ] Define analysis request/output ABI and calibration contract.
- [ ] Implement bounded synchronous DFT pipeline.
- [ ] Add known-tone and safety tests.
- [ ] Run Debug/Release build and CTest plus harness checks.
- [ ] Commit and update Jira PD-83.

## Verification
- `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64`
- `cmake --build build --config Debug --target AudioForgeEngineTests`
- `ctest --test-dir build -C Debug --output-on-failure`
- `cmake --build build --config Release --target AudioForgeEngineTests`
- `ctest --test-dir build -C Release --output-on-failure`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `python3 Harness/run_checks.py --config Debug`
- `git diff --check`
