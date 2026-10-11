# 競技個体Registry v1

## 目的

`ParticipantRegistry` は古典AI、外部AI、実験AIを同じ登録表で扱います。競技個体のIDにはfamily、層、model IDとversion、探索・orchestrator・時間制御の各設定hashを含めます。対局予定、Rating、対局実行、Dataset出力はこのRegistryの責務に含めません。

## Participant

| Field | Meaning |
| --- | --- |
| `kind` | `classical`、`external`、`experimental` のいずれか |
| `family` | AI系列を示すASCII token |
| `layer` | `raw`、`integrated`、`full` のいずれか |
| `model_id`, `model_version` | model参照の再現可能なIDとversion |
| `search_config_hash` | 探索設定のSHA-256 config hash |
| `orchestrator_config_hash` | orchestrator設定のSHA-256 config hash |
| `time_control_config_hash` | 時間制御設定のSHA-256 config hash |
| `id` | 上記全fieldから生成した `sha256:<64 lowercase hex>` |

各設定hashは `kadoka.best_manifest.v1` と同じ `sha256:` + 小文字hex 64桁形式です。個体IDはfield名と値を含む正規化config mapを同じSHA-256実装で計算するため、同一設定は同一IDになり、どの設定を変えても別IDになります。モデル内容の署名や完全性を証明するものではありません。

## 保存形式

RegistryはUTF-8のtab区切りテキスト `kadoka.participant_registry.v1` で保存します。1行目はversion headerです。以後の行はID、kind、family、layer、model ID、model version、search hash、orchestrator hash、time-control hashの9 fieldをtabで区切ります。ID順に出力するため、同じRegistryのserializationは常に同じbyte列です。fieldは単一行のASCII tokenまたはhashに制限し、tabや改行を含めません。

`deserialize_registry()` は未知のheader、列数不一致、不正field、ID不一致、重複IDを拒否します。空Registryはheaderだけで表します。

## C++ API

`tools/kadoka_best/include/kadoka/best/participant_registry.hpp` が個体型、Registry、ID生成、検証、保存・復元APIを提供します。登録前に `participant_id()` でIDを設定します。`register_participant()` は設定とIDの一致を再検査し、重複IDを例外で拒否します。

