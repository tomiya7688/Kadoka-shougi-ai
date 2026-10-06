# Official Training Recipe v1

This manifest records the inputs and configuration used to produce one official model generation. It is a reproducibility record; it does not run training, resolve datasets, or select a champion.

## JSON shape

The v1 document is UTF-8 JSON with these fields:

```json
{
  "schema_version": 1,
  "datasets": [
    { "dataset_id": "selfplay-2026-01", "dataset_revision": "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "weight": 1.0 }
  ],
  "source_weights": {
    "external": 0.0,
    "human": 0.0,
    "league": 0.0,
    "self": 1.0
  },
  "architecture_id": "policy-value-v1",
  "architecture_version": "1",
  "effective_config": {
    "batch_size": 32,
    "learning_rate": 0.01,
    "optimizer": "adam"
  },
  "config_hash": "sha256:2212b4020bc7b36774309460140f000ce55fc9b7b004c1e968185579c9b355c6",
  "champion_generation": 1,
  "autotune_config_id": null,
  "evaluation_result_id": null
}
```

- `schema_version` is exactly `1`; readers reject unsupported versions.
- `datasets` is a non-empty array. Every entry has a `dataset_id`, an immutable non-empty `dataset_revision`, and a finite positive `weight`. The revision is a content digest or immutable registry snapshot ID and must never be retargeted.
- `source_weights` records the configured proportions for `self`, `external`, `league`, and `human`. All four keys are required.
- `architecture_id` and `architecture_version` identify the model architecture.
- `effective_config` is the canonical, effective architecture/training configuration object stored in the manifest. It contains the values needed to reproduce the generation.
- `config_hash` is `sha256:` followed by the lowercase SHA-256 digest of the UTF-8 bytes of canonical `effective_config`.
- `champion_generation` is an unsigned 64-bit integer in the inclusive range `0` to `18446744073709551615`. Fractions, exponents, negative values, and values above the bound are rejected.
- `autotune_config_id` and `evaluation_result_id` are nullable references. When present, they identify the exact configuration and result used for this generation.

## Validation

A recipe is valid only when:

- required strings and references are non-empty;
- dataset IDs are unique and every `(dataset_id, dataset_revision)` pair exists in the supplied Dataset Registry view;
- each revision is pinned and each dataset weight is finite and greater than zero;
- `effective_config` is a canonical JSON object and `config_hash` matches its canonical UTF-8 bytes;
- each source weight is finite and in `[0, 1]`, and the four source weights sum to one within `1e-9`;
- `champion_generation` is an unsigned 64-bit integer in the documented range; fractions, exponents, negatives, and overflow are rejected;
- the schema version is supported.

Validation resolves references against caller-provided registry data. It does not open dataset paths or silently rewrite references. Duplicate IDs, unknown references, invalid weights, malformed JSON, and unsupported versions are errors.

## Deterministic serialization

The v1 writer emits compact UTF-8 JSON with no byte-order mark, insignificant whitespace, or trailing newline. Property order is fixed:

- top-level: `schema_version`, `datasets`, `source_weights`, `architecture_id`, `architecture_version`, `effective_config`, `config_hash`, `champion_generation`, `autotune_config_id`, `evaluation_result_id`;
- dataset entry: `dataset_id`, `dataset_revision`, `weight`;
- source weights: `external`, `human`, `league`, `self`;
- object properties inside `effective_config`: ascending UTF-8 byte order; array elements retain their input order.

Strings use UTF-8. The writer escapes quotation marks as `\\"`, backslashes as `\\\\`, and the control characters backspace, form feed, newline, carriage return, and tab as `\\b`, `\\f`, `\\n`, `\\r`, and `\\t`. Other control bytes use lowercase `\\u00hh`. Slash and non-ASCII UTF-8 bytes are emitted without escaping. Null is `null`; booleans are lowercase. Dataset weights and source weights use a locale-independent shortest round-trippable decimal representation. Integer fields use unsigned decimal with no sign, fraction, or exponent.

`effective_config` must itself use compact JSON, sorted object keys, and the string escaping above. Its numeric token spelling is retained as part of the exact configuration bytes. Serialization rejects a non-canonical in-memory configuration string. The `config_hash` is SHA-256 over those exact canonical UTF-8 bytes.

The manifest stores source weights and per-dataset multipliers as configured. Training code decides how to apply them; the serializer does not normalize or otherwise change them.
