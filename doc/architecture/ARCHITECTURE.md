# Architecture

## Goal

Kadoka Shougi AI is a multi-engine shogi laboratory. The shared game core stays independent from individual AI implementations so weak character engines, evaluation engines, learned engines, and heavyweight engines can all be compared under identical rules.

A second design goal is implementation clarity for Codex and other contributors: a task should have an obvious directory, a narrow dependency surface, and tests that state the acceptance conditions.

## Source layout

### `engine/`
Authoritative shogi state and rules.

Responsibilities:
- board and hand representation
- SFEN parsing/serialization
- pseudo-legal and legal move generation
- check/checkmate legality
- move make/unmake
- promotion and drops
- repetition/perpetual-check handling
- USI move notation helpers
- position hashing

No engine personality, search policy, UI behavior, or learned-model behavior belongs here.

### `engines/`
AI implementations behind the common `Engine` interface.

Planned families:
- `evaluate`
- `evaluate_learning`
- `hyper_fast`
- `mine_learning`
- `mine_learning_path`
- `tree_ai`
- `kadoka_best`
- `super_star`
- `obake_kadoka`
- `obake_maru`

### `protocol/`
Frontends and adapters such as CLI and USI. Protocol code translates external commands into core positions/search limits but does not implement shogi rules.

### `tools/`
Self-play, training, dataset conversion, benchmarks, tournament runners, and analysis utilities.

### `tests/`
Correctness and regression tests. Tests should be grouped by the subsystem they lock down. Small SFEN positions are preferred for rule regressions because they make failures reproducible and easy to understand.

### `doc/`
Project documentation other than the repository-level `README.md`.

Detailed documents belong in subdirectories:
- `doc/architecture/` — architecture and dependency decisions
- `doc/specifications/` — rule/API/file-format specifications
- `doc/diagrams/` — diagrams, tables, and visual design notes

Add new subdirectories instead of accumulating unrelated documents at the root of `doc/`.

## Dependency direction

Keep dependencies simple and one-way where practical:

```text
engine <- engines <- protocol/tools
   ^          ^
   |          |
 tests ------+
```

The shared `engine` layer must not import a concrete AI implementation. AI implementations may depend on the engine core. Frontends may depend on both but should not become a second rules engine.

## Codex implementation conventions

When adding a feature:

1. Put the behavior in the narrowest matching subsystem.
2. Prefer a small explicit API over hidden cross-module state.
3. Add or update tests in the same change.
4. Document non-obvious invariants in `doc/` rather than long comments scattered through source files.
5. Do not combine unrelated refactors with a rule/search/model change.
6. Keep unfinished rule layers explicit. For example, pseudo-legal generation must not silently claim to implement self-check filtering.
7. Preserve SFEN-based regression positions whenever fixing a rule bug.

A good Codex task should be expressible as: target files/directories, required behavior, acceptance tests, and explicitly out-of-scope behavior.

## Search contract

Every engine receives an immutable `Position` and `SearchLimits`, and returns a `SearchResult`.

Time-limited engines should keep a valid current best move so they can terminate cleanly at the requested budget. Engines that support full-search or convergence modes may expose those modes through engine-specific options later without changing the common match interface.

## Obake rule

Obake engines may propose silly or illegal intentions for UI/character purposes, but the authoritative game core never accepts an illegal move. UI speech and rejected-attempt logging stay outside the canonical game record.

## Near-term milestones

1. Lock board coordinates and SFEN round-tripping.
2. Implement pseudo-legal movement for every piece, promotion, and basic drop restrictions.
3. Filter self-check and implement attack/check detection.
4. Add make/unmake plus position hashing.
5. Implement pawn-drop mate rejection at the legal-move layer.
6. Add repetition/perpetual-check adjudication.
7. Add USI frontend.
8. Build the first deliberately weak Obake engine and the first real evaluation engine on the same interface.
