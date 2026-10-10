# AIモデルメタデータ

## この文書の目的

`kadoka.ai_metadata.v1` は、Kadoka Shougi AI、Kadoka Othello AI、Kadoka Tetris AIで共有するモデル・AIの横断メタデータ形式です。Model Hub、AI作成ツール、学習、ベンチマークが、ゲーム固有のモデル形式を読み解かずに識別情報や出典を扱えるようにします。

これは実行用のruntime/package manifest、重み、探索設定を置き換えません。ゲームごとの実行形式は各プロジェクトで決めます。

## 必須項目

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

| 項目 | 意味 |
| --- | --- |
| `format` | `kadoka.ai_metadata.v1` 固定 |
| `model_id` | 安定した機械可読ID |
| `model_name` | 表示名 |
| `model_version` | モデルまたはpackageの版 |
| `architecture` | AI方式の大分類 |
| `game` | `shogi`、`othello`、`tetris` のいずれか |
| `license` | ライセンス文字列。技術モデルとキャラクター等の条件が異なる場合はobjectで分ける |

## 共通optional項目

以下の項目を必要に応じて記録します。各兄弟repoで共通名の意味を変えず、未知のゲーム固有項目は許可します。

- `format_version`, `variant`
- `runtime_requirements`, `search_config`
- `training_recipe`, `dataset_provenance`
- `determinism`, `benchmark_results`
- `source`, `distribution`, `created_at`

metadataには重み、dataset、secret、端末固有の絶対パスを入れません。外部モデルはsource、hash、license、再配布条件を残します。benchmarkは比較条件を再現できる設定・hardware情報を含めます。

## 既存manifestとの関係

metadataはモデルの識別、由来、要求環境、再現性、license、benchmarkを記録します。AI package/runtime manifestは実行時に何をどうloadするかを定義します。両者を混同せず、同じ情報を重複管理する場合はIDとversionの一致を確認します。

将棋側のModel Registry manifestはIssue #10で別途定義します。この共通metadataはその候補packageの横断情報を受け持ち、downloadやRegistry APIは実装しません。

## 兄弟repoの対応表

| Project | 仕様書 | 例・validator | game固有manifestとの関係 |
| --- | --- | --- | --- |
| Shougi | [`AI_MODEL_METADATA.md`](AI_MODEL_METADATA.md) | [`family_metadata.json`](../../models/examples/family_metadata.json)、[`validate_metadata.py`](../../tools/family_metadata/script/validate_metadata.py) | runtime/package manifestとは独立 |
| Othello | [`family-model-metadata.md`](https://github.com/tomiya7688/Kadoka-othello-AI/blob/main/doc/family-model-metadata.md) | packageの`metadata.json`、共通validator | `manifest.json`からmetadataを参照。binary形式はゲーム固有 |
| Tetris | [`AIモデルメタデータ.md`](https://github.com/tomiya7688/kadoka_tetris_ai/blob/main/docs/AI%E3%83%A2%E3%83%87%E3%83%AB%E3%83%A1%E3%82%BF%E3%83%87%E3%83%BC%E3%82%BF.md) | [`family_metadata.json`](https://github.com/tomiya7688/kadoka_tetris_ai/blob/main/models/examples/family_metadata.json)、共通validator | Python設定と将来のC++ runtime形式から独立 |

3 repoの仕様とvalidatorを照合し、必須項目、format identifier、対応game値が一致することを確認しています。Othello validatorは加えてbenchmark数値の有限性を検査します。

## 検証例

将棋の例は[`family_metadata.json`](../../models/examples/family_metadata.json)です。CIのmetadata validatorで`game=shogi`を指定して検査します。

## 英語版

[AI Model Metadata](AI_MODEL_METADATA.md)

