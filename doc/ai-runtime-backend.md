# AI Runtime Backend Boundary

## Purpose

All AI implementations enter the match Runtime through one proposal boundary. The Runtime owns legality validation and state transition; AI implementations never mutate the authoritative `Position`.

```text
Position + SearchLimits
        |
     AIBackend
        |
   SearchResult
        |
    TurnRunner
        |
legal move validation
        |
 next Position / rejection
```

## Canonical backend kinds

`AIBackendKind` uses the Kadoka AI sibling-project vocabulary:

- `native`
- `dynamic_library`
- `external_process`
- `script`
- `network`

These names describe execution/transport, not game logic or model architecture.

## Native backend

Existing `Engine` implementations remain the native engine contract. `NativeEngineBackend` adapts them to `AIBackend` without moving engine/search responsibilities into Runtime.

```text
Engine::search()
      |
NativeEngineBackend
      |
 run_ai_turn()
```

## Transport adapters

`FunctionAIBackend` is a small integration adapter for transport implementations owned elsewhere. A process/script/dynamic-library/network transport may expose a callable that returns `SearchResult`; the TurnRunner remains unchanged.

Transport code must not be placed in the Core. Persistent process/session management, script runtimes, DLL loading and network clients belong to Runtime/adapter modules.

## Authority rule

A backend result is always non-authoritative.

- Runner generates the authoritative legal-move list.
- Backend proposes one move.
- Runner compares the proposal against legal moves.
- Illegal output does not mutate the current Position.
- If no legal move exists, the backend is not called.

This rule applies equally to native and external implementations.

## Performance rule

Do not force slow transport onto native models. The common boundary costs one Runtime-level dispatch per decision; search/evaluation hot loops stay inside the backend.

For external processes, prefer a persistent session over spawning a process and creating temporary files for every move. Process startup and serialization are compatibility costs, not part of the native hot path.

## Sibling-project mapping

- Othello: `IAIEngine` + package interface/adapter -> authoritative `Game::play`
- Shogi: `AIBackend` -> `TurnRunner` -> authoritative `Position::after_move`
- Tetris: observation -> AI adapter/backend -> semantic command -> authoritative game core

The games do not share move/state binary types. They share the boundary semantics and backend vocabulary.
