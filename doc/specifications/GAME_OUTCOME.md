# Game Outcome Contract

## Purpose

Kadoka Shougi AI uses one explicit match-outcome value for headless play, future GUI play, league evaluation, benchmarks, and dataset provenance.

A result and the reason for that result are different facts and must not be collapsed into one enum.

```cpp
struct GameOutcome {
    GameResult result;
    GameEndReason reason;
    std::optional<Color> winner;
    std::optional<Color> loser;
};
```

This avoids ambiguous records such as `loss` without knowing whether the cause was checkmate, perpetual check, timeout, resignation, or impasse adjudication.

## Result values

### `BlackWin` / `WhiteWin`

The game has an identified winner and loser.

`winner` and `loser` must both be present and must be opposite colors.

### `Draw`

The completed game is scored as a draw by the applicable match policy.

No current core rule emits this value yet. It is reserved for rules or tournament policies that genuinely score a completed game as a draw.

### `ReplayRequired`

The current game does not produce a winner and the applicable shogi rule requires a restart/replay.

Ordinary fourfold repetition and replay-result impasse map here. This is intentionally not called `Draw`: under the Japan Shogi Association rules, these cases normally cause a replay and are not treated as a completed scored draw.

### `Unresolved`

The runtime stopped without an official shogi winner/draw/replay result.

Examples currently include:

- engine attempt limit
- safety ply limit
- a malformed/non-standard position with no legal moves but no checkmate fact

Runtime/tooling policy may later convert some of these into a forfeit, retry, adapter error, or discarded sample, but the base runtime does not invent a game result.

## End reasons

`GameEndReason` currently defines:

- `Checkmate`
- `Repetition`
- `PerpetualCheckViolation`
- `Resignation`
- `TimeForfeit`
- `Impasse`
- `NoLegalMoves`
- `EngineAttemptLimit`
- `PlyLimit`

`Impasse` now covers entering-king declaration, mutually agreed point adjudication, and the automatic 500-move impasse rule. Detailed impasse facts should be preserved separately when needed for dataset or tournament diagnostics.

`Resignation` is emitted when an AI returns `EngineAction::Resign`. `TimeForfeit` is emitted by the Headless Match Runtime when a configured official match clock is exceeded.

## Current mappings

| Runtime/Core fact | Result | Reason | Winner/loser |
| --- | --- | --- | --- |
| side to move is checkmated | opponent win | `Checkmate` | present |
| ordinary fourfold repetition | replay required | `Repetition` | absent |
| Black continuous-check repetition | White win | `PerpetualCheckViolation` | White / Black |
| White continuous-check repetition | Black win | `PerpetualCheckViolation` | Black / White |
| valid entering-king declaration with 31+ points | declarer win | `Impasse` | present |
| valid entering-king declaration with 24-30 points | replay required | `Impasse` | absent |
| failed entering-king declaration | opponent win | `Impasse` | present |
| explicitly accepted 24-point impasse, both 24+ | replay required | `Impasse` | absent |
| explicitly accepted point adjudication, one side below threshold | opponent win | `Impasse` | present |
| automatic 500-move impasse | replay required | `Impasse` | absent |
| engine resignation | opponent win | `Resignation` | present |
| official match clock exceeded | opponent win | `TimeForfeit` | present |
| engine fails retry policy | unresolved | `EngineAttemptLimit` | absent |
| safety ply guard reached | unresolved | `PlyLimit` | absent |
| no legal move without checkmate fact | unresolved | `NoLegalMoves` | absent |

## Core vs Runtime responsibility

Single-position and history-dependent rule facts belong to the authoritative Core.

Core adjudicators include:

- `adjudicate_terminal_position()` for checkmate/no-legal-move facts
- `adjudicate_repetition()` for fourfold repetition and continuous-check repetition
- `adjudicate_entering_king_declaration()` for declaration attempts
- `adjudicate_mutual_impasse_points()` for 24/27-point material adjudication
- `adjudicate_500_move_impasse()` for the automatic 500-move rule

The Runtime combines authoritative rule facts with execution facts such as engine retry exhaustion or a safety ply limit and produces a `GameOutcome`.

```text
Position / position history
        ↓
Authoritative Core adjudication
        ↓
Runtime execution policy
        ↓
GameOutcome
        ↓
League / Dataset / GUI / Benchmark
```

## Checkmate

The Japan Shogi Association defines the objective as checkmating the opponent king and states that the game ends at checkmate. The runtime therefore converts a no-legal-move result into a win only when the Core also confirms that the side to move is in check.

A synthetic or malformed position that has no legal moves but is not in check is not silently awarded to the opponent.

## Repetition

Ordinary fourfold repetition uses `ReplayRequired + Repetition`.

Continuous-check repetition uses a normal win/loss result with `PerpetualCheckViolation`, because the continuously checking player loses under the rules.

See `REPETITION_ADJUDICATION.md` for position-history semantics.

## Impasse

Impasse uses the same result/reason separation:

- a successful 31+ entering-king declaration is a win
- a 24-30 declaration is replay-required
- an invalid declaration attempt is a loss by the declarer
- mutually agreed 24-point impasse can be replay-required or a point-threshold loss
- optional 27-point tournament policy can use replay or White-win treatment for a 27-27 tie
- the automatic 500-move rule is replay-required and ignores point totals

See `IMPASSE_ADJUDICATION.md` for point scope, declaration conditions, and the 500-move continuous-check exception.

## Dataset and league rules

Persist both `result` and `reason`.

Do not reduce all non-wins to a single `draw` flag. In particular:

- repetition replay must remain distinguishable from a scored draw
- impasse replay must remain distinguishable from a scored draw
- infrastructure stops must remain distinguishable from rule results
- perpetual-check losses must remain distinguishable from checkmate losses
- declaration losses should remain distinguishable from checkmate losses through impasse diagnostics
- timeout and resignation outcomes must retain their reason

When tournament-selectable impasse rules are used, persist the selected policy as match configuration/provenance.

This distinction is required for trustworthy training filters, benchmark statistics, rating policy, debugging, and model provenance.

## Sibling-project alignment

Othello and Tetris do not need the same C++ enum names or game-specific reasons, but the Kadoka AI family should align on the conceptual schema:

```text
result + reason + winner/loser (when known)
```

Dataset, league, and Model Hub formats should preserve this separation across games.

## Rule reference

Japan Shogi Association, `対局規則`:
https://www.shogi.or.jp/match/taikyoku_rules/
