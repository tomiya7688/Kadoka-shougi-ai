# 再評価Priority Sampler v1

## 目的と使い方

保存済みのconfidence・uncertaintyとengine disagreementから、再評価する元サンプルを重複なく選びます。低信頼サンプルの優先、中信頼サンプルの混入、高信頼サンプルの無作為監査を、調整可能な重みで実現します。

`PrioritySamplingCandidate` は [Multi-engine Relabeling](MULTI_ENGINE_RELABELING_V1.md) の `RelabelSource` と [Confidence Scorer](CONFIDENCE_SCORER_V1.md) の `ConfidenceResult` を保持します。`sample_relabel_candidates(candidates, count, seed, policy)` が返す `candidate_index` から元候補へ戻り、`source` を再評価APIへ渡せます。抽選中に元ラベル・評価根拠は変更しません。

## 抽選policy

既定の帯と重みは次の通りです。

| 帯 | confidenceの範囲 | 基本重み |
| --- | --- | --- |
| low | 0以上0.4未満 | 8 |
| medium | 0.4以上0.8未満 | 3 |
| high audit | 0.8以上1以下 | 1 |

`low_confidence_threshold`、`high_confidence_threshold`、各帯の基本重み、`disagreement_boost` を設定できます。境界は有限な `[0,1]` 内でlow < highとし、基本重みは少なくとも1帯を正にします。基本重み0の帯は除外し、disagreementでも復活しません。

基本重みが正の候補には、整数の抽選券を次の式で割り当てます。

```text
disagreement_units = round(engine_disagreement * 1024)
tickets = band_weight * 1024 + disagreement_boost * disagreement_units
```

disagreementが未提供なら0、既定のboostは4です。量子化は最近傍への丸めで、ちょうど半分は大きい側へ丸めます。帯・boostの重みは符号なし32-bit整数です。`PrioritySampleChoice` は採用された帯と抽選券数も返すため、抽選理由を記録できます。

各回、残存する抽選券から一様に1枚選び、対応候補を取り除きます。候補の選択確率はその回の残存総和に対する抽選券数の割合です。高信頼帯の正重みは監査機会を作りますが、各batchの帯別件数を保証するquotaではありません。`count` が正重み候補数を超える場合は、その全件を返します。空入力またはcount=0の結果は空です。

## 再現性と検証

- 抽選前に `(dataset_id, dataset_revision, sample_id)` のbyte文字列順へ正規化するため、元配列の並べ替えで選択IDや抽選順は変わりません。返すindexは、その呼出しの元配列に対応します。
- 乱数生成器はC++標準の `mt19937_64`、範囲写像は剰余偏りを除く棄却法です。`std::uniform_int_distribution` の実装差には依存しません。同じ入力値・policy・seedなら同じ抽選順を再現します。
- 全候補のIDは非空とし、同一dataset/revision/sampleの重複は拒否します。異なるdatasetやrevisionの同じsample IDは別候補です。
- confidence・uncertaintyは有限な `[0,1]` 内、和は1に対して許容差 `1e-12` 以内、evidence_countは1以上とします。disagreementも有限な `[0,1]` 内です。count=0や除外帯の候補も検証します。
- 抽選券総和のuint64_t overflowは拒否します。識別子の整列はO(N log N)、抽選はFenwick treeで1件O(log N)、使用メモリはO(N)です。

## 境界

scorerの式、disagreementの生成、SFENの合法性検証、再評価の実行、計算予算、resume cursorはそれぞれの担当APIへ委譲します。Samplerは対象選択だけを担当し、入力局面を実行しません。

`kadoka_priority_sampler_test` はseedのgolden順、入力順非依存、抽選分布、disagreement加算、帯除外、重複排除、境界・非有限値の拒否、元ラベル保持を確認します。
