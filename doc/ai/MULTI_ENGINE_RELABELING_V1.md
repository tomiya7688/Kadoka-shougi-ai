# Multi-engine Relabeling v1

## 目的と利用

この機能は、既存のDatasetサンプルが持つ局面を複数のAI backendへ渡し、評価結果を元レコードを保持したまま追記可能なRelabelRecordとして返します。呼び出し側は返されたrecordをGameRecordやDatasetの追記先へ保存します。元サンプルの値を置換・削除しません。

呼び出し側はsource sampleと、ParticipantId・model version・config hash・AIBackendの組を2個以上渡します。各AIには同じSFEN局面とSearchLimitsを渡し、既存RuntimeのTurnRunnerが提案手を検証します。

## 保存する情報

RelabelRecordは以下をまとめて返します。

- 評価前のdataset ID、revision、sample ID、SFEN、観測着手、観測評価値
- 各評価個体のParticipantId、model version、config hash
- record単位で共通の要求SearchLimits（backendでの遵守は保証しない）
- 成功時のTurnStatus、SearchResult、決定時間
- backend例外または違法手のエラー

評価は渡された順に実行し、backendの状態や共有資源は隔離・リセットしません。呼び出し側がbackend間の独立性と、自己申告された個体ID・設定情報と実体の一致を管理します。failedのSearchResultは診断用であり、有効なラベルとして利用しないでください。

各AI結果を独立した行として保持するため、異なる候補手や評価値によるdisagreementを元データと併せて確認できます。失敗したbackendも失敗理由とprovenanceを残し、残りのbackendを続けて実行します。

## 入力検証と境界

- dataset ID、revision、sample ID、SFENは必須
- evaluatorは2個以上で、ParticipantIdは互いに異なる
- evaluatorごとにmodel version、config hash、backendが必要
- 不正局面や呼び出し全体の契約違反は評価開始前に失敗
- 個々のbackend実行失敗や違法手はその評価結果だけをfailedにする
- 合法手のない局面ではRuntimeはbackendを呼ばず、各評価を `NoLegalMoves` とfailedで記録する
- Runtimeが返す元Positionは変更せず、各AIは同じ初期局面から評価する

## 完了確認

kadoka_relabeler_testは異なる2個のAI出力、元ラベルとprovenanceの保持、途中AI失敗後の継続、重複個体と不足・未設定入力の拒否を検査します。
