# Impasse and Entering-King Adjudication

## Scope

Impasse (`持将棋`) and entering-king declaration (`入玉宣言法`) are authoritative shogi rules and therefore belong in the Core. Runtime/GUI/protocol layers may decide when a player requests a declaration or agrees to a point count, but they must consume Core adjudication rather than reimplement the rules.

The implementation follows the current Japan Shogi Association `対局規則` Article 9.

Rule reference:
https://www.shogi.or.jp/match/taikyoku_rules/

## Point values

For impasse scoring:

- rook / dragon: 5 points
- bishop / horse: 5 points
- gold / silver / knight / lance / pawn and their promoted forms: 1 point
- king: 0 points

Promoted pieces retain the point value of their original piece family.

## Core analysis

```cpp
ImpasseAnalysis analyze_impasse(const Position& position, Color color);
```

The analysis exposes:

- `total_points`: all of the side's non-king board pieces plus hand pieces
- `declaration_points`: hand pieces plus the side's non-king pieces located in the opponent camp
- `enemy_camp_piece_count`: non-king own pieces in the opponent camp
- `king_in_enemy_camp`
- `in_check`

The declaration score and ordinary 24/27-point score are deliberately separate because the official declaration rule counts only hand pieces and pieces in the opponent camp.

## Entering-king declaration

```cpp
EnteringKingDeclarationResult adjudicate_entering_king_declaration(
    const Position& position,
    Color declarer
);
```

A declaration attempt is evaluated as an actual declaration, not merely an eligibility query. Under the official rule, failure of any required condition makes the declarer lose.

The Core checks:

1. the declarer is the side to move
2. fewer than 500 moves have been completed (`Position::ply() <= 500`)
3. the declarer's king is in the opponent's three-rank camp
4. at least 10 declarer pieces other than the king are in that camp
5. the declarer's king is not in check
6. declaration points are sufficient

Point result:

- 31 points or more -> declaration win
- 24 through 30 points -> no-result / replay
- below 24 -> declaration loss

The runtime helper `entering_king_declaration_outcome()` maps these facts to the canonical `GameOutcome` contract using reason `Impasse`.

## Mutually agreed impasse

```cpp
MutualImpasseResult adjudicate_mutual_impasse_points(
    const Position& position,
    MutualImpassePolicy policy = MutualImpassePolicy::Jsa24Point
);
```

The caller must separately establish that the players agreed to use the procedure and that the position is otherwise an appropriate impasse. The Core function owns only the deterministic point calculation and point-policy result.

### JSA 24-point policy

`Jsa24Point` is the default:

- both sides at 24 points or more -> replay
- one side below 24 -> that side loses

### Optional tournament 27-point policies

The JSA rules allow tournament organizers to use a 27-point system. Two explicit policies are supported:

- `Tournament27PointReplayTie`: 27-27 is replay
- `Tournament27PointWhiteWinsTie`: 27-27 is a White (後手) win

Outside a 27-27 tie, a side meeting 27 while the opponent is below 27 wins. Material combinations that cannot be meaningfully adjudicated by the selected policy are returned as `InvalidMaterial` rather than inventing a result.

The selected policy must be included in benchmark/league configuration and dataset provenance when it affects a game.

## Automatic 500-move impasse

```cpp
Move500ImpasseStatus adjudicate_500_move_impasse(
    const std::vector<Position>& history
);
```

Under current JSA official rules, when 500 moves have been completed, the game becomes impasse and is replayed regardless of point totals.

Exception: if the position after move 500 is in check, the game continues while that checking side continues the sequence of checks. The impasse becomes effective when that checking side first makes a move that does not continue check.

The Core function always implements the JSA rule semantics. Whether a Headless match applies that automatic rule is a Runtime policy:

```cpp
limits.automatic_impasse_rule = AutomaticImpasseRule::Jsa500Moves; // default
limits.automatic_impasse_rule = AutomaticImpasseRule::Disabled;    // tournament/custom rules
```

This distinction matters because computer-shogi events and other tournaments may use their own maximum-move, declaration, or adjudication policy instead of the JSA 500-move fallback. Disabling the automatic rule does not change Core legality; it only prevents Headless Runtime from auto-ending the game at the JSA threshold.

The function consumes canonical position history. It does not infer this from AI annotations.

For correct deferred-check adjudication, the supplied history must include the exact position immediately after move 500 (`ply == 501`). A later standalone SFEN cannot prove whether a continuous-check sequence began at move 500, so the Core deliberately returns `None` rather than guessing when that threshold position is absent.

The Headless Match Runtime naturally satisfies this requirement for matches it runs through move 500 because it retains every accepted canonical position.

A completed JSA 500-move impasse maps to:

```text
GameResult::ReplayRequired
GameEndReason::Impasse
```

## Action boundary

Entering-king declaration and mutually agreed impasse are player actions/agreements, not ordinary board moves. The current `Engine::search()` contract returns only a `Move`, so the Headless Match Runtime does not automatically invent declaration or agreement actions for an AI.

The authoritative adjudication and outcome mapping are already available for a later action/protocol extension. A future engine/protocol action type can add `DeclareEnteringKing` or `AgreeImpasse` without changing the Core rule implementation.

The automatic 500-move rule requires no player action. Headless Runtime applies it only when the selected Runtime rule profile enables it.

## Dataset and league requirements

Persist enough information to reproduce an impasse result:

- `GameOutcome.result`
- `GameOutcome.reason`
- selected mutual-impasse policy, when used
- selected automatic-impasse rule/profile
- declarer color for entering-king declarations
- declaration analysis/point count when diagnostic detail is retained
- canonical move count / final SFEN
- canonical history covering move 500 when deferred-check adjudication matters

Do not collapse replay-required impasse into a scored draw.


## Runtime declaration action

The Core adjudicator does not choose to declare for an AI. Declaration is a player decision and therefore enters Runtime through the semantic AI action:

```cpp
SearchResult result;
result.action = EngineAction::DeclareEnteringKing;
```

The Headless Match Runtime then calls `adjudicate_entering_king_declaration()` for the side to move and maps the result to `GameOutcome`.

This distinction is intentional:

- the AI decides whether to invoke the declaration procedure
- the Core decides whether the declaration satisfies the official conditions
- Runtime never auto-declares merely because a winning declaration is available
- a failed declaration is immediately adjudicated as a loss, rather than retried as an illegal move

External process/script AIs use `action declare_entering_king`. Native engines use the same `EngineAction` value directly.

Resignation uses the sibling semantic action `EngineAction::Resign`; neither action is encoded as a fake board move.
