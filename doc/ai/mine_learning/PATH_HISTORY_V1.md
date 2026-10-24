# 経路履歴 v1

## 目的

湯のみコウモリなど、同じ盤面でもそこへ至った手順を特徴として使うAI向けに、現在局面と保持する着手列を一つの履歴として扱います。

```cpp
FullMovePathHistory history{initial_sfen};
history.record("7g7f", next_sfen);
const std::string key = history.identity();
```

`FullMovePathHistory` は記録した全着手を残します。`LastNMovePathHistory` は指定数まで直近の着手を残します。どちらも `current_position_sfen()` で最新局面を返し、同じ局面でも保持着手列が異なれば別の `identity()` を返します。

## 記録のしかた

`record(move_usi, resulting_position_sfen)` は、着手と着手適用後のSFENを同時に渡します。SFENの生成と着手の合法性確認は呼び出し側の責務です。この型は入力を再生せず、学習用の経路情報を保存します。

`FullMovePathHistory` を使うと、経路の全手順を保持します。`LastNMovePathHistory(capacity, initial_sfen)` は最大 `capacity` 手を保持します。capacityが0なら着手列を残さず、現在局面だけを更新します。

## 保存形式

保存データは `KPH\0` magic、64-bit big-endianのformat version、形式種別、保持上限、現在SFEN、着手数、各USI着手のUTF-8 byte長と内容を順に並べたバイナリ形式です。各整数と文字列の長さは8 byteです。version 1では形式種別 `1` が全手、`2` が直近N手です。全手形式の保持上限は0です。

文字列は長さで区切るため、区切り文字や改行を含んでも切り分けを誤りません。復元時に未知version・未知形式・切り詰め・余分なbyte・空の着手を検出すると `std::invalid_argument` を送出します。

```cpp
const std::string bytes = history.serialize();
std::unique_ptr<PathHistory> restored = deserialize_path_history(bytes);
```

`identity()` は保存形式と同じ正準バイト列を返します。これはハッシュではなく内容を完全に含む識別子です。そのためハッシュ衝突はありませんが、完全な履歴を使う場合は着手列に比例した長さになります。直近N手形式のidentityには、現在SFEN、保持着手列、設定上限が含まれます。

## 確認

`path_history_test` は全手・直近N手の保存と復元、同一局面へ至る異なる経路の識別、空履歴、0手保持、破損データ拒否を確認します。

