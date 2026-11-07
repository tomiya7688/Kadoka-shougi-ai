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

個体IDの計算対象は `family`、`kind`、`layer`、`model_id`、`model_version`、`orchestrator_config_hash`、`search_config_hash`、`time_control_config_hash` の8キーとそれぞれの文字列値です。`id` 自体は計算に含めません。正規化とハッシュのbyte規則は [Kadoka Best構成manifest v1](KADOKA_BEST_MANIFEST_V1.md) と共通です。
## 保存形式

RegistryはUTF-8のtab区切りテキスト `kadoka.participant_registry.v1` で保存します。1行目はversion headerです。以後の行はID、kind、family、layer、model ID、model version、search hash、orchestrator hash、time-control hashの9 fieldをtabで区切ります。ID順に出力するため、同じRegistryのserializationは常に同じbyte列です。fieldは単一行のASCII tokenまたはhashに制限し、tabや改行を含めません。

`deserialize_registry()` は未知のheader、列数不一致、不正field、ID不一致、重複IDを拒否します。空Registryはheaderだけで表します。 改行はLF（`0x0A`）に固定し、headerと各行には終端のLFが必要です。CRLF、途中や末尾の空行は拒否します。 形式や種別・層の解析エラーは `std::runtime_error`、値の検証・ID不一致・重複は `std::invalid_argument` を送出します。バイト列をそのまま読み書きする場合はバイナリモードを使用してください。

## C++ API

`tools/kadoka_best/include/kadoka/best/participant_registry.hpp` が個体型、Registry、ID生成、検証、保存・復元APIを提供します。既定構築した個体は必須fieldが空のため無効です。各fieldを設定してから `participant_id()` でIDを生成します。`register_participant()` は設定とIDの一致を再検査し、重複IDを例外で拒否します。

`register_participant()` は値渡しです。左辺値はコピーされ、`std::move` した右辺値は検証・重複の失敗時も呼出し時にmoveされ得ます。失敗時に元の個体を保持する場合は左辺値を渡してください。
