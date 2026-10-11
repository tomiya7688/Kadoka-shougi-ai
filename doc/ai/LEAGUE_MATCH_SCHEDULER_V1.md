# AIリーグ対局Scheduler v1

## 目的

`create_schedule()` は `ParticipantRegistry` から対局設定の一覧を生成します。対局の実行とRating更新は行いません。参加者はRegistryのID順に読み取るため、総当たりモードは同じ入力から常に同じ順序になります。

## 方式と先後

- `round_robin`: すべての異なる参加者ペアを列挙します。
- `random`: 総当たり対象はそのままに、seedを使って対局順を決定論的に並べ替えます。
- 各ペアは2局を作り、2局目で先後を入れ替えます。これによりペア単位で先後が同数になります。
- 2名未満のRegistryは入力誤りとして拒否します。

3名なら3組のペアに各2局、合計6局を作ります。v1ではラウンド間の休み順やリーグ戦途中からの追加スケジュールは扱いません。

## MatchConfig

各出力には連番 `match_id`、先手・後手のParticipantId、両参加者のtime-control config hash、対局固有seedを含めます。スケジューラseedと同一入力を使うとmatch順・ID・対局seedが一致します。randomモードは標準化された `mt19937_64` と明示的なFisher-Yates手順でシャッフルするため、標準ライブラリの `shuffle` 実装差に依存しません。

## C++ API

`tools/ai_league/include/kadoka/league/match_scheduler.hpp` が `ScheduleMode`、`SchedulerOptions`、`MatchConfig` と `create_schedule()` を提供します。入力Registryを変更せず、出力は値型のvectorとして返します。
