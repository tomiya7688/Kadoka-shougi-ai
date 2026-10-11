# AIリーグRating Store v1

## 目的

`RatingStore` は `ParticipantRegistry` に登録済みの参加者を初期Ratingへ登録し、確定済みの1局結果から両者のRatingと不確実性を更新します。対局の実行、結果の収集、Scheduler、対局結果Storeは担当しません。呼び出し側は確定した先手・後手と勝敗を `record_result()` に渡します。

## 更新方式

v1は1局ずつ更新するGlicko-1形式を使います。初期値はRating 1500、RD（不確実性）350です。対局結果は先手勝ち・引分け・後手勝ちをそれぞれ 1 / 0.5 / 0 として扱い、両参加者の更新は対局前snapshotから同時に計算します。対局後のRDは最低30に制限します。

各局を独立した更新期間として扱います。未対局期間によるRD増加、Rating期間の一括更新、順位表示はv1の対象外です。rating履歴には元の勝敗と更新後のRating/RDを残します。

## 保存形式

保存形式はUTF-8 tab区切りテキスト `kadoka.rating_store.v1` です。`P` 行はParticipantIdを1件ずつ記録し、`G` 行は連番、先手・後手ID、結果、更新後の両者Rating/RDを記録します。ID順と結果の追加順で出力し、数値はlocaleに依存しないround-trip表記です。

`deserialize_rating_store()` は参加者一覧から初期状態を作り、全ゲーム結果を順に再生してRatingを再計算します。未知ID、自己対局、不正結果、連番の飛び、相対誤差 `1e-12` を超える保存済み更新値を拒否します。小さな丸め差を許容するため、異なる標準数学ライブラリ間でも復元できます。

## C++ API

`tools/ai_league/include/kadoka/league/rating_store.hpp` が `RatingStore`、`RatingSnapshot`、`RatingHistoryEntry` とserialize / deserialize APIを提供します。各対局結果は追記型の履歴として保存され、Rating変更前の履歴は上書きしません。
