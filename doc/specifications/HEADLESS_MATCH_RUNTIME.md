# Headless Match Runtime

## Scope

The headless match runner executes two `Engine` implementations without a GUI while preserving the authoritative shogi-core boundary.

Every AI decision passes through `run_engine_turn()`. Single-position terminal facts and history-dependent repetition facts come from the Core. Runtime execution failures remain Runtime facts.

The final public result is `MatchResult::outcome`, using the common contract in `GAME_OUTCOME.md`.

## API

```cpp
MatchResult run_headless_match(
    Engine& black_engine,
    Engine& white_engine,
    const Position& initial_position,
    const MatchLimits& limits = {}
);
```

`MatchLimits` contains per-side `SearchLimits`, `max_engine_attempts_per_turn`, and `max_plies`.

## Illegal-output retry

For one ply:

```text
current canonical Position
        ↓
Engine::search
        ↓
run_engine_turn validation
   ├─ legal   -> advance position
   └─ illegal -> keep same position and retry
```

`max_engine_attempts_per_turn` is the total number of engine calls permitted for that side on one ply.

Rejected outputs increment the per-side illegal-output counter, but they do not enter `accepted_moves`, do not enter repetition history, and never mutate the canonical position.

If the attempt limit is exhausted, the outcome is:

```text
result = Unresolved
reason = EngineAttemptLimit
```

`stopped_side` identifies the side whose engine could not continue. This is diagnostic information, not an automatic shogi loss.

## Checkmate and no-legal-move handling

`run_engine_turn()` reports `NoLegalMoves` without assigning a winner.

The Headless Match Runtime then asks the Core `adjudicate_terminal_position()` for the authoritative single-position fact.

If the side to move is checkmated:

```text
result = opponent win
reason = Checkmate
winner = opponent
loser = side to move
```

The engine is not called.

If a synthetic/non-standard position has no legal moves but the Core does not establish checkmate, the runtime returns:

```text
result = Unresolved
reason = NoLegalMoves
```

This prevents malformed positions from silently becoming wins.

## Repetition

The runner keeps canonical position history consisting of the initial position plus the position after every accepted move. After each accepted move it calls the Core repetition adjudicator.

Ordinary fourfold repetition maps to:

```text
result = ReplayRequired
reason = Repetition
```

This is deliberately not represented as `Draw`, because the Japan Shogi Association rule requires a replay and does not count the repetition game as a completed game.

Continuous-check repetition maps to a normal win/loss:

```text
result = opponent-of-checker win
reason = PerpetualCheckViolation
winner/loser = present
```

## Ply safety guard

If `max_plies` is reached before an official result:

```text
result = Unresolved
reason = PlyLimit
```

This is a runtime safeguard, not a game-rule draw.

## MatchResult fields

`MatchResult` contains:

- `outcome` — canonical result + reason + winner/loser
- `final_position`
- `accepted_moves`
- per-side illegal-output counts
- optional `stopped_side` for unresolved runtime stops

Consumers should use `outcome` for scoring and dataset metadata. `stopped_side` is diagnostics only.

## Deferred result sources

`GameEndReason` already reserves stable reason values for several upcoming paths, but this runner does not emit them yet:

- resignation
- time forfeit
- entering-king / impasse adjudication

External process crash/disconnect policy is also still deferred and should not be conflated with an official game loss unless an explicit match policy says so.

## Dataset and league use

League, benchmark, and dataset writers must preserve both `outcome.result` and `outcome.reason`.

Examples that must remain distinguishable:

- checkmate loss vs perpetual-check loss
- repetition replay vs scored draw
- official result vs engine/runtime failure
- future timeout vs resignation

## Sibling-project alignment

The same conceptual result schema should be used across Kadoka Shougi, Othello, and Tetris even though game-specific reasons differ:

```text
result + reason + winner/loser when known
```

This keeps cross-project league, dataset, Model Hub, and benchmark tooling compatible without forcing identical game-rule implementations.
