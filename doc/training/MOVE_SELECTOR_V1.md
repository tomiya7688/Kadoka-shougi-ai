# Move Selector v1

`MoveSelector` converts scored USI move strings into one move from the caller's
legal-move list. Model scores that are non-finite, empty, or absent from the
legal list are ignored. Duplicate predictions are collapsed to their highest
score.

The reference implementations are:

- `ArgmaxSelector`: choose the highest score; ties use lexical USI order.
- `TopKSelector`: sample uniformly among the highest-scored `k` legal moves.
- `ThresholdSelector`: sample uniformly among legal moves at or above a score
  threshold.

The random selectors use a fixed-width seeded generator, so two selector
instances with the same seed and calls produce the same sequence. When no
scored candidate remains, the selector uses the requested fallback if it is
legal; otherwise it picks the first non-empty legal move. If there are no legal
moves, the result is empty. `used_fallback` distinguishes fallback results from
model-scored selections.
