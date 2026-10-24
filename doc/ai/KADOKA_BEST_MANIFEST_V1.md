# Kadoka Best 構成manifest v1

## 目的

`kadoka.best_manifest.v1` は、Raw Model、Integrated Model、Full Engineの構成と参照先を保存するJSON形式です。同じ構成から同じ `config_hash` を計算し、AIやpackageを再現する手掛かりを残します。

この3層はKadoka内部だけで対戦する仕組みを意味しません。Raw / Integrated / Fullはいずれも、古典AI、学習AI、外部AI、OSS AI、旧世代AI、実験AIなどを含む共通AI Poolの参加者候補です。対戦登録・組合せ生成・Ratingは別のLeague契約で扱います。

## 形式

```json
{
  "format": "kadoka.best_manifest.v1",
  "layer": "full",
  "package_version": "1.0.0",
  "model": null,
  "checkpoint": null,
  "source_layer": "raw",
  "target_layer": "integrated",
  "components": [
    {"role": "evaluation", "id": "kadoka.eval", "version": "1.2.0"},
    {"role": "search", "id": "kadoka.search", "version": "3.0.1"},
    {"role": "orchestrator", "id": "kadoka.orchestrator", "version": "1.0.0"}
  ],
  "config": {"deterministic": "true", "seed": "17"},
  "config_hash": "sha256:<64 lowercase hexadecimal digits>"
}
```

| Field | Meaning |
| --- | --- |
| `format` | 固定値 `kadoka.best_manifest.v1` |
| `layer` | `raw`、`integrated`、`full` のいずれか |
| `package_version` | この構成packageの版 |
| `model` | `id` と `version` を持つmodel参照。Rawでは必須 |
| `checkpoint` | `id` と `version` を持つcheckpoint参照。指定時はmodel参照も必要 |
| `source_layer`, `target_layer` | 構成の移行元と移行先。指定するときは両方を書く |
| `components` | evaluation、search、opening、endgame、orchestratorの参照。roleごとに1件まで |
| `config` | 再現に必要な設定値。v1ではkeyとstring値のobject |
| `config_hash` | `config` の正規化JSONに対するSHA-256 |

## 検証規則

- Rawにはmodel参照が必要です。
- Integratedにはmodelまたはcomponent参照が必要です。
- Fullにはorchestratorとevaluationまたはsearchのcomponentが必要です。
- すべての参照にIDとversionが必要です。checkpointはmodelを参照する構成に含めます。
- component roleは重複できません。未知のroleやmanifest fieldは拒否します。
- source / target layerは同時に指定し、既知の3層名を使います。
- `config_hash` は、config keyを昇順に並べてJSON objectを生成し、そのUTF-8 byte列をSHA-256で計算します。空config `{}` の値は `sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a` です。

Hashは構成の再現性確認に使います。modelやpackageの内容完全性を保証する署名・trust情報ではありません。

## C++ API

`tools/kadoka_best/include/kadoka/best/manifest.hpp` がmanifest型とAPIを提供します。`refresh_config_hash()` で設定変更後にhashを更新し、`validate_manifest()` で検証し、`serialize_manifest()` / `deserialize_manifest()` でJSONとの変換を行います。不正な形式や参照は検証エラーとして拒否します。

