# Engine Runtime Contract

## Scope

The runtime layer sits between AI engines and the authoritative shogi core.

Its first responsibility is deliberately small: run one engine decision, validate the returned move against the core's legal-move list, and only then derive the next canonical position.

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

The engine receives the immutable current `Position`. The runtime obtains the authoritative legal moves from the core and compares the engine's `best_move` against that set.

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

### `NoLegalMoves`

The current position has no legal move.

- the engine is not called.
- `search_result` is absent.
- `next_position` is absent.

Game-result adjudication remains a separate responsibility. This status only reports that the legal-move set is empty.

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
MoveApplied / IllegalMove / NoLegalMoves
```

The external protocol may serialize an illegal-move response, but the semantic source of truth is `TurnStatus::IllegalMove`.

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

This slice intentionally does not define:

- retry-count policy for repeatedly illegal AI output
- timeout/cancellation policy beyond `SearchLimits`
- external process lifecycle
- JSON/USI serialization
- match result adjudication
- repetition/perpetual-check result handling
- dataset logging

Those should build on this runtime contract rather than bypass it.
