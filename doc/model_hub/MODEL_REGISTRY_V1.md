# Model Registry v1

Issue #10 のローカル登録APIは `kadoka/model_registry/model_registry.hpp` にあります。
bundled、downloaded、user_trained、externalのmanifestを同じ形式で保存します。
download、学習テンプレート、GUI、実行sandboxはこのAPIには含みません。

## manifest

`schema` は `kadoka.model_manifest.v1` 固定です。
[JSON例](../../models/examples/model_manifest_v1.json) を参照してください。

| フィールド | 内容 |
| --- | --- |
| `model_id` / `model_version` | 登録key。IDとversionの組を一意にします |
| `model_type` | 例: `evaluation_search` |
| `artifact_format` / `artifact_format_version` | 重み・book等の実際の配布形式と版 |
| `api_version` | 実行APIの版 |
| `artifact_sha256` | 配布物のSHA-256、64桁小文字hex |
| `source_kind` / `source_reference` | 出典の種類と識別子。`source_kind` は `bundled`、`downloaded`、`user_trained`、`external` のいずれか |
| `license` | 配布物の利用ライセンス |
| `runtime_id` | 必要なruntime |
| `min_memory_mb` / `min_threads` | 実行に必要な最小資源 |

ID、version、type、artifact format、runtime IDはASCII英数字で始まり、
以降はASCII英数字、dot、underscore、hyphenのみを許可します。
長さは1～128文字、連続dotは許可しません。これらの制約は保存ファイル名の安全性にも使います。
JSON parserは14フィールドを必須とし、重複、未知フィールド、型違い、不正なUTF-8、余剰文字を拒否します。
source referenceとlicenseは各4096 byte以下で、NUL文字を許可しません。

既存の [`kadoka.ai_metadata.v1`](../specifications/AI_MODEL_METADATA.md) は兄弟プロジェクトと共有する説明用メタデータです。
`kadoka.model_manifest.v1` はローカル登録と実行互換性を判断する追加契約です。
同じモデルを両方で表す場合、model ID・version・source・licenseを一致させてください。
`schema` はmanifest形式、`artifact_format` はモデル配布物の形式を表します。

## ローカル登録

`LocalModelRegistry(directory)` が保存先を作成します。
`add(manifest)` は同一ID・versionの重複を拒否し、一時ファイルに書いた後に確定名へrenameします。
`get(id, version)` は指定版、`list()` は全版をID・version順に返します。
`remove(id, version)` は指定版だけを削除し、削除した場合にtrueを返します。
別のプロセスでregistryを開き直しても保存済みmanifestを再読込できます。
破損したJSONとファイル名・内部IDの不一致はエラーとして通知します。

registry保存先はアプリケーション専用ディレクトリとし、同時writerは1つに限定してください。
一時ファイルは未確定として列挙時に無視します。前回の異常終了で同じ版の一時ファイルが残った場合は破棄して登録を再開します。OS・ストレージの電源断耐久性を保証するfsyncはありません。
このAPIは`artifact_sha256`の形式を検証します。モデル配布物の内容hash照合は
[Issue #40](https://github.com/tomiya7688/Kadoka-shougi-ai/issues/40)のdownload・install側で行います。

## 互換性判定

`check_compatibility(manifest, target)` はAPI版、artifact形式と版、runtime ID、
メモリMiB、スレッド数を順に照合します。異なる値は最初の不一致項目を返します。
未知の形式は使用可能と判定しません。実際のruntimeロードや安全性検査は別の境界です。

`kadoka_model_registry_test` はJSON往復、非互換形式・API、重複版、再読込、
削除、破損・不正入力の拒否を検査します。
