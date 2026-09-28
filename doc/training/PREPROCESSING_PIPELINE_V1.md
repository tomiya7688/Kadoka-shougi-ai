# Training Preprocessing Pipeline v1

## Purpose

Convert parser `ParsedRecord` values into reproducible training inputs before
dataset splitting or trainer-specific target generation. The pipeline remains a
tooling component and does not add a dependency from Runtime/Core.

## Composition

`PreprocessingPipeline` is a `ParsedRecordSink`, so a parser can write directly
into it. Each configured preprocessor receives a record and can drop it, pass
it through, or emit multiple records. Stages run in the order saved in the
configuration. Parser errors pass through unchanged.

The reference configuration runs these stages:

1. `exclude_invalid` parses the SFEN and drops records whose `side_to_move`
   disagrees with the SFEN. Dropped records increment `invalid_records`.
   It validates the position representation; downstream target generation can
   use each action's `ActionResultStatus` when selecting accepted moves.
2. `normalize_side_to_move` rotates White-to-move positions by 180 degrees,
   swaps piece colors, hands, clocks, actors and terminal winners, and
   transforms recorded moves. The result always has Black to move.
3. `expand_file_mirror` emits the record and its left-right file reflection.
   Piece colors and side to move do not change. Recorded moves are reflected
   along with the board.
4. `deduplicate` removes a repeated normalized SFEN with the same clocks and
   event sequence. Game ID, ply and source provenance do not affect this key,
   so identical examples from separate games collapse while different action
   labels for the same board remain available.

The default stage order is deterministic. Applications may save another order
or omit stages. The order matters; for example, deduplication after expansion
also removes mirrored collisions.

## Configuration

Configurations use versioned JSON and stable preprocessor IDs:

```json
{
  "schema": "kadoka.training_preprocessing_pipeline",
  "version": 1,
  "steps": [
    "exclude_invalid",
    "normalize_side_to_move",
    "expand_file_mirror",
    "deduplicate"
  ]
}
```

Unknown fields, duplicate fields, invalid JSON, and unsupported versions are
rejected. A registry supplies preprocessor factories; the reference registry
contains the four stages above and permits applications to add their own
stages.

## Summary counters

- `input_records`: records received by the pipeline
- `emitted_records`: records sent to the downstream sink
- `invalid_records`: malformed or side-inconsistent positions dropped
- `duplicate_records`: records dropped by the deduplication stage
- `expanded_records`: reflected records produced by the expansion stage
- `forwarded_errors`: parser errors forwarded to the downstream sink

Call `reset()` before reusing a pipeline for an independent dataset. It clears
the deduplication state and summary counters.

Dataset splitting, training targets, and trainer behavior are outside this
pipeline's scope.
