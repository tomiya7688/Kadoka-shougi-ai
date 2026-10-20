# 再評価バッチ実行 v1

Issue #38 のうち、queue、予算付き実行、保存先の呼び出し、再開cursorの基盤です。
公開APIは `kadoka/relabel/batch.hpp`、リンク先は `kadoka_shogi_relabeler` です。

## queueと再開

`RelabelBatchQueue` は元sampleの順序とrevisionを固定します。
Priority Samplerの結果順に `RelabelSource` を詰めることもできます。
同じdataset ID・dataset revision・sample IDの重複は拒否します。
全sampleの識別子・SFENと評価個体情報を、最初のbackend呼び出し前に検証します。

初回cursorは `{queue.revision, 0}`。次回は保存済みcursorを渡します。
revision不一致と範囲外indexは拒否します。呼び出し元はsampleの内容・順序、
個体ID・model version・config hash、探索設定のいずれかを変更したらrevisionを更新してください。
revision自体の内容hash計算は保存層の責務です。実行中のqueueや設定の変更は禁止です。

## 予算

- `max_samples` は今回保存するsample数。既定値0では評価しません。
- `max_time` と `max_nodes` は未指定なら無制限、0なら評価しません。
- 再開のたびに予算を新しく割り当てます。過去の消費量を含むjob全体の予算は呼び出し元が減算してください。
- 残時間と残nodeを評価個体数で割り、各AIへの上限にします。個別 `search_limits` の方が小さければそちらを使います。
- 各AIに1msまたは1nodeすら割り当てられなければ次sampleを開始しません。
- 時間計測は入力検証と保存処理を含みます。入力検証そのものは途中で打ち切りません。
- node合計はAIが返した `SearchResult.nodes` を使い、整数overflow時は最大値で飽和させます。
- AI例外によりnode数が不明になった場合、部分結果を保存し、node予算指定時は `unaccounted_nodes` で停止します。

既存の単一sample APIは全AIを同期的に呼び出します。探索制限はbackendが守る協調的な予算です。
このAPIは実行中のbackendや保存処理を強制中断しません。制限を無視するAI、例外前の未報告node、
検証・Runtime・保存の実行時間により上限を超える可能性があります。
厳密なCPU時間制限やプロセス強制終了はプロセス管理層で行ってください。

## 部分結果の保存契約

`RelabelBatchSink` はsampleごとに `RelabelRecord` と次cursorを受け取ります。
元ラベル、成功したAI、失敗したAIの情報をまとめて、**recordとcursorを原子的に永続化してから** trueを返してください。
成功・失敗が混在していても、保存できたsampleは再開位置を進めます。
返却結果は全recordをメモリへ蓄積せず、保存済み件数とcursorだけを保持します。

falseなら停止理由は `persistence_failed`。cursorは進めず、`pending_record` に未保存結果を返します。
未保存結果を再保存して対応cursorを確定するか、元cursorから再評価できます。
sinkが例外を投げた場合はそのまま伝播します。保存の成否が不明な場合もあるため、
保存先は `(queue revision, sample index)` を一意keyとして冪等に実装してください。
先行sampleの保存結果は維持されます。

この段階ではファイル形式・DB adapter・checkpoint読込CLIを提供しません。
既定の保存adapterと専用の振る舞いテストはIssue #38の残作業です。
既存CIの成功だけではbatchの予算停止・再開・保存障害の振る舞いの検証にはなりません。

## 停止理由

| 値 | 意味 |
| --- | --- |
| completed | queue末尾まで保存済み |
| sample_budget | 今回の保存件数上限 |
| time_budget | 次sampleへ時間を分配できない |
| node_budget | 次sampleへnodeを分配できない |
| unaccounted_nodes | node予算指定中に消費量不明のAIが発生 |
| persistence_failed | 保存先が確定を拒否、未保存recordを返却 |

最後のsampleを保存してnode不明が判明した場合は `unaccounted_nodes` を優先します。
cursorが末尾かどうかでqueueの消化状態を確認できます。
