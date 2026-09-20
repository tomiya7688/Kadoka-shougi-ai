# Wire Compatibility Fixtures

## 目的

Player API、GameHistory、AI Package parser の wire compatibility を
通常CIで継続的に固定する。

この検証は単なるunit testではなく、CMake install後の配布候補を対象にする。

```text
source
  ↓ configure/build
build tree
  ↓ cmake --install
distribution candidate
  ├─ bin/kadoka_shogi
  ├─ bin/kadoka_wire_compat
  └─ share/kadoka/wire_v1/*
            ↓
      installed verifier
```

CIはsource treeのfixtureパスをverifierへ直接渡さない。

## v1 fixtures

`tests/fixtures/wire_v1/` を正本とする。

- `player_observation.json`
- `player_observation_unknown_optional.json`
- `player_observation_unsupported_version.json`
- `player_action_move.json`
- `player_action_resign.json`
- `action_result_accepted.json`
- `action_result_illegal.json`
- `board_state.jsonl`
- `game_aux.jsonl`

CMake installではこれらを:

```text
share/kadoka/wire_v1/
```

へコピーする。

## verifier

配布候補には `kadoka_wire_compat` を含める。

使用例:

```text
kadoka_wire_compat <fixture-directory>
```

検証内容:

### PlayerObservation

- schema/version
- canonical encode/decode round-trip
- SFEN
- side-to-move
- visible clock
- unknown optional field tolerance
- unsupported version rejection
- AI Package reference parserとの一致

### PlayerAction / ActionResult

- move
- resign
- accepted
- illegal + reason
- canonical round-trip

### BoardState / GameAux JSONL

- JSONL round-trip
- fixed game_id
- `game_id + ply` join
- `event_index` 単調増加
- visible clock round-trip
- illegal actionがplyを進めない
- accepted moveのみ次BoardStateを生成
- resign action
- terminal result + reason

fixtureのaction prefixは `GameHistoryRecorder` でも再実行し、
違法手でcanonical履歴が変化しないことと、合法手後のSFENが
fixtureの次BoardStateと一致することを確認する。

## compatibility policy

v1 fixtureはcompatibility contractであり、既存実装に合わせるためだけに
気軽に書き換えない。

互換な変更:

- readerが未知optional fieldを受け入れる
- producerが既存required fieldの意味を維持する
- internal implementationを変更して同じwireを生成する

breaking change例:

- required field削除
- field型変更
- enum意味変更
- SFEN/USI以外への非互換置換
- `game_id + ply` join意味変更
- illegal actionでplyが進むような変更

breaking changeが必要な場合は:

1. schema versionを更新
2. 旧version reader継続可否を明示
3. 新version fixtureを別ディレクトリで追加
4. migration / compatibility方針を文書化

既存v1 fixtureを単純上書きしてCIを通すことはしない。

## CI

Linux:

1. Release configure
2. build
3. CTest
4. `cmake --install ... --prefix build/stage`
5. installed `kadoka_wire_compat`
6. installed `kadoka_shogi` smoke

Windows:

1. `build.bat`
2. `cmake --install ... --prefix build\stage`
3. installed `kadoka_wire_compat.exe`
4. installed `kadoka_shogi.exe` smoke
5. `build/stage` 全体をdeveloper distribution candidateとしてartifact upload

これにより、source treeだけ正常でinstall/package結果が壊れている状態も検出する。
