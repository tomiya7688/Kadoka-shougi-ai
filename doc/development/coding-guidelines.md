# Coding Guidelines

This document defines the default implementation rules for Kadoka Shougi AI. The goal is to keep the codebase easy to extend while preserving direct, low-overhead implementations in performance-critical engine code.

## 1. Language and build baseline

- Application and engine code use C++20 by default.
- C-style or C implementations are allowed when profiling or the component contract makes the performance reason clear.
- New code must build without compiler extensions unless there is a documented platform requirement.
- Prefer standard-library facilities over adding dependencies for small utilities.

## 2. Architecture boundary: shogi core vs AI

The shogi core is the source of truth for rules and legality. AI implementations must not duplicate or redefine the rules.

The intended boundary is:

```text
Position / game state
        ↓
common AI interface
        ↓
AI implementation / adapter
        ↓
Move
        ↓
shogi core legality validation
```

Rules:

- Adding a new AI must not require changing the shogi rule implementation.
- AI-specific settings must not leak into the core move or position types unless they are genuinely game-level concepts.
- External engines, Python models, learned models, and experimental implementations connect through adapters.
- If an AI returns an illegal move, the core reports the illegal result; the AI does not become a second rules authority.
- Shared rule logic such as nifu, promotion restrictions, king safety, and pawn-drop mate belongs in the core.

## 3. UPD Commander Base Design policy

For non-hot-path application logic, use the design principles from:

- https://github.com/tomiya7688/upd-commander-base-design

The default responsibility split is:

- **Commander**: coordinates a use case and controls the sequence of work.
- **Messenger**: carries requests, results, and cross-boundary communication.
- **Processing**: performs the actual domain operation.

Avoid bypassing layers casually. A UI component should not directly reach into unrelated processing internals when a Commander/Messenger boundary is appropriate.

### Good candidates for Commander/Messenger/Processing

- UI flows
- configuration management
- match orchestration
- dataset management
- model management
- training-job management
- model download / registry operations
- non-hot-path application services

### Performance-first exceptions

Do not mechanically apply the Commander structure to code where abstraction overhead harms the purpose of the component.

Performance-first areas include:

- board representation
- attack detection
- move generation
- legality filtering
- search loops
- evaluation hot paths
- model inference inner loops
- time-critical routing
- Hyper Fast and similar latency-oriented engines

In these areas, prefer direct data flow, cache-friendly layouts, fewer allocations, and measurable performance. Keep the public boundary clean even when the internals are intentionally low-level.

## 4. Naming

Use these defaults:

- types, classes, structs, enums: `PascalCase`
- functions and variables: `snake_case`
- constants: `kPascalCase`
- files: `snake_case.hpp` / `snake_case.cpp`
- private data members: `snake_case_`

Prefer domain names over generic names such as `Manager`, `Util`, or `Helper` when a more precise responsibility exists.

## 5. Ownership and lifetime

- Prefer automatic storage and value types first.
- Use `std::unique_ptr` for exclusive heap ownership.
- Use `std::shared_ptr` only when shared ownership is actually required.
- Raw pointers and references are non-owning unless a local API explicitly documents otherwise.
- Use RAII for resources.
- Avoid global mutable state.
- Keep ownership visible at API boundaries.

## 6. Const-correctness and data access

- Mark read-only member functions `const`.
- Pass large read-only objects by `const&` where copying is not intentional.
- Use immutable transformations when they simplify correctness; optimize to make/unmake or mutable buffers only when profiling justifies the change.
- Prefer narrow interfaces over exposing internal containers for modification.

## 7. Enums and configuration

- Use `enum class` for piece types, colors, modes, states, and similar closed sets.
- Avoid magic integers and magic strings.
- Use named constants for fixed implementation constants.
- Use a configuration struct when a function would otherwise gain multiple boolean or loosely related scalar parameters.
- Configuration used for training, benchmarking, or official model generation must be serializable or otherwise reproducible.

## 8. Errors and expected failures

Do not use exceptions as normal control flow.

Expected runtime outcomes should use explicit result/status handling when practical, for example:

- illegal move
- timeout
- cancelled search
- unavailable model
- incompatible model format
- invalid training sample

Exceptions remain appropriate for programmer errors, invalid construction input, parse failures where the caller explicitly expects exception-based parsing, and unrecoverable invariant violations.

## 9. Logging and reproducibility

Library and engine code should not write arbitrary output directly to `std::cout`.

Use a controlled logging or reporting boundary so CLI, GUI, tests, and dataset-generation tools can choose how output is handled.

For training, league matches, benchmarks, and reproducibility-sensitive runs, retain enough metadata to reconstruct the run, including as applicable:

- random seed
- model and checkpoint version
- AI configuration
- search configuration
- time limit / time-control settings
- dataset version or manifest
- opponent identity / configuration
- build or package version

## 10. Tests

Rule changes require focused regression tests.

Prefer compact SFEN positions that isolate one rule. Important shogi-specific cases include:

- nifu
- pawn-drop mate (`uchi-fuzume`)
- leaving the king in check
- discovered attacks on the king
- promotion and non-promotion
- mandatory promotion / dead-rank movement
- drops and hand accounting
- check and checkmate behavior
- repetition / perpetual-check adjudication when implemented

Keep rule fixes separate from AI-strength tuning when possible. A rules regression should be reviewable without also evaluating evaluation weights or search heuristics.

## 11. Formatting

`.clang-format` is the repository formatting source of truth.

Typical usage:

```bash
clang-format -i engine/src/*.cpp engine/include/kadoka/*.hpp tests/*.cpp protocol/cli/*.cpp
```

Format touched C/C++ files before opening a PR. Avoid large format-only rewrites mixed with functional changes.

## 12. Static analysis

`.clang-tidy` defines the baseline static-analysis policy.

The initial check set focuses on:

- bug-prone constructs
- performance issues
- portability problems

Example CMake usage:

```bash
cmake -S . -B build -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build
```

Static-analysis fixes should remain understandable and should not introduce abstraction or allocations into hot paths merely to silence a warning. Suppressions must be local and justified.

## 13. Hot-path review rule

Before adding a layer, allocation, virtual dispatch, lock, container conversion, or string operation to a frequently executed engine path, ask whether the operation is required by the component contract.

If performance is part of the feature contract:

1. keep the implementation direct,
2. measure before and after meaningful changes,
3. document non-obvious optimizations,
4. keep the external API stable where possible.

## 14. Quick rules for implementation agents

Use this checklist before submitting code:

1. Core rules remain centralized in the shogi core.
2. A new AI connects through an interface/adapter rather than editing rule code.
3. Commander/Messenger/Processing is used for non-hot-path application flows where it improves separation.
4. Search, move generation, evaluation, and inference hot paths may use direct low-level code.
5. Names follow repository conventions.
6. Ownership is explicit; `unique_ptr` is preferred over shared ownership.
7. Expected failures use explicit status/result handling where practical.
8. No uncontrolled `std::cout` is added to library code.
9. New rule behavior has a compact regression test.
10. Touched C/C++ files are formatted and major clang-tidy findings are addressed or justified.
11. README stays concise; detailed specifications belong under `doc/`.
