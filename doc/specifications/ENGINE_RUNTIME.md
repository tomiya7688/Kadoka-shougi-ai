# Engine Runtime Contract

## Scope

The runtime layer sits between AI engines and the authoritative shogi core.

Its first responsibility is deliberately small: run one engine decision, validate move decisions against the core's legal-move list, and only then derive the next canonical position. Non-move decisions such as resignation or entering-king declaration are surfaced explicitly and never disguised as board moves.

This is the common path for built-in engines and future external/script/process adapters.

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

The engine receives the immutable current `Position`. `SearchResult::action` defaults to `EngineAction::Move`, preserving existing engines. Move actions are checked against the authoritative legal-move set. `Resign` and `DeclareEnteringKing` are semantic actions with no synthetic square or fake move encoding.

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

Future external adapters should still implement or wrap the common `Engine` decision boundary. Transport details such as JSON, process I/O, IPC, network connections, Python, Rust, or Go belong outside the shogi core.

Conceptually:

```text
Position / limits
      ↓
External AI Adapter
      ↓
Engine::search
      ↓
Turn Runner
      ↓
Core legal validation
      ↓
MoveApplied / IllegalMove / Resigned / EnteringKingDeclaration / NoLegalMoves
```

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

- minimal authoritative game core
- AI behind an adapter/interface boundary
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
