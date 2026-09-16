# Headless Match Runtime

## Scope

The headless match runner executes two `Engine` implementations without a GUI while preserving the authoritative shogi-core boundary.

It is intentionally a Runtime component, not a second rules engine. Every AI decision is passed through `run_engine_turn()`, so built-in engines, character engines, and future external adapters all receive the same legality treatment.

History-dependent shogi rules are also delegated to the Core. The runtime stores canonical history and asks the Core repetition adjudicator for a result after each accepted move.

## API

```cpp
MatchResult run_headless_match(
    Engine& black_engine,
    Engine& white_engine,
    const Position& initial_position,
    const MatchLimits& limits = {}
);
```

`MatchLimits` currently contains:

- per-side `SearchLimits`
- `max_engine_attempts_per_turn`
- `max_plies`

The two sides may therefore use different time/node/depth budgets while still sharing one match loop.

## Illegal-output retry

An engine may return an illegal move. This is expected to be possible for external adapters and character-oriented engines such as Kadoka/Maru.

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

`max_engine_attempts_per_turn` is the total number of `Engine::search` calls permitted for that side on that ply.

Examples:

- `1`: no retry after the first illegal output
- `3`: at most three total engine decisions for that ply
- `0`: the attempt limit is already reached; the match stops without calling the engine

Illegal outputs are counted in `black_illegal_outputs` / `white_illegal_outputs` for diagnostics, but they are not inserted into `accepted_moves`, do not enter repetition history, and never mutate the canonical position.

## Accepted move record and history

`MatchResult::accepted_moves` contains only moves accepted by the authoritative Core.

The runner also keeps an in-memory canonical position history consisting of the initial position plus the position after every accepted move. This history is passed to `adjudicate_repetition()` after each accepted move.

This distinction is important for later Dataset generation:

- canonical match record: legal accepted moves only
- diagnostic/character log: rejected intentions may be recorded separately by a higher layer

The headless runtime does not currently persist either form to disk.

## End reasons

### `NoLegalMoves`

The current side has no legal move according to the Core.

- that engine is not called
- `stopped_side` identifies the side to move
- the runtime does not yet classify checkmate/stalemate/result semantics

### `EngineAttemptLimit`

The current side failed to produce a legal move within `max_engine_attempts_per_turn`.

- `stopped_side` identifies that side
- the canonical position remains at the last accepted position
- the opponent is not advanced automatically

A future match-policy layer may decide whether this is a loss, adapter failure, disqualification, or recoverable external-process error.

### `RepetitionDraw`

The Core detected the fourth occurrence of the same board, both hands, and side to move, and did not identify one side as the unique continuous checker.

- `stopped_side` is empty
- `losing_side` is empty
- the current single game terminates as ordinary repetition

Tournament-level replay/side-switch policy is deliberately outside this runner.

### `PerpetualCheckLoss`

The Core detected fourfold repetition and one player's every move through the repetition sequence was check.

- `losing_side` identifies the continuously checking side
- `stopped_side` is empty
- the runtime does not infer this from AI annotations; the Core derives it from canonical positions and attack detection

### `PlyLimit`

The configured safety limit was reached after accepted moves without another adjudicated result.

This is a neutral runtime safeguard. `stopped_side` and `losing_side` are empty because neither engine is treated as the cause.

## What this runner does not decide

The runtime now handles the single-game termination facts needed for ordinary repetition and continuous-check repetition, but it still does not provide a complete tournament result layer.

Still deferred:

- ordinary-repetition replay and side-switch orchestration
- resignation
- timeout loss
- external process crash/disconnect policy
- checkmate/stalemate result classification
- entering-king / impasse result policy
- opening adjudication or special tournament rules
- Dataset persistence

Those policies must consume Core/Runtime facts rather than duplicate move legality or repetition rules.

## Sibling-project alignment

This follows the shared Kadoka AI family structure:

```text
Creator / Training / Analysis
            ↓
     Engine / Adapter
            ↓
   Headless Match Runtime
            ↓
       Turn Runtime
            ↓
      Authoritative Core
```

The same design principle can be used in Othello and Tetris even though their actions and terminal rules differ:

- AI output is never allowed to mutate canonical state directly
- rejected output is diagnostic, not canonical history
- retry/transport policy belongs above rule validation
- history-dependent game rules remain in the authoritative game layer
- GUI-free matches are the standard path for league, benchmark, and Dataset generation
