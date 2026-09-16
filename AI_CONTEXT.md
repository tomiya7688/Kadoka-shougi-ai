# AI Context

This is the compact entrypoint for AI-assisted work. Do not preload the whole repository, all docs, all Issues or generated artifacts.

## Project

- Name: Kadoka Shougi AI
- Main language: C++20
- Purpose: authoritative shogi core, interchangeable AI engines, lightweight match runtime, protocols and AI-development tooling

## Source of Truth

- Architecture: `doc/architecture/ARCHITECTURE.md`
- Engine/runtime contract: `doc/specifications/ENGINE_RUNTIME.md`
- Move generation: `doc/specifications/MOVE_GENERATION.md`
- Context/validation routing: `doc/architecture/CONTEXT_ROUTING.md`
- Sibling-project engineering policy: `doc/architecture/SIBLING_PROJECT_ALIGNMENT.md`
- Current task: GitHub Issue or explicit user request
- Source/tests: `engine/`, `runtime/`, `protocol/`, `tests/`

## Start Here

1. Read the current task.
2. Choose the matching route in `doc/architecture/CONTEXT_ROUTING.md`.
3. Read target source and matching tests first.
4. Read detailed specs only when the affected contract requires them.
5. Stop broad exploration when Goal / Required / Acceptance and validation are clear.

## Important Invariants

- The engine/core owns canonical shogi legality and state.
- AI/engine output is a proposal; runtime/core validates it before state changes.
- Correctness comes before engine strength.
- Runtime stays small and must not depend on training, analysis or conversion tooling.
- Fixed positions, seeds and limits are preferred for reproducible engine tests/benchmarks.
- Protocol/frontends translate commands; they do not implement a second rules engine.
- Hot-path optimizations are allowed when they preserve public boundaries and correctness.

## Ignore Normally

- `build/`
- generated model/data outputs
- large logs
- unrelated Issues/history
- successful validation logs after the result is known

## Validation

Broad/shared changes use:

```text
python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Prefer targeted tests first for local rule/runtime changes. Broaden for core/public/build-contract changes.

## Working Rules

- Search first, read second.
- Do not mix unrelated refactors into the task.
- Summaries are indexes, not replacements for source/specs.
- Report relevant unverified areas rather than reading unrelated code to fill gaps.
