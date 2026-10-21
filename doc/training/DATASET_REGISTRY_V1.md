# Dataset Registry v1

## 目的

DatasetをIDで登録し、学習・検証・固定評価・リーグ評価で使うデータの出自を追跡します。対局や学習データの内容自体は保存しません。利用側は登録したIDからメタデータを取得し、実データの置き場所を別途管理します。

## 利用方法

`kadoka::dataset::DatasetRegistry` に専用ディレクトリを渡し、`DatasetMetadata` を `add` します。同じディレクトリから作り直したRegistryでも `get` と `list` で再読込できます。用途ごとの一覧には `list_by_role` を使います。存在するIDの再登録は `false` を返し、既存の内容を上書きしません。

## メタデータ

| 項目 | 意味 |
| --- | --- |
| `dataset_id` | 安定ID。小文字英数字、`_`、`-` のみ、最大128文字。WindowsとLinuxで同じ識別結果にするため大文字は使えません。 |
| `source_type` | `self_play`、`imported`、`derived`、`external` のいずれか。 |
| `generator_model_id/version` | 生成モデルのIDと版。指定時は対で記録。 |
| `opponent_model_id/version` | 対戦相手のIDと版。指定時は対で記録。 |
| `parent_dataset_id` | 派生元Dataset。`derived` の場合に必須。 |
| `generation` | 親がない場合は0。派生時は親の世代+1。 |
| `search_config_hash`、`training_config_hash` | 任意の小文字64桁SHA-256。空欄は未記録。 |
| `created_at` | 秒精度のUTC日時 `YYYY-MM-DDTHH:MM:SSZ`。 |
| `license` | 利用条件。空欄不可。 |
| `role` | `training`、`validation`、`benchmark`、`league_eval`。 |

モデル情報と設定hashは出自の記録です。実ファイルやライセンス内容の真偽をRegistryが検証するものではありません。登録時に親IDの存在と世代を確認し、子が残る間は親を削除できません。用途は単一値で、`list_by_role` は他用途を返しません。

## 保存形式と運用境界

各IDを `<dataset_id>.dataset` に保存します。先頭は `kadoka.dataset_registry.v1` と改行、その後に13項目を固定順のUTF-8値で保存します。各項目はバイト長、コロン、値を連結した形式です。読み取り時はversion、全項目、末尾、ファイル名とのID一致、1 MiB上限を検査します。未知versionや破損を黙って読み飛ばしません。

登録時は同じディレクトリ内の一時ファイルを完成させてからrenameします。同一ディレクトリは単一writerを想定します。電源断まで保証するfsync、複数プロセス間の排他、実データの検証・移動は対象外です。
