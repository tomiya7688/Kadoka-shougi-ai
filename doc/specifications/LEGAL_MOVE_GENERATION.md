# Legal Move Generation

## Scope

Legal move generation is layered on top of pseudo-legal move generation.

The pseudo-legal layer owns piece movement, occupancy, promotion choices, mandatory promotion, drops, dead-rank restrictions, and nifu.

The legal layer owns king safety. A pseudo-legal move is legal only when applying it leaves the moving side's king not in check.

## Public API

```cpp
bool is_square_attacked(const Position& position, Square square, Color by_color);
bool is_in_check(const Position& position, Color color);
std::vector<Move> generate_legal_moves(const Position& position);
Position Position::after_move(const Move& move) const;
```

`after_move()` is intentionally immutable/copy-based at this stage. Correctness and a simple contract are preferred over search speed. A later make/unmake implementation may optimize internals without changing callers that only need a derived position.

## Required behavior

- attack detection handles every normal and promoted piece
- sliding attacks stop at the first occupied square
- king moves into attacked squares are rejected by legal filtering
- moves that expose the moving king to a slider are rejected
- moves that resolve check are retained
- captures add the unpromoted captured piece to the mover's hand
- drops consume one piece from the mover's hand
- side-to-move and ply advance after move application

## Explicitly deferred

Pawn-drop mate (`uchi-fuzume`) is not part of this slice. Repetition and perpetual-check adjudication are also deferred.

## Regression strategy

Prefer compact SFEN positions that isolate one rule. Tests should state the exact move expected to be present or absent. Avoid large scenario fixtures when a 2-4 piece position proves the same rule.
