# Architecture

## Goal

Kadoka Shougi AI is both a standalone shogi application/core and a multi-engine shogi laboratory. The shared game core must remain fully usable without any AI implementation: human-vs-human play, legal move enforcement, game progression, result adjudication, record/replay, and protocol/front-end use must not require an AI package. AI players are optional consumers of the game boundary, not part of the authority of the game itself.

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

No engine personality, search policy, UI behavior, learned-model behavior, model loading, or AI-package management belongs here.

The core may generate legal moves because a complete shogi game needs that capability. This does **not** mean the common game-to-AI API supplies a legal-move list. An AI package that wants legal moves for search is responsible for deriving them from the observed board/game state using its own bundled logic. The core remains the final authority and validates every returned player action before mutating canonical state.

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

### `runtime/`
Minimal match-time coordination shared by built-in and external AI adapters.

Responsibilities:
- connect a player/AI adapter to the game-facing player contract
- provide ordinary game observations/state needed by a player
- accept semantic player actions such as move/resign/declaration
- validate returned actions against authoritative core legality
- report action results and final game results
- preserve the current canonical position when an AI returns an illegal move

The runtime must stay small. Training, dataset conversion, model analysis, GUI behavior, and transport-specific process management do not belong in the match hot path.

### `protocol/`
Frontends and adapters such as CLI and USI. Protocol code translates external commands into ordinary game observations/actions/results but does not implement shogi rules. AI-specific search features and legal-move candidate lists are not required game-protocol payloads.

Future process/IPC/network adapters should translate their external representation into the common engine/runtime boundary rather than bypassing core validation.

### `tools/`
Self-play, training, dataset conversion, benchmarks, tournament runners, and analysis utilities.

Tooling may use runtime for actual games, but heavy conversion, training, and analysis stay outside runtime.

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
                 engines
                    ↓
engine/core ← runtime ← protocol / headless tools
     ↑          ↑
     └── tests ─┘

Creator / Training / Analysis
             ↓
        engines/adapters
             ↓
           runtime
             ↓
         engine/core
```

The shared `engine` layer must not import a concrete AI implementation or runtime behavior. AI implementations may depend on the engine core. Runtime depends on the common engine contract and authoritative legal-move APIs, but not on a concrete AI family. Frontends may depend on runtime and adapters but should not become a second rules engine.

Runtime must not depend upward on Creator/Training/Analysis. This mirrors the sibling-project direction used by Kadoka Othello AI and Kadoka Tetris AI: match-time code stays lightweight, while model creation and heavy data work remain outside the hot path.

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

## Game-facing player contract

The public game/AI boundary is intentionally game-shaped rather than search-engine-shaped.

Conceptually the game provides only what an ordinary shogi player/client can observe: the current board/game state and normal match context. A player returns a semantic action. The game then returns the action result and, when the match ends, the game result.

```text
Game observation/state
        ↓
Player / AI adapter
        ↓
Player action
        ↓
Authoritative core validation + state transition
        ↓
Action result / game result
```

The common boundary does **not** require the game to send a precomputed legal-move list, handcrafted evaluation features, search candidates, policy targets, or other AI-specific helper data. AI packages may bundle their own legal-move generation, preprocessing, search, evaluation, and model code so they remain portable to other compatible shogi environments.

The core remains authoritative even when an AI contains its own rules implementation: every returned action is validated by the game before canonical state changes.

The existing C++ `Engine::search(Position, SearchLimits)` interface is an internal native-engine convenience layer and must not be treated as the external/public game protocol. Adapters may translate between the game-facing player contract and an engine-specific internal search API.

See `doc/specifications/ENGINE_RUNTIME.md`.

## Obake rule

Obake engines may propose silly or illegal intentions for UI/character purposes, but the authoritative game core never accepts an illegal move. Runtime reports the rejected attempt and leaves the canonical position unchanged. UI speech and rejected-attempt logging stay outside the canonical game record.

## Near-term milestones

1. Lock board coordinates and SFEN round-tripping.
2. Implement pseudo-legal movement for every piece, promotion, and basic drop restrictions.
3. Filter self-check and implement attack/check detection.
4. Add make/unmake plus position hashing.
5. Implement pawn-drop mate rejection at the legal-move layer.
6. Add a validated game-facing player boundary for human/native/external adapters without exposing legal-move lists as a required AI input.
7. Add repetition/perpetual-check adjudication.
8. Add USI frontend.
9. Build the first deliberately weak Obake engine and the first real evaluation engine on the same interface.
