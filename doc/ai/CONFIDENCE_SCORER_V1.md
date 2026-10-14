# Confidence / Uncertainty Scorer v1

## 目的と利用

Scorerは、保存済みの探索・評価要素から学習サンプルのconfidenceとuncertaintyを再現可能に計算します。ConfidenceInputsは受け取った要素をそのままConfidenceResult.inputsに保持するため、計算結果を保存しても元の根拠を失いません。Scorerは対局のrating計算や、再評価対象の選択は行いません。

利用側は、利用できる指標だけを設定してDefaultConfidenceScorer::scoreを呼びます。最低1つのconfidence根拠が必要です。出典ratingだけでは信頼性の根拠にならないため、その値は結果に保存しますがconfidenceへ加算しません。

## 入力契約

| 入力 | 単位・範囲 | confidence根拠 |
| --- | --- | --- |
| source_rating | 有限値。符号は問わない | 使わず、出典情報として保持 |
| source_uncertainty | rating deviation相当、0以上 | 1 / (1 + RD / 350) |
| search_depth | ply、0以上 | depth / (depth + 8) |
| search_nodes | node数、0以上 | log1p(nodes) / log1p(1,000,000) |
| search_time_ms | ミリ秒、0以上 | log1p(time) / log1p(30,000) |
| candidate_gap_cp | centipawn差、0以上 | gap / (gap + 100) |
| policy_entropy | 最大entropyで割った正規化値 [0,1] | 1 - entropy |
| engine_disagreement | disagreementの正規化値 [0,1] | 1 - disagreement |
| result_consistency | 結果との整合度 [0,1] | その値 |

探索深さ、nodes、時間、gapは基準値を超えても1を超えないよう飽和します。全ての提供根拠を同じ重みで平均し、uncertainty = 1 - confidence とします。正規化済みの入力は利用側で算出し、Scorerは生のentropyやengine差の意味を推測しません。

NaN、無限値、負の実数、正規化範囲外はstd::invalid_argumentで拒否します。根拠がない入力（source ratingのみを含む）は同じく拒否します。全ての変換と集約は固定順・固定式で行い、同じ入力から同じ結果を返します。

## v1の基準値

RD 350、depth 8、1,000,000 nodes、30,000 ms、100 cpはv1の正規化基準です。これらは比較可能な0から1の手掛かりを得るための既定値であり、将来の学習品質を保証する閾値ではありません。変更する場合は形式・結果の変化として仕様とテストを同時に更新します。

## 完了確認

kadoka_confidence_scorer_testはgolden値、欠損指標、元入力保持、rating値非依存性、境界外と非有限値の拒否を検査します。
kadoka_confidence_scorerは通常のC++ビルドへ含まれます。
