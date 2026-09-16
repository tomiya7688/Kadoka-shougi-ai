# Context Routing

Use the smallest route that matches the task. Expand only when a shared/public contract or uncertain dependency requires it.

## core-state

Board, hands, SFEN, make/unmake, hashing and canonical position state.

- Source: `engine/src/position.cpp` and matching headers under `engine/include/`
- Tests: `tests/core_smoke_test.cpp` plus the nearest regression test
- Docs: `doc/architecture/ARCHITECTURE.md`
- Validation: targeted test -> core test group -> full CTest for public/core API changes

## move-generation

Piece movement, promotion, drops, attack detection and legal-move filtering.

- Source: `engine/src/movegen.cpp`, `engine/src/legal.cpp`, `engine/src/attack.cpp`
- Tests: `tests/movegen_test.cpp`, `tests/legal_move_test.cpp`
- Docs: `doc/specifications/MOVE_GENERATION.md`, `doc/specifications/LEGAL_MOVE_GENERATION.md`
- Validation: SFEN-based targeted regression positions first

## runtime

Engine invocation, move acceptance/rejection and canonical state preservation.

- Source: `runtime/src/turn_runner.cpp` and `runtime/include/`
- Tests: `tests/runtime_turn_runner_test.cpp`
- Docs: `doc/specifications/ENGINE_RUNTIME.md`, `doc/architecture/ARCHITECTURE.md`
- Invariant: engine output is non-authoritative until validated by core legality

## protocol

CLI/USI/external adapters.

- Source: `protocol/`
- Tests: matching protocol tests when present
- Docs: relevant protocol specification; architecture when the common boundary changes
- Validation: producer + consumer smoke; protocol must not duplicate rules

## engines

Concrete AI/search/evaluation/learning implementations.

- Source: `engines/` when present
- Contracts: engine interface + `SearchLimits` / `SearchResult`
- Validation: fixed SFEN + fixed limits/seed where applicable; compare deterministic outputs before broad self-play
- Performance: benchmark search/evaluation hot paths under identical inputs

## tooling

Training, self-play, dataset conversion, benchmarks and analysis.

- Source: `tools/`
- Boundary: tooling may depend on runtime; runtime must not depend on tooling
- Validation: disposable output paths, bounded datasets/logs and reproducible inputs

## build-policy

CMake, CI, checker, formatter and one-command build behavior.

- Source: `CMakeLists.txt`, `build.bat`, `.github/workflows/`, `tools/kadoka_rule_checker/`
- Validation: rule checker -> Linux CMake/CTest -> Windows `build.bat` when platform/build behavior changes

## Broadening Rules

Broaden validation when any of these changes:

- engine/core public API
- legal-move semantics
- runtime/common engine contract
- CMake target/dependency direction
- protocol contract
- package/distribution boundary

Otherwise prefer targeted evidence and stop when acceptance is satisfied.

## Ignore Normally

- `build/`
- generated models/datasets
- large logs
- unrelated docs/issues/history

## Compact Completion Report

Record changed responsibility, behavior/compatibility impact, validation performed and relevant unverified areas only.
