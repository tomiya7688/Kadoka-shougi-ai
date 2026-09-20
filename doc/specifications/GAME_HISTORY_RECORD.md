# 対局履歴 Record v1

## 目的

通常対局の正本履歴を、用途の異なる2本のJSON Linesへ分離して保存する。

- BoardState JSONL: canonical盤面系列
- GameAux JSONL: 操作・拒否・終局等のイベント系列

両者は `game_id + ply` でjoinする。

AI探索ログ、評価値、policy/value/Q、rating、dataset recipe等はこの層へ混ぜない。

## game_id

`game_id` は26文字のcanonical ULIDを使用する。

Runtimeは次を提供する。

```cpp
generate_ulid();
make_ulid(timestamp_ms, entropy);
is_valid_ulid(value);
```

`make_ulid()` は固定timestamp/entropyを受け取れるため、テストや再現確認でも利用できる。

## BoardState

```cpp
struct BoardStateRecord {
    std::string game_id;
    std::uint64_t ply;
    std::string sfen;
    Color side_to_move;
    HistoryClock clock;
};
```

wire schema:

```json
{
  "schema": "kadoka.board_state",
  "version": 1,
  "game_id": "01...",
  "ply": 0,
  "sfen": "...",
  "side_to_move": "black",
  "clock": {
    "black_main_ms": 60000,
    "white_main_ms": 60000,
    "black_byoyomi_ms": 30000,
    "white_byoyomi_ms": 30000,
    "black_increment_ms": 0,
    "white_increment_ms": 0,
    "per_move_limit_ms": null
  }
}
```

初期局面は必ず `ply = 0`。

`side_to_move` は便利な参照fieldとしてSFENと重複しているため、readerはSFEN内部の手番と一致することを検証する。

## GameAux

```cpp
struct GameAuxRecord {
    std::string game_id;
    std::uint64_t ply;
    std::uint64_t event_index;
    GameAuxEventType event_type;
    std::optional<Color> actor;
    std::optional<PlayerAction> action;
    std::optional<ActionResult> result;
    std::optional<GameOutcome> terminal;
};
```

### action event

```json
{
  "schema": "kadoka.game_aux",
  "version": 1,
  "game_id": "01...",
  "ply": 0,
  "event_index": 0,
  "event_type": "action",
  "actor": "black",
  "action": {
    "type": "move",
    "move": "7g7f"
  },
  "result": {
    "status": "accepted",
    "reason": null
  },
  "terminal": null
}
```

action eventの `ply` は、操作を試みた時点のcanonical盤面を指す。

そのため7g7fが合法の場合:

```text
BoardState ply 0: 初期局面
GameAux   ply 0: 7g7f accepted
BoardState ply 1: 7g7f後の局面
```

違法手の場合:

```text
BoardState ply 0: 初期局面
GameAux   ply 0 event 0: illegal
GameAux   ply 0 event 1: illegal
GameAux   ply 0 event 2: accepted
BoardState ply 1: accepted後のみ追加
```

これにより違法操作は学習・評価用イベントとして保持できるが、canonical盤面系列を汚染しない。

### terminal event

```json
{
  "schema": "kadoka.game_aux",
  "version": 1,
  "game_id": "01...",
  "ply": 120,
  "event_index": 121,
  "event_type": "terminal",
  "actor": null,
  "action": null,
  "result": null,
  "terminal": {
    "result": "black_win",
    "reason": "resignation"
  }
}
```

終局イベントは最後のcanonical `ply` に紐付く。

## event_index

`event_index` はGameAux内の実イベント通番で、0から単調増加する。

`ply` は合法着手数なので、同一plyに複数イベントが存在できる。イベントの一意な順序は `event_index` で保持する。

## GameHistoryRecorder

```cpp
GameHistoryRecorder recorder{
    Position::startpos(),
    initial_clock
};

recorder.apply_and_record(
    Color::Black,
    PlayerAction::move_action(move_from_usi("7g7f")),
    clock_after
);

recorder.record_terminal(outcome);
```

Recorderは以下を保証する。

- 初期BoardStateをply 0として自動追加
- actorとside-to-moveの一致確認
- #84の `apply_player_action()` で合法性を判定
- 違法操作ではply不変
- 違法操作ではBoardStateを追加しない
- 合法moveだけplyを1増やしてBoardStateを追加
- event_indexを単調増加
- resign後はterminal記録前に次操作を許可しない
- terminal後は追加操作を許可しない

## Reader / Writer

1record API:

```cpp
serialize_board_state_record(...)
deserialize_board_state_record(...)
serialize_game_aux_record(...)
deserialize_game_aux_record(...)
```

JSONL API:

```cpp
serialize_board_state_jsonl(...)
deserialize_board_state_jsonl(...)
serialize_game_aux_jsonl(...)
deserialize_game_aux_jsonl(...)
```

stream API:

```cpp
write_board_state_jsonl(output, records);
read_board_state_jsonl(input);
write_game_aux_jsonl(output, records);
read_game_aux_jsonl(input);
```

## 互換性

v1 readerは:

- `schema` を厳密に確認
- `version == 1` を厳密に確認
- ULID形式を検証
- 必須fieldの型を検証
- clockは非負整数またはnull
- SFENを検証
- actionのUSIを検証
- action/terminal eventのnull関係を検証
- 未知fieldは無視

breaking changeのみschema versionを上げる。

## 層境界

```text
Player API / Core state
        ↓
GameHistoryRecorder
        ├─ BoardState JSONL
        └─ GameAux JSONL
              ↓
TrainingRecord / League / Dataset / Analyzer
```

`game_id` は履歴・学習・評価用であり、通常のPlayerObservationには送らない。
