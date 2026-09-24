# Training Parser v1

## 目的

AI作成ツールが複数の入力形式を、後段の前処理・学習から独立した
共通中間Recordへ変換できるようにする。

最も基本的なimport形式は Kadoka Core が出力する:

- BoardState JSONL
- GameAux JSONL

の組み合わせとする。

Core schema自体をAI Tools都合で変更しない。

## 配置

```text
tools/training_parser/
  include/kadoka/training/
    parser.hpp
    core_history_parser.hpp
    sfen_lines_parser.hpp
  src/
    parser.cpp
    core_history_parser.cpp
    sfen_lines_parser.cpp
```

CMake target:

```text
kadoka_shogi_training_parser
```

依存方向:

```text
Training Parser
      ↓
Runtime history/player contracts
      ↓
Authoritative Core
```

Runtime/CoreからTraining Parserへの逆依存は作らない。

## Parser interface

```cpp
class Parser {
public:
    virtual std::string_view id() const noexcept = 0;
    virtual std::string_view display_name() const noexcept = 0;

    virtual ParseSummary parse(
        std::span<const ParserSource> sources,
        ParsedRecordSink& sink
    ) const = 0;
};
```

Parserはファイル探索をしない。

入力は:

```cpp
struct ParserSource {
    std::string name;
    std::istream* stream;
};
```

とし、実ファイル、memory stream、archive展開結果、network stream等を
caller側で自由に供給できる。

#109 の再帰Importerは:

1. folderを探索
2. source候補を発見
3. ParserSourceを構築
4. registryからParserを選択
5. parse

という責務を持つ。

## ParsedRecord

v1中間Record:

```cpp
struct ParsedRecord {
    std::optional<std::string> game_id;
    std::optional<std::uint64_t> ply;
    std::string sfen;
    Color side_to_move;
    runtime::HistoryClock clock;
    std::vector<ParsedEvent> events;
    SourceProvenance provenance;
};
```

保持するもの:

- canonical position
- side-to-move
- visible clock
- 同一plyのaction / terminal event
- source provenance

保持しないもの:

- policy target
- value/Q
- training weight
- split
- normalized feature tensor
- model-specific encoding
- search log

これらは後段のDataset/Preprocess/Training層で生成する。

## Provenance

```cpp
struct SourceLocation {
    std::string source_name;
    std::uint64_t line;
};

struct SourceProvenance {
    std::string parser_id;
    std::vector<SourceLocation> locations;
};
```

Core historyの1つのParsedRecordには:

- 対応するBoardState行
- 同一 `game_id + ply` のGameAux行

をすべて記録する。

これにより後から元データへ追跡できる。

## ParsedEvent

```cpp
struct ParsedEvent {
    std::uint64_t event_index;
    ParsedEventType type;
    std::optional<Color> actor;
    std::optional<PlayerAction> action;
    std::optional<ActionResult> result;
    std::optional<GameOutcome> terminal;
    SourceLocation source;
};
```

GameAuxの意味を学習ツール都合で変換せず保持する。

違法手もeventとして残る。

## Error contract

```cpp
enum class ParseErrorCode {
    InvalidInputCount,
    DuplicateSourceName,
    UnsupportedInput,
    MalformedRecord,
    JoinMismatch,
    OrderingViolation,
    InternalError,
};
```

`ParseError` は:

- parser_id
- source_name
- line
- message
- recoverable

を保持する。

streaming parserはdata errorを例外だけで外へ投げず、
可能なものはsinkへ構造化して報告する。

## ParsedRecordSink

```cpp
class ParsedRecordSink {
public:
    virtual void on_record(ParsedRecord record) = 0;
    virtual void on_error(ParseError error) = 0;
};
```

Parserはvectorを返すことを必須にしない。

大量データではcallerが:

- DBへ逐次保存
- Dataset builderへ逐次送信
- batch bufferへ追加

できる。

テストや小規模利用向けに `CollectingParsedRecordSink` も提供する。

## Parser Registry

```cpp
ParserRegistry registry =
    make_reference_parser_registry();
```

reference registryには:

- `kadoka.core_history.v1`
- `reference.sfen_lines.v1`

を登録する。

ID重複は拒否する。

registryはformatの実装を差し替え可能にし、
他ソフト由来棋譜やCSV等を後から追加できる。

## Core History Parser

Parser ID:

```text
kadoka.core_history.v1
```

入力は2 stream:

- BoardState JSONL
- GameAux JSONL

source順序やfilenameには依存せず、
先頭data recordの `schema` で役割を識別する。

### streaming join

全recordを一括loadせず、両streamを1recordずつ先読みする。

```text
BoardState ply 0 ─┐
GameAux ply 0 #0  ├→ ParsedRecord ply 0
GameAux ply 0 #1  ┘

BoardState ply 1 ─┐
GameAux ply 1 #2  ├→ ParsedRecord ply 1
GameAux ply 1 #3  ┘
```

検証:

- BoardState plyは0開始、1ずつ増加
- game_idはpair内で一定
- GameAux game_idがBoardStateと一致
- GameAux event_indexは0開始、1ずつ増加
- GameAux plyに対応するBoardStateが存在
- terminal後に追加eventがない
- terminal後に追加BoardStateがない
- schema/version validationはRuntime正本readerを使用

違法手が同一plyに複数存在しても正常にjoinする。

## SFEN-lines reference parser

Parser ID:

```text
reference.sfen_lines.v1
```

1 sourceを受け取り、1行1SFENとして読む。

例:

```text
# comment
lnsgkgsnl/1r5b1/... b - 1
lnsgkgsnl/1r5b1/... w - 2
```

- 空行を無視
- `#` から始まる行を無視
- valid SFENは1 ParsedRecord
- game_id / plyはnull
- clock/eventsは空
- malformed行はrecoverable error
- malformed行の後も継続

これは追加Parser実装の最小referenceであり、
本番の棋譜formatを定義するものではない。

## 非対象

このIssueでは実装しない。

- recursive folder scan (#109)
- schema autodetect registry orchestration (#109)
- preprocessing
- train/validation split
- training target生成
- trainer
- GUI
- model selector

## 次の接続

```text
Folder Import (#109)
      ↓
Parser Registry / Parser (#11)
      ↓
ParsedRecord stream
      ↓
Preprocess / Split
      ↓
Trainer
```

これによりユーザーは最終的に親folderを1つ選択するだけで、
配下のCore履歴をまとめてAI Toolsへ流せる。
