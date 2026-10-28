# Training Checkpoint Manager v1

## 目的

`CheckpointManager` は checkpoint core が保存する immutable snapshotを使ってlatest、periodic、bestの参照を管理します。モデルやoptimizer stateの解釈は行いません。

## API

- `save_latest(state)` は新しいcheckpointを保存し、latest pointerを更新します。
- `save_periodic(state)` はperiodic checkpointを保存してlatestを更新します。新しく保存したcheckpointを必ず残し、残りは最終更新時刻の新しい順に選んで合計 `periodic_keep` 件を保持します。`periodic_keep` は1以上です。
- `consider_best(state, metric_name, metric_value, lower_is_better)` は最初の値か、指定した向きに改善した値の場合だけcheckpointを保存します。metric名または向きは途中変更できません。best判定指標そのものは呼び出し側が選びます。
- `latest_path()` と `best_path()` は保存済みpointerが指す既存checkpointを返します。
- managerを再生成しても世代付きpointer recordから最新状態を復元します。

## ファイル公開

checkpoint本体はcheckpoint coreのatomic createを使います。pointer recordは同じdirectoryのtemporary fileに全量を書いてcloseを検査し、一意名へrenameして公開します。readerは確定済み世代の最大recordを選びます。古いpointer recordは安全なrollback用に保持します。書込み中のtemporary fileはreaderから無視されます。

periodic retentionはperiodic checkpoint本体だけを削除します。latestが指す新規periodic checkpointは保持対象に含めます。bestは独立したcheckpointを保持します。

## 境界

- 定期保存のタイミングはtrainerが決定します。
- best指標の生成、評価方向の選択、学習停止はtrainer側の責務です。
- checkpoint形式、checksum、config/dataset互換性検査はTraining Checkpoint v1が担当します。
