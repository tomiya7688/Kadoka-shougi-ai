# Engine Runtime Contract

## Scope

The runtime layer sits between player implementations (human/native/external/script) and the authoritative shogi core.

The public boundary is deliberately game-shaped: the game exposes ordinary observable game state, the player returns an action, and the game reports the action result and eventual game result. The runtime validates move decisions against core legality before deriving the next canonical position. Non-move decisions such as resignation or entering-king declaration are surfaced explicitly and never disguised as board moves.

This boundary must not require AI-only helper data such as a precomputed legal-move list, handcrafted evaluation features, search candidates, or policy targets.

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

The current C++ helper passes an immutable `Position` to native engines. This is an implementation convenience for in-process engines, not the definition of the external/public game protocol. `SearchResult::action` defaults to `EngineAction::Move`, preserving existing engines. Move actions are checked against the authoritative legal-move set. `Resign` and `DeclareEnteringKing` are semantic actions with no synthetic square or fake move encoding.

An external AI package may bundle its own shogi move generator and preprocessing. The game does not need to send legal moves to it. Regardless of the AI's internal rules implementation, the core validates the returned action and remains the sole authority over canonical state.

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
- callers may query the same engine again, switch adapter/engine, or apply character-specific UI behavior without contaminating the canonical game record.

This is the runtime form of the project rule that Obake/Kadoka-style engines may make rejected attempts while the game itself never accepts an illegal move.

### `Resigned`

The engine explicitly resigned.

- `search_result` is present.
- `next_position` is absent.
- match runtime converts this to a win for the opponent with `GameEndReason::Resignation`.

### `EnteringKingDeclaration`

The engine explicitly invoked the entering-king declaration procedure.

- `search_result` is present.
- `next_position` is absent because a declaration is not a board move.
- match runtime asks the authoritative impasse adjudicator to determine win, replay, or declaration loss.
- an invalid declaration is a terminal loss, not an illegal-move retry.

### `NoLegalMoves`

The current position has no legal move.

- the engine is not called.
- `search_result` is absent.
- `next_position` is absent.

Game-result adjudication remains a separate responsibility. This status only reports that the legal-move set is empty.

## Match-level policy

Retry limits and alternating two engines now live in the headless match runtime built on top of this one-turn API.

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

A transport may encode the board with SFEN or another agreed representation. Time information may be included when it is part of the ordinary match context (remaining time, byoyomi, move deadline). A transport must not depend on receiving the core's legal-move list. AI-specific search limits, node limits, evaluation features, or candidate lists belong behind the adapter/package boundary unless a separate optional extension is explicitly defined.

The persistent process protocol accepts exactly one decision record per response:

```text
move normal <from_file> <from_rank> <to_file> <to_rank> <promote_0_or_1>
move drop <piece> <to_file> <to_rank>
action resign
action declare_entering_king
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
