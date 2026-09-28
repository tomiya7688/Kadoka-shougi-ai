# Dataset Splitter v1

`split_training_records` partitions preprocessed records into train, validation,
and test sets. Records are grouped by `game_id` before shuffling, so every
position from a game stays in one split. Missing or empty game IDs are rejected
because record-level fallback grouping cannot guarantee leakage prevention.

`DatasetSplitConfig` supplies a reproducible 64-bit seed and three finite ratios
in `[0, 1]` whose sum must be one. Game IDs are sorted before the seeded shuffle;
split sizes use the largest-remainder method (ties follow train, validation,
test order). The input record order is preserved within each output split.

`serialize_dataset_split_manifest` emits versioned JSON containing the seed,
ratios, and assigned game IDs. Persist this alongside generated datasets to
recreate the exact assignment. The manifest intentionally records IDs rather
than source paths or record contents.
