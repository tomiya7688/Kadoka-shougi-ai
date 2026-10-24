# AI Model Metadata

Japanese canonical specification: [AIモデルメタデータ](AI%E3%83%A2%E3%83%87%E3%83%AB%E3%83%A1%E3%82%BF%E3%83%87%E3%83%BC%E3%82%BF.md).

Kadoka Shougi AI adopts the sibling-project metadata format `kadoka.ai_metadata.v1` shared with Kadoka Othello AI and Kadoka Tetris AI.

This is a cross-project **metadata** contract, not the shogi runtime model binary format. Shogi-specific search, evaluation, NN, opening-book, or training assets remain free to use formats appropriate for shogi.

## Required fields

```json
{
  "format": "kadoka.ai_metadata.v1",
  "model_id": "kadoka.shogi.example",
  "model_name": "Kadoka Shogi Example",
  "model_version": "0.1.0",
  "architecture": "evaluation_search",
  "game": "shogi",
  "license": "MIT"
}
```

Required meanings:

- `format`: exactly `kadoka.ai_metadata.v1`
- `model_id`: stable machine-readable identity
- `model_name`: display name
- `model_version`: model/package version
- `architecture`: high-level model family
- `game`: `shogi`
- `license`: string or object. Use an object when technical/model/character licensing differs.

## Shared optional fields

- `format_version`
- `variant` (`base`, `pretrained`, `user_trained`, `external_base`, `character`, ...)
- `runtime_requirements`
- `search_config`
- `training_recipe`
- `dataset_provenance`
- `determinism`
- `benchmark_results`
- `source`
- `distribution`
- `created_at`

These names have the same meaning in all three sibling projects. Unknown fields are allowed for shogi-specific metadata.

## Relation to Issue #10

This format provides the common metadata layer for bundled, downloaded, user-trained, and external-base models described by #10. It does not by itself implement download management or training templates.

Recommended `distribution` examples:

```json
{"kind":"bundled"}
```

```json
{
  "kind":"external",
  "sha256":"...",
  "source_url":"...",
  "redistribution":"upstream-only"
}
```

Recommended provenance:

```json
"dataset_provenance": [
  {
    "dataset_id": "...",
    "recipe_id": "...",
    "sha256": "..."
  }
]
```

## Rules

1. Runtime/package manifests remain authoritative for execution.
2. Metadata must not contain large weights, datasets, secrets, or machine-local absolute paths.
3. Model identity/version should match the runtime package when both exist.
4. Benchmark entries need enough configuration/hardware context to avoid invalid comparisons.
5. External model source, hash, license, and redistribution constraints should be retained.
6. Character licensing must remain distinguishable from technical model/code licensing.

See `models/examples/family_metadata.json` for a validated example.


## Sibling project implementation map

| Project | Specification | Example and validator | Game-specific package relation |
| --- | --- | --- | --- |
| Shougi | `doc/specifications/AIモデルメタデータ.md` | `models/examples/family_metadata.json`; `tools/family_metadata/script/validate_metadata.py` | Metadata is separate from runtime/package manifests. |
| Othello | `doc/family-model-metadata.md` | Package `metadata.json` files; `tools/family_metadata/script/validate_metadata.py` | Package `manifest.json` refers to metadata; model binaries remain game-specific. |
| Tetris | `docs/AIモデルメタデータ.md` | `models/examples/family_metadata.json`; `tools/family_metadata/script/validate_metadata.py` | Metadata remains independent of Python settings and future C++ runtime formats. |

The three validators use the same format identifier, required identity fields, and supported game values. Othello additionally checks benchmark numbers for finite values. Each game keeps its own runtime/package manifest and model format; this metadata contract does not replace them.
