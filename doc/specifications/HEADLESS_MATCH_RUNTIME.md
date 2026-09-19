# Headless Match Runtime

## Scope

The headless match runner executes two `Engine` implementations without a GUI while preserving the authoritative shogi-core boundary.

Every AI decision passes through `run_engine_turn()`. Single-position terminal facts, history-dependent repetition facts, and impasse facts come from the Core. Runtime execution failures and tournament policy selection remain Runtime facts.

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

`MatchLimits` contains per-side internal engine limits and optional headless-runner safety limits such as `max_plies`.

The normal game itself does not impose an illegal-move retry ceiling. Headless/benchmark tooling may expose explicit safety-stop settings to prevent a broken or rule-unaware engine from running forever, but such a stop is tooling policy rather than a shogi rule or player loss.

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

Rejected move outputs increment the per-side illegal-output counter, but they do not enter `accepted_moves`, do not enter canonical rule history, never mutate the canonical position, never change side-to-move, and do not end the game.

`EngineAction::Resign` ends the game immediately with a resignation loss. Ordinary illegal move attempts remain retryable without a game-rule limit.

If a headless test/benchmark explicitly configures a safety stop for repeated invalid output, reaching that stop returns an unresolved tooling/runtime result. It is not a shogi loss and is not part of the normal game rules.

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

Continuous-check repetition maps to a normal win/loss:

```text
result = opponent-of-checker win
reason = PerpetualCheckViolation
winner/loser = present
```

Repetition is checked before automatic impasse after an accepted move, so a continuous-check repetition loss is not hidden by a later result rule.

## Real-world adjudication procedures

Entering-king declaration and mutually agreed impasse are real-world/tournament procedures and are not required parts of the normal software-game Player API. If a specific tournament or compatibility mode needs them, they belong in an explicit optional policy/profile above the ordinary game interaction contract.

The normal software-game loop remains board observation → player action → validation/result, without forcing these real-world procedures into every AI or UI path.

## Ply safety guard

If `max_plies` is reached before an official result:

```text
result = Unresolved
reason = PlyLimit
```

This is a runtime safeguard, not a game-rule draw.

## MatchResult fields

`MatchResult` contains `outcome`, `final_position`, `accepted_moves`, per-side illegal-output counts, and optional `stopped_side` for unresolved runtime stops.

Consumers should use `outcome` for scoring and dataset metadata. `stopped_side` is diagnostics only.

## Deferred result sources

`GameEndReason` reserves stable reason values for paths still requiring protocol/runtime actions:

- time forfeit

External process crash/disconnect policy is also still deferred and should not be conflated with an official game loss unless an explicit match policy says so.

## Dataset and league use

League, benchmark, and dataset writers must preserve both `outcome.result` and `outcome.reason`, plus any explicitly selected tournament/compatibility policy when relevant.

This keeps checkmate, repetition replay, impasse replay, runtime failure, timeout, resignation, and tournament-specific maximum-move rules distinguishable.

## Sibling-project alignment

The same conceptual result schema should be used across Kadoka Shougi, Othello, and Tetris even though game-specific reasons differ:

```text
result + reason + winner/loser when known
```

This keeps cross-project league, dataset, Model Hub, and benchmark tooling compatible without forcing identical game-rule implementations.
