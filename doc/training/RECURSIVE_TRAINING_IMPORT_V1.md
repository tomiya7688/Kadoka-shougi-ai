# Recursive Training Data Import v1

## 目的

ユーザーが学習データの親フォルダを1つ選択するだけで、配下に保存された複数対局の Kadoka Core 標準履歴をAI Toolsへまとめてimportできるようにする。

基本形式は BoardState JSONL + GameAux JSONL であり、Issue #11 の ParserRegistry / CoreHistoryParser を利用する。

## 入口

ParserRegistry registry = make_reference_parser_registry();
RecursiveTrainingImporter importer{registry};
ImportSummary summary = importer.import_folder(root, sink);

ImporterはRecordをvectorで返さず、既存の ParsedRecordSink へ逐次出力する。

## 想定ディレクトリ

training-data/
  game-001/
    board_state.jsonl
    game_aux.jsonl
  game-002/
    board_state.jsonl
    game_aux.jsonl
  tournament-a/
    game-003/
      board.json
      aux-data.jsonl

ファイル名は最終判定に使わない。board_state.jsonl / game_aux.jsonl は推奨名だが、実際の種類は先頭data recordのschemaで判定する。

## discovery

再帰scan対象は .json と .jsonl。拡張子比較はASCII case-insensitive。その他のファイルは探索対象外で、detected_files にも含めない。

symlinkは追跡しない。選択root外への意図しない探索、loop、重複読込を避ける。

候補pathはroot相対のgeneric pathでsortしてから処理するため、OSやdirectory iteratorの返却順に依存しない。

## schema autodetect

各候補について先頭の空でない1行だけを読み、Runtime正本readerを順に試して判定する。

1. deserialize_board_state_record()
2. 失敗したら deserialize_game_aux_record()

未知JSON/JSONLはwarningとしてskipする。Kadoka Core schema名を含むがrecordとして壊れているものは MalformedCoreCandidate errorとしてskipする。schema validationをTraining Tools側で再実装しない。

## pair key

Core pairは relative directory + game_id で作る。

例:
game-001/board.jsonl  game_id=A
game-001/events.jsonl game_id=A

これは1 pairになる。同じdirectoryでもgame_idが違えばpairにしない。

## missing pair

BoardStateだけ、またはGameAuxだけ存在する場合は MissingPair warningとしてimportしない。他の正常gameは継続する。

## duplicate role

同一 directory + game_id にBoardStateが複数、またはGameAuxが複数存在する場合は DuplicateRole errorとし、そのgameはimportしない。

## duplicate game_id

異なるdirectoryに完全なpairが存在し、同一game_idを持つ場合は全該当groupを DuplicateGameId errorとしてimportしない。先に見つかった方だけ採用する挙動にはしない。

## streaming preflight

正常gameだけをcaller sinkへ流すため、各pairは2 passで読む。

Pass 1: CoreHistoryParser -> PreflightSink
Pass 2: Pass 1成功時のみ CoreHistoryParser -> caller ParsedRecordSink

Pass 1はRecordを保持しない。これによりfile全体をvector化せず、後半でordering errorが見つかった壊れたgameの途中Recordをcallerへ流さない。

I/Oは2倍になるが、1.0.0では安全なpartial-failure semanticsを優先する。

## partial failure

good-a がvalid、brokenがmalformed、good-bがvalidなら、good-aをimportし、brokenをdiagnosticへ残し、good-bを継続importする。root全体を失敗させない。

## source provenance

CoreHistoryParserへ渡すsource名はroot相対pathとする。ParsedRecordのprovenanceには元ファイル名と行番号が残る。絶対pathを中間Recordへ固定しない。

## ImportSummary

ImportSummaryは detected_files / imported_games / imported_records / skipped_files / warnings / errors / diagnostics を保持する。

detected_files: 再帰scanで見つかったJSON/JSONL候補数。
imported_games: preflight + actual parseの両方を完了したCore pair数。
imported_records: caller sinkへ実際に流したParsedRecord数。
skipped_files: detected_files から成功gameが消費した2 filesずつを引いた数。

## diagnostic codes

RootNotFound
RootNotDirectory
ScanFailed
FileOpenFailed
UnsupportedFile
MalformedCoreCandidate
MissingPair
DuplicateRole
DuplicateGameId
MissingParser
ParserFailed

diagnosticには severity / code / root相対path / game_id / message を保持する。Parser内部errorは ParsedRecordSink::on_error() にも元の ParseError を渡す。

## deterministic order

1. candidate pathをroot相対generic pathでsort
2. pairを directory + game_id のmap順で処理
3. CoreHistoryParserがply順にRecordをemit

同じdataset treeなら同じimport順序になる。

## 大規模data

Importerは候補pathとpair metadataは保持するが、BoardState/GameAux本文は一括loadしない。Core pairはstreamで読み、preflight時もRecordを捨てながら検証する。

testでは3000 BoardState recordを持つ1 gameをCollecting sinkなしでimportし、全recordを逐次処理できることを確認する。

## 非対象

archive / zip展開、CSV/棋譜の自動選択、preprocessing、Dataset split、training、GUI folder picker、model generation。

GUIはIssue #98で本Importerを呼び出す。

## 次の接続

AI作成UI (#98)
  -> folder select
RecursiveTrainingImporter (#109)
  -> ParserRegistry / Parser (#11)
  -> ParsedRecord stream
  -> Preprocess / Split
  -> Trainer

これで1.0.0の基本導線を「親フォルダを選択する」から開始できる。
