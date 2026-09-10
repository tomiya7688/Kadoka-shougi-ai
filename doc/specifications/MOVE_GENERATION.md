# Move Generation Specification

## Purpose

Move generation is intentionally split into two layers so rule correctness can be tested independently from king-safety filtering.

## Pseudo-legal layer

`generate_pseudo_legal_moves(const Position&)` is responsible for:

- movement of all normal and promoted pieces
- board boundaries
- blocking for sliding pieces
- rejecting destinations occupied by the moving side
- captures by movement onto an opponent piece
- optional promotion when either endpoint is in the promotion zone
- mandatory promotion for pawn/lance on the final rank and knight on the final two ranks
- drops from hand onto empty squares
- rejecting pawn/lance drops on the final rank
- rejecting knight drops on the final two ranks
- rejecting nifu (an unpromoted pawn already exists on the same file)

It is intentionally NOT responsible for:

- rejecting moves that leave the moving side's king in check
- rejecting king moves into check
- pawn-drop mate (`uchi-fuzume`)
- repetition or perpetual-check adjudication

Those require attack/check or game-history information and belong to later legal-move/adjudication layers.

## Promotion zones

With the internal rank convention used by `Square`:

- Black promotion zone: ranks 1 through 3
- White promotion zone: ranks 7 through 9

A promotable board move may promote when either its source or destination is in the moving side's promotion zone.

Promotable types are pawn, lance, knight, silver, bishop, and rook. Gold and king never promote. Already promoted pieces never promote again.

## Drops

Only pawn, lance, knight, silver, gold, bishop, and rook may exist in hand and be dropped.

Pseudo-legal generation enforces restrictions that can be determined from the current board alone. Pawn-drop mate is deferred until legal move generation because it depends on check and opponent-response analysis.

## Testing guidance

Prefer minimal SFEN positions that isolate one rule. Keep each regression position small enough that the expected move count or exact target squares can be understood without a full game record.
