# AIリーグ対局Scheduler v1

## 目的

`create_schedule()` は `ParticipantRegistry` から対局設定の一覧を生成します。対局の実行とRating更新は行いません。参加者はRegistryのID順に読み取るため、総当たりモードは同じ入力から常に同じ順序になります。

## 方式と先後

- `round_robin`: すべての異なる参加者ペアを列挙します。
- `random`: 総当たり対象はそのままに、seedを使って決定論的な擬似乱数順へ並べ替えます。厳密な確率的一様性ではなく、同じmodeとseedで結果を再現することを目的とします。round_robinとrandomでは乱数列の消費位置が異なるため、modeをまたいだ対局seedの一致は保証しません。
- 各ペアは2局を作り、2局目で先後を入れ替えます。これによりペアごとに先後が同数となり、全参加者についても先手数と後手数が一致します。
- 2名未満のRegistryは入力誤りとして拒否します。

3名なら3組のペアに各2局、合計6局を作ります。v1ではラウンド間の休み順やリーグ戦途中からの追加スケジュールは扱いません。

## 入力と例外

未対応のmodeと2名未満のRegistryは `std::invalid_argument` で拒否します。全対局数が `size_t` の範囲を超える場合は `std::length_error` を送出します。対局一覧の確保に必要なメモリが不足する場合は、標準コンテナから送出される `std::length_error` または `std::bad_alloc` が呼び出し側へ伝わります。参加者数に実装固有の上限は設けません。

## MatchConfig

各出力には連番 `match_id`（`match-`接頭辞と、1始まりで8桁以上にゼロ埋めした番号。例: `match-00000001`）、先手・後手のParticipantId、両参加者のtime-control config hash、対局固有seedを含めます。`match_id` は1回のschedule内で一意で、別のschedule呼び出しでは再利用されます。同じRegistry、mode、seedから、C++20標準で定義された `mt19937_64` を使ってmatch順・ID・対局seedを再現します。整数seedからのエンジン初期化も標準で定義された方式を使います。

randomモードは残数を `n` とするFisher-Yates手順を降順で適用します。各indexは `threshold = (0 - n) % n` を使った棄却法でengine出力から一様に選び、棄却されなかった出力を `value % n` に変換します。シャッフル後は同じengineから出力順に1値ずつ取り出し、各matchのseedへ割り当てます。round_robinはシャッフルを省くため、異なるmode間で対局seedの一致は保証しません。

## C++ API

`tools/ai_league/include/kadoka/league/match_scheduler.hpp` が `ScheduleMode`、`SchedulerOptions`、`MatchConfig` と `create_schedule()` を提供します。入力Registryを変更せず、出力は値型のvectorとして返します。
