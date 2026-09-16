# Sibling Project Alignment

Kadoka Shougi AI, Kadoka Othello AI and Kadoka Tetris AI are sibling projects. Reuse proven engineering methods across them when the same problem exists, but keep game-specific rules, hot paths and language details local.

## Shared invariants

- Canonical game state belongs to the authoritative core.
- AI output is a proposal and must pass the same authoritative validation path as human/protocol input.
- Correctness before strength.
- Match-time Runtime stays independent from training, Creator, conversion and rich analysis tooling.
- Randomness/time-sensitive tests use explicit seeds/limits when practical.
- Headless/structured validation is preferred before GUI/manual validation.
- Hot paths may use localized performance exceptions without reversing dependency direction.

## Adopted from siblings

### Kadoka Othello AI

- Runtime / Creator-tooling dependency boundary
- compact `AI_CONTEXT.md` and Context Routing
- mechanically verifiable dependency checker
- model/package/tooling work kept out of normal inference hot paths

### Kadoka Tetris AI

- one-command build expectation
- Linux source CI plus Windows build/artifact evidence
- deterministic seed discipline
- distribution/artifact smoke as separate evidence from source tests
- never report unperformed GUI/artifact validation as passed

### Kadoka Shougi AI

This repository contributes the following back to the siblings:

- correctness-before-strength as an explicit engine principle
- engine/AI result is non-authoritative until core validation
- C++ `.clang-format` / `.clang-tidy` baseline
- narrow core/runtime/protocol dependency direction
- small reproducible position-based regression tests

## Cross-project review trigger

Inspect sibling implementations before introducing or redesigning:

- CI/build/release workflows
- common AI engine protocol
- Runtime/tooling boundaries
- deterministic benchmark methodology
- package/model formats
- coding-policy automation
- distribution artifact validation

Do not copy blindly. Adopt only what fits the local game's semantics and performance constraints.
