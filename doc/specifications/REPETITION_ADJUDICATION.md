# Repetition Adjudication

## Scope

Repetition is an authoritative shogi-rule decision, but it is not a property of one move in isolation. It therefore lives in the Core as history adjudication rather than inside pseudo-legal/legal move generation.

The implementation follows the Japan Shogi Association rule definition:

- an identical position is the same board arrangement, both players' hands, and side to move
- the fourth occurrence establishes repetition
- if, throughout the sequence bounded by those occurrences, all moves by one player are checks, the continuously checking player loses
- otherwise the repetition is treated as ordinary repetition/draw at the single-game runtime level

The move/ply number is not part of repetition identity.

## API

```cpp
RepetitionResult adjudicate_repetition(std::span<const Position> history);
```

`history` is chronological and contains:

1. the initial canonical position
2. every canonical position after an accepted legal move

Rejected AI intentions must never be inserted into this history.

The current implementation adjudicates the latest position. Callers should invoke it after each accepted move and stop or otherwise apply match policy when the result is not `None`.

## Results

### `None`

The latest canonical position has occurred fewer than four times.

### `Draw`

The latest position is the fourth occurrence and neither player is the unique continuous checker through the repetition sequence.

At tournament level, ordinary shogi repetition may lead to a replay with sides exchanged. The Core result here describes the termination of the current game only; series/tournament replay policy belongs above the game runtime.

### `BlackLosesPerpetualCheck`

Black made a checking move on every Black move in the four-occurrence sequence. Black is the losing side.

### `WhiteLosesPerpetualCheck`

White made a checking move on every White move in the four-occurrence sequence. White is the losing side.

## Position identity

A repetition comparison uses:

```text
board piece types/colors
+ Black hand
+ White hand
+ side to move
```

It intentionally ignores `Position::ply()`.

This correctness-first implementation compares canonical state directly. A future Zobrist/repetition hash may accelerate lookup, but hash equality must remain an optimization of this semantic rule rather than redefine it.

## Continuous-check detection

For each post-move position in the repetition interval:

- the mover is the opposite of `side_to_move()`
- if the mover is the candidate continuous checker, the new side to move must be in check
- one non-checking move by that candidate makes the sequence ordinary repetition rather than a perpetual-check loss for that player

The implementation derives check status from the authoritative attack/check functions instead of trusting AI metadata or move annotations.

## Runtime integration

The Headless Match Runtime stores canonical position history and asks the Core for repetition adjudication after every accepted move.

Mapping:

```text
Core None                         -> continue
Core Draw                         -> MatchEndReason::RepetitionDraw
Core Black/White loses by checks  -> MatchEndReason::PerpetualCheckLoss
```

For perpetual check, `MatchResult::losing_side` identifies the checking side. `stopped_side` remains reserved for operational termination such as no legal move or exhausted engine attempts.

## Regression requirements

Tests must cover at least:

- fewer than four identical positions -> `None`
- fourfold quiet repetition -> `Draw`
- Black continuous-check repetition -> Black loses
- White continuous-check repetition -> White loses
- same board/turn but different hand state -> not identical
- Headless Runtime termination for ordinary repetition
- Headless Runtime termination with the correct perpetual-check losing side
