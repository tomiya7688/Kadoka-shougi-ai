# Codex Workflow

## When to use Codex

The repository is ready for Codex-driven implementation once the rules-core interfaces and documentation layout are stable. PR #2 established that baseline. New work should normally be given to Codex as a narrow task with explicit acceptance criteria.

## Task shape

A good Codex task should contain:

1. the target subsystem and files
2. the specification file under `doc/specifications/`
3. exact behavior to add or change
4. tests that must be added or updated
5. explicitly deferred behavior
6. a request not to refactor unrelated code

Example:

> Implement pawn-drop mate rejection in the legal move layer. Read `doc/specifications/MOVE_GENERATION.md` and `doc/specifications/LEGAL_MOVE_GENERATION.md`. Keep pseudo-legal generation unchanged except where strictly necessary. Add focused SFEN regression tests. Do not implement repetition or search changes.

## Repository boundaries

- `engine/` owns authoritative shogi state and rules.
- `engines/` owns AI implementations.
- `protocol/` owns CLI/USI adapters.
- `tools/` owns training, self-play, benchmarks, and conversion utilities.
- `tests/` mirrors functional behavior and regression cases.
- `doc/` owns detailed documentation. Keep only the project-level `README.md` at repository root.

## Review checklist

Before accepting a Codex PR:

- the change is limited to the requested subsystem
- new behavior has regression tests
- rule behavior is documented when the contract changes
- deferred behavior remains explicitly deferred
- no engine personality or AI policy leaks into the authoritative rules core
- no unrelated refactor is mixed into the task

## Performance rule

Do not optimize a rules path before correctness tests exist. Copy-based position transitions are acceptable during early development. Introduce make/unmake, hashing, bitboards, SIMD, or C-specific hot paths only behind tested behavior boundaries.
