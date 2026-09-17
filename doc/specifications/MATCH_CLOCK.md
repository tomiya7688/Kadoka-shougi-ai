# Match Clock and Time Forfeit

## Purpose

The Headless Match Runtime owns the official match clock. Search configuration and game-clock adjudication are intentionally separate concepts.

```text
SearchLimits.time_limit
    = advisory budget passed to an AI for one decision

PlayerTimeControl
    = official main time + byoyomi used to adjudicate the game
```

An engine may return after its advisory search limit. That is not automatically a game loss unless an official match clock is configured. When a match clock is configured, Runtime measures the actual AI decision duration and adjudicates time forfeit independently of engine cooperation.

## Rule basis

Japan Shogi Association rule Article 10 treats failure to complete a move within the allotted main time and byoyomi as an immediate loss.

The association also permits organizers to define detailed time-control procedures. Kadoka therefore provides the reusable clock primitive here while leaving event-specific recording conventions, pauses, and clock adjustments above the base Headless Runtime.

## Configuration

Each side can independently enable a clock:

```cpp
MatchLimits limits;
limits.black_time_control = PlayerTimeControl{
    .main_time = std::chrono::minutes{10},
    .byoyomi = std::chrono::seconds{30},
};
limits.white_time_control = PlayerTimeControl{
    .main_time = std::chrono::minutes{10},
    .byoyomi = std::chrono::seconds{30},
};
```

An empty optional means that side has no official match clock.

Different controls are allowed for testing, handicaps, adapters, or tournament simulation.

## Accounting

Only time spent inside `AIBackend::decide()` is charged to the player clock.

Core work is excluded:

- legal move generation
- move validation
- canonical `Position` transition
- repetition adjudication
- impasse adjudication

For process/script/network-style backends, transport/serialization/waiting performed inside `decide()` is part of the backend's response time and is therefore charged.

The monotonic `std::chrono::steady_clock` is used so wall-clock adjustments cannot move a player's clock backwards.

## Main time and byoyomi

For one player turn:

1. main time is consumed first.
2. once main time reaches zero, the current turn consumes byoyomi.
3. byoyomi resets when that player's next turn begins.
4. main time never replenishes.

A decision is on time when cumulative charged time for the turn is less than or equal to:

```text
remaining main time + byoyomi
```

Exceeding that allowance yields:

```text
result = opponent win
reason = TimeForfeit
winner = opponent
loser = timed-out side
```

## Illegal-output retries

Kadoka character/external engines may be retried after an illegal move output. Clock accounting does not reset between those attempts.

Example:

```text
byoyomi = 1000 ms
attempt 1 illegal: 400 ms
attempt 2 illegal: 350 ms
attempt 3 legal:   300 ms
total:            1050 ms -> time forfeit
```

This prevents retries from creating free thinking time.

Only a completed player turn resets byoyomi for that side.

## Search budget propagation

Before each engine call, Runtime computes the remaining official allowance for the current turn.

The engine receives:

```text
effective time_limit =
    min(configured SearchLimits.time_limit, remaining official allowance)
```

when both exist.

This helps cooperative engines stop before flagging while retaining Runtime as the final authority.

The official clock is measured independently, so exceeding the propagated limit can still be detected even when the engine ignores it.

## Terminal actions

Time accounting happens before semantic terminal actions are adjudicated.

Therefore an engine that returns `Resign` or `DeclareEnteringKing` only after its clock has expired loses by `TimeForfeit`; the late action does not override the clock result.

For a timely entering-king declaration, Runtime then invokes the Core declaration adjudicator.

## Result diagnostics

`MatchResult::clock` records:

- remaining Black main time when enabled
- remaining White main time when enabled
- measured total Black AI decision time
- measured total White AI decision time

This is suitable for dataset provenance, benchmark diagnostics, and later GUI clock display.

## Scope and deferred policies

This first clock contract supports continuous-duration main time plus per-move byoyomi.

Still policy-specific and deferred:

- organizer-specific whole-minute accounting conventions
- Fischer increment / delay clocks
- adjournments and pauses
- arbiter clock corrections
- transport failure vs timeout classification
- replay-game clock inheritance after repetition/impasse
- GUI clock presentation

Those should build on the clock state rather than altering Core game rules.

## Rule reference

Japan Shogi Association, 対局規則, Article 10:
https://www.shogi.or.jp/match/taikyoku_rules/
