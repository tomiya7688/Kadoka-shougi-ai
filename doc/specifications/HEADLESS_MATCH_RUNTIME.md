# Headless Match Runtime

## Scope

The headless match runner executes two `Engine` implementations without a GUI while preserving the authoritative shogi-core boundary.

Every AI decision passes through `run_engine_turn()`. Single-position terminal facts, history-dependent repetition facts, and automatic 500-move impasse facts come from the Core. Runtime execution failures remain Runtime facts.

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

Rejected outputs increment the per-side illegal-output counter, but they do not enter `accepted_moves`, do not enter canonical rule history, and never mutate the canonical position.

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

Continuous-check repetition maps to a normal win/loss:

```text
result = opponent-of-checker win
reason = PerpetualCheckViolation
winner/loser = present
```

Repetition is checked before automatic 500-move impasse after an accepted move, so a continuous-check repetition loss is not hidden by the later result rule.

## Automatic 500-move impasse

The same canonical history is passed to `adjudicate_500_move_impasse()`.

When move 500 completes without check, the runner immediately returns:

```text
result = ReplayRequired
reason = Impasse
```

If move 500 ends in check, the Core keeps the result pending while that checking side continues its sequence. The replay result is emitted when that side first makes a move that does not continue check.

Point totals do not affect the automatic 500-move rule.

Correct deferred-check adjudication requires canonical history containing the exact position after move 500 (`ply == 501`). A run started from a later standalone SFEN does not contain enough information to reconstruct whether the move-500 check sequence was continuous, so the runtime does not guess an impasse result in that case.

## Entering-king declaration and mutually agreed impasse

These are player actions/agreements rather than ordinary moves.

The authoritative Core APIs and `GameOutcome` mapping already exist, but the current `Engine::search()` contract returns only `Move`. Therefore the Headless Match Runtime does not silently auto-declare or invent mutual agreement.

A later action/protocol extension should expose explicit declaration/agreement actions and call the existing Core adjudicators. See `IMPASSE_ADJUDICATION.md`.

## Ply safety guard

If `max_plies` is reached before an official result:

```text
result = Unresolved
reason = PlyLimit
```

This is a runtime safeguard, not a game-rule draw.

The default guard remains useful for malformed/custom positions, but standard games run from normal history now have the official automatic 500-move impasse path before that safeguard.

## MatchResult fields

`MatchResult` contains:

- `outcome` — canonical result + reason + winner/loser
- `final_position`
- `accepted_moves`
- per-side illegal-output counts
- optional `stopped_side` for unresolved runtime stops

Consumers should use `outcome` for scoring and dataset metadata. `stopped_side` is diagnostics only.

## Deferred result sources

`GameEndReason` reserves stable reason values for paths still requiring protocol/runtime actions:

- resignation
- time forfeit
- explicit entering-king declaration transport
- explicit mutual-impasse agreement transport

External process crash/disconnect policy is also still deferred and should not be conflated with an official game loss unless an explicit match policy says so.

## Dataset and league use

League, benchmark, and dataset writers must preserve both `outcome.result` and `outcome.reason`.

Examples that must remain distinguishable:

- checkmate loss vs perpetual-check loss
- repetition replay vs impasse replay vs scored draw
- official result vs engine/runtime failure
- future timeout vs resignation

Tournament-selectable 24/27-point impasse policy must also be preserved in match configuration when relevant.

## Sibling-project alignment

The same conceptual result schema should be used across Kadoka Shougi, Othello, and Tetris even though game-specific reasons differ:

```text
result + reason + winner/loser when known
```

This keeps cross-project league, dataset, Model Hub, and benchmark tooling compatible without forcing identical game-rule implementations.
