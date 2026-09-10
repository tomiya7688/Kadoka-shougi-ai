# Architecture

## Goal

Kadoka Shougi AI is a multi-engine shogi laboratory. The shared game core must remain independent from any individual AI so that weak character engines, evaluation engines, learned engines, and heavyweight engines can all be compared under identical rules.

## Layers

### `engine/`
Authoritative shogi state and rules.

Responsibilities:
- board and hand representation
- legal move generation
- check/checkmate legality
- move make/unmake
- promotion and drops
- repetition/perpetual-check handling
- SFEN/USI move notation
- position hashing

No engine personality or learned model behavior belongs here.

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

## Search contract

Every engine receives an immutable `Position` and `SearchLimits`, and returns a `SearchResult`.

Time-limited engines should keep a valid current best move so they can terminate cleanly at the requested budget. Engines that support full-search or convergence modes may expose those modes through engine-specific options later without changing the common match interface.

## Obake rule

Obake engines may propose silly or illegal intentions for UI/character purposes, but the authoritative game core never accepts an illegal move. UI speech and rejected-attempt logging stay outside the canonical game record.

## Near-term milestones

1. Lock board coordinates and SFEN round-tripping.
2. Implement pseudo-legal movement for every piece.
3. Add drops, promotion rules, and mandatory promotion.
4. Filter self-check and implement legal move generation.
5. Add make/unmake plus position hashing.
6. Add repetition/perpetual-check adjudication.
7. Add USI frontend.
8. Build the first deliberately weak Obake engine and the first real evaluation engine on the same interface.
