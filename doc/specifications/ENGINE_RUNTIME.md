# Engine Runtime Contract

## Scope

The runtime layer sits between player implementations (human/native/external/script) and the authoritative shogi core.

The public boundary is deliberately game-shaped: the game exposes only information an ordinary player can see or know, the player returns an action, and the game reports the result of that action. When the match ends, the game emits the completed game-history record. The runtime validates move decisions against core legality before deriving the next canonical position. Non-move decisions such as resignation or entering-king declaration are surfaced explicitly and never disguised as board moves.

This boundary must not require AI-only or internal helper data such as a precomputed legal-move list, check flag, repetition history, handcrafted evaluation features, search candidates, policy targets, game ID, or dataset metadata. If an AI needs legal moves, check state, repetition history, or other derived state, it reconstructs and maintains that information itself.

## Dependency direction

```text
Creator / Training / Analysis
            ↓
       Engine / Adapter
            ↓
          Runtime
            ↓
        Shogi Core
```

The runtime may depend on the authoritative rule APIs. The rule core must not depend on runtime, a concrete AI, GUI behavior, dataset tooling, or transport code.

## One-turn API

```cpp
TurnResult run_engine_turn(
    Engine& engine,
    const Position& position,
    const SearchLimits& limits = {}
);
```

The current C++ helper passes an immutable `Position` to native engines. This is an implementation convenience for in-process engines, not the definition of the external/public game protocol. `SearchResult::action` defaults to `EngineAction::Move`, preserving existing engines. Move actions are checked against the authoritative legal-move set. `Resign` is a semantic action with no synthetic square or fake move encoding. Real-world procedures such as entering-king declaration or mutually agreed impasse are not required parts of the normal game-facing Player API.

An external AI package may bundle its own internal board, shogi move generator, history tracking, preprocessing, screen-recognition input, and search stack. The game does not need to send legal moves, check status, repetition history, or game bookkeeping IDs to it. Regardless of the AI's internal rules implementation, the core validates the returned action and remains the sole authority over canonical state.

### `MoveApplied`

The engine returned a legal move.

- `search_result` is present.
- `next_position` is present.
- the next position is produced only after core validation succeeds.

### `IllegalMove`

The engine returned a move that is not in the authoritative legal-move set.

- `search_result` is present so adapters/UI/tooling can inspect the rejected attempt.
- `next_position` is absent.
- the input `Position` is unchanged.
- the side to move is unchanged.
- this result is non-terminal: an illegal move alone never causes defeat or game end.
- callers may query the same player again or let a human retry.
- GUI/CLI should visibly report that the attempted move was illegal.
- the rejected attempt may be recorded as a rejected-action event for learning/evaluation, but it is not appended to the canonical legal move sequence.

This is intentional so rule-unaware AIs can be trained and evaluated, and so character engines such as Kadoka can make mistakes and try again while the game itself never accepts an illegal move.

### `Resigned`

The engine explicitly resigned.

- `search_result` is present.
- `next_position` is absent.
- match runtime converts this to a win for the opponent with `GameEndReason::Resignation`.

### `NoLegalMoves`

The current position has no legal move.

- the engine is not called.
- `search_result` is absent.
- `next_position` is absent.

Game-result adjudication remains a separate responsibility. This status only reports that the legal-move set is empty.

## Match-level policy

Alternating players and match safety policy live in the headless match runtime built on top of this one-turn API. Ordinary illegal moves are not converted into losses by a retry limit; they leave the board and side-to-move unchanged and return an explicit illegal result.

See `doc/specifications/HEADLESS_MATCH_RUNTIME.md`.

The separation is intentional:

- `run_engine_turn()` owns legality validation for exactly one decision.
- the headless match runner owns retry count, side selection, and match safety limits.
- future GUI or external-process coordinators may use the same one-turn primitive with different retry/presentation policies.

## External AI alignment

External adapters must implement the game-facing player boundary. Transport details such as JSON, process I/O, IPC, network connections, Python, Rust, or Go belong outside the shogi core.

Conceptually:

```text
Observable game state / ordinary match context
      ↓
External AI Adapter / AI Package
      ↓
Player action
      ↓
Turn Runner
      ↓
Core legal validation
      ↓
Action result / game result
```

A transport may encode the visible board state with SFEN or another agreed representation. Public time information may be included when it is part of the ordinary match context (remaining time, byoyomi, move deadline). A transport must not depend on receiving the core's legal-move list, check flag, repetition history, or match/game ID. AI-specific search limits, node limits, evaluation features, candidate lists, and history-derived features belong behind the adapter/package boundary unless a separate optional extension is explicitly defined.

Completed game history is an output of the game/match layer after terminal adjudication. It is not part of per-turn PlayerObservation. Game-history storage may contain bookkeeping metadata such as a game ID, but such metadata is not sent to the player merely because it exists in the record.

The persistent process protocol accepts exactly one decision record per response:

```text
move normal <from_file> <from_rank> <to_file> <to_rank> <promote_0_or_1>
move drop <piece> <to_file> <to_rank>
action resign
```

Existing `move` responses remain unchanged. Multiple decision records are rejected. The semantic source of truth remains the normalized `EngineAction` / `TurnStatus`, not transport text.

## Sibling-project alignment

This follows the Kadoka AI family direction shared with Kadoka Othello AI and Kadoka Tetris AI:

- standalone authoritative game core that remains useful with no AI installed
- AI behind an adapter/interface boundary
- no required legal-move-list feed from game to AI
- runtime validation of AI output
- no direct AI mutation of canonical game state
- native fast path separate from external transport
- Creator/Training/Analysis kept out of the match hot path

Binary protocol formats do not need to be identical between games; the responsibility boundaries should remain aligned.

## Deferred

This layer still does not define:

- timeout/cancellation result policy beyond `SearchLimits`
- JSON/USI serialization
- mutual-impasse agreement signaling between players
- dataset logging

Those should build on this runtime contract rather than bypass it.
