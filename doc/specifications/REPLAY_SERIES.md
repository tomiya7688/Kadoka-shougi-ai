# Replay Series Orchestrator

## Purpose

A shogi contest does not necessarily finish when one board reaches a no-result.

Under the Japan Shogi Association rules:

- ordinary repetition (千日手) is replayed with Black/White swapped
- no-result impasse (持将棋) is replayed with Black/White swapped
- a replay that again becomes repetition or no-result is replayed again
- repetition/no-result games are normally not counted as completed games; the contest completes when a replay is decided
- replay-game time control is defined by the individual event or tournament rules

The Replay Series Orchestrator turns the existing one-board Headless Match Runtime into that larger logical contest.

Rule reference:
https://www.shogi.or.jp/match/taikyoku_rules/

## API

```cpp
ReplaySeriesResult run_replay_series(
    Engine& player_a,
    Engine& player_b,
    const Position& initial_position,
    const ReplaySeriesLimits& limits = {}
);
```

Player A is Black in the first game and Player B is White.

When a game returns `GameResult::ReplayRequired`, the next game:

1. starts from `Position::startpos()`
2. swaps the two participants' colors
3. carries each participant's search configuration with that participant
4. applies the configured replay-clock policy
5. runs through the same authoritative Headless Match Runtime

The Series layer never reimplements legal moves, repetition, impasse, declaration, time-forfeit, or terminal adjudication.

## Participant identity vs board color

A replay swaps colors, so a final `BlackWin` / `WhiteWin` is not sufficient to identify the logical contest winner.

The Series layer uses:

```cpp
enum class SeriesPlayer {
    A,
    B,
};
```

Every `ReplayGameRecord` stores which participant was Black and White in that game.

The final Series result maps the deciding board color back to the participant:

```text
game 1: A = Black, B = White
        -> ReplayRequired

game 2: B = Black, A = White
        -> WhiteWin

series winner = A
```

This mapping is important for league ratings, dataset attribution, and reproducible benchmarks.

## Search configuration

`MatchLimits::black_search` is treated as Player A's configured search limits for the first game.

`MatchLimits::white_search` is treated as Player B's configured search limits for the first game.

On a replay, the configurations follow the participants rather than remaining attached to the color.

This allows different engines to have different node/depth/time search settings without silently exchanging those settings after a replay.

## Replay clock policy

The official rules deliberately leave replay-game time control to each event's implementation rules.

Kadoka therefore makes the policy explicit.

### `ResetConfigured`

Each replay starts with the participant's originally configured:

- main time
- byoyomi

This is the default generic Runtime policy.

### `CarryRemainingMain`

Each participant keeps the remaining main time from the previous no-result game.

The participant's configured byoyomi remains unchanged and starts fresh per move in the replay.

Remaining time follows the participant across the Black/White swap.

Example:

```text
game 1:
  A Black: 60 min -> 42 min remaining
  B White: 60 min -> 38 min remaining

replay:
  B Black: 38 min
  A White: 42 min
```

This is a reproducible policy option, not a claim that every official event uses it.

Tournament-specific rules that need another clock formula should add another explicit policy rather than silently changing these semantics.

## Replay limit

Official rules can require repeated replay, while tournaments may define special handling after multiple no-results.

Runtime also needs a safety bound for automated leagues.

```cpp
limits.max_replays = 16;
```

The value counts replays after the first game.

For example:

- `0`: run the first game only; a replay request ends with `ReplayLimit`
- `1`: permit at most two board games total
- `16`: permit at most seventeen board games total

When the limit is reached:

```text
ReplaySeriesStatus::ReplayLimit
final_outcome.result == GameResult::ReplayRequired
```

No winner or loser is invented.

## Series status

```cpp
enum class ReplaySeriesStatus {
    Decided,
    Draw,
    Unresolved,
    ReplayLimit,
};
```

### `Decided`

The final board game produced Black or White win.

`winner` and `loser` identify Player A/B, not colors.

### `Draw`

The final board game was genuinely scored as a completed draw.

Current normal JSA repetition/impasse paths do not use this; they use `ReplayRequired`.

### `Unresolved`

The board runtime stopped without an official game result, for example an engine-attempt or safety-ply limit.

The Series does not convert an infrastructure/runtime stop into a replay or loss.

### `ReplayLimit`

The last game requires replay but the configured automated replay bound has been reached.

## Game records

The result retains every board-game attempt:

```cpp
struct ReplayGameRecord {
    std::uint32_t game_number;
    SeriesPlayer black_player;
    SeriesPlayer white_player;
    MatchResult match;
};
```

This preserves:

- the exact result/reason of each no-result
- accepted moves and final position
- illegal-output counts
- clock snapshots
- color assignment
- replay count

Dataset and league tooling should not flatten this into a single final result without retaining the replay records or their provenance.

## Initial position

The caller may provide a custom initial position for the first game, which is useful for tests, analysis, or resumed workflows.

A `ReplayRequired` result always restarts the next game from the standard initial position because the shogi replay rule returns the board to the initial arrangement.

## Engine lifecycle

The same Engine objects are reused across replay games.

Engines must therefore treat each incoming `Position` as authoritative and must not assume that a new call continues from their previous board state.

This is already a project-wide engine-boundary requirement. Stateful adapters may keep caches or model state, but canonical board state always comes from Runtime.

## Layering

```text
AI A / AI B
    ↓
Replay Series Orchestrator
    ↓
Headless Match Runtime
    ↓
Turn Runner / Match Clock
    ↓
Authoritative Core
```

The Series layer owns only:

- color swapping
- replay-loop control
- participant identity mapping
- replay clock policy
- replay provenance

It does not own shogi rule adjudication.
