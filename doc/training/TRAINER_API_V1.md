# Trainer API v1

The training API accepts `TrainingExample` values with an input representation
and target label. A `Trainer` implementation provides `train` and `evaluate`,
and reports its stable implementation ID. Training can emit per-epoch progress
and checkpoint snapshots through optional callbacks.

`TrainingConfig` carries a seed, epoch count, and checkpoint interval. Zero
epochs and a zero interval are rejected. Checkpoint callbacks run at each
configured interval and always on the final epoch. Callback exceptions are
propagated to the caller.

`ReferenceTrainer` is a small deterministic baseline for API integration. It
memorizes the most frequent target for each exact input, breaking ties by
lexicographically smaller target. Its JSON checkpoint records those counts;
this reference model is intended for contract tests and examples, not general
shogi strength. Production trainers can implement the same interface without
changing dataset parsing or splitting.
