# Official Training Recipe v1

This manifest records the inputs and configuration used to produce one official model generation. It is a reproducibility record; it does not run training, resolve datasets, or select a champion.

## JSON shape

The v1 document is UTF-8 JSON with these fields:

```json
{
  "schema_version": 1,
  "datasets": [
    { "dataset_id": "selfplay-2026-01", "weight": 1.0 }
  ],
  "source_weights": {
    "external": 0.0,
    "human": 0.0,
    "league": 0.0,
    "self": 1.0
  },
  "architecture_id": "policy-value-v1",
  "architecture_version": "1",
  "config_hash": "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "champion_generation": 1,
  "autotune_config_id": null,
  "evaluation_result_id": null
}
```

- `schema_version` is exactly `1`; readers reject unsupported versions.
- `datasets` is a non-empty array of dataset references and their configured multipliers. Dataset content and paths stay in the Dataset Registry.
- `source_weights` records the configured proportions for `self`, `external`, `league`, and `human`. All four keys are required.
- `architecture_id` and `architecture_version` identify the model architecture.
- `config_hash` is the caller-provided hash of the effective architecture/training configuration. v1 records it but does not prescribe how that configuration is assembled.
- `champion_generation` is a zero-based, non-negative generation number.
- `autotune_config_id` and `evaluation_result_id` are nullable references. When present, they identify the exact configuration and result used for this generation.

## Validation

A recipe is valid only when:

- required strings and references are non-empty;
- dataset IDs are unique and each dataset reference exists in the supplied Dataset Registry view;
- each dataset weight is finite and greater than zero;
- each source weight is finite and in `[0, 1]`, and the four source weights sum to one within `1e-9`;
- the schema version and generation number are supported.

Validation resolves references against caller-provided registry data. It does not open dataset paths or silently rewrite references. Duplicate IDs, unknown references, invalid weights, malformed JSON, and unsupported versions are errors.

## Deterministic serialization

Serialization emits the fields shown above in the listed order, uses UTF-8, sorts `datasets` by `dataset_id`, and writes finite numbers using a locale-independent shortest round-trippable decimal representation. Null references are emitted as JSON `null`. Deserializing and reserializing a valid recipe therefore produces the same bytes, independent of input dataset order.

The manifest stores the source weights and per-dataset multipliers as configured. Training code decides how to apply them; the serializer does not normalize or otherwise change them.
