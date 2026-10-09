# Runtime / Core / Adapter依存境界

## 目的

本書は、将棋Core、Runtime、AI実装・Adapter、Protocol、Creator/Training/Analysisの依存方向を固定します。対象は設計と依存レビューを行う開発者です。新しいbackend、headless実行経路、学習・評価ツールを追加するときは、依存方向と状態更新の権威がこの契約に沿っていることを確認してください。

## 層の責務

- **Core**: 盤面・持ち駒・合法手・終局など、将棋規則に基づくcanonical stateと裁定を所有します。
- **Engine API / AI実装**: Positionを入力として探索し、行動候補を返します。提案はCoreの裁定前にはcanonical stateではありません。
- **Runtime**: backend呼び出し、Actionの検証、状態遷移、対局進行、clock、headless matchなどを調整します。
- **Adapter / Transport**: Engine APIやRuntimeの境界をnative、外部process、script等へ接続し、入出力を変換します。Core規則を再実装せず、canonical stateを直接変更しません。
- **Protocol / UI**: 利用者や外部クライアントの操作を通常のPlayer Actionに変換し、Runtime/Coreの検証結果を表示します。
- **Creator / Training / Analysis**: データ変換、学習、評価、集計を行います。必要に応じてCore/Runtimeの公開契約を利用しますが、match-time層から逆依存されません。

## 許可する依存方向

矢印は「左側が右側を利用できる」ことを示します。

~~~text
Protocol / UI ───────→ Runtime ───────→ Core
        │                  ↑
        └──→ Adapter ──────┘
               │
               └──────────→ Engine API / AI実装 ──→ Core

Creator / Training / Analysis ──→ Core
Creator / Training / Analysis ──→ Runtime（self-play / headless評価時のみ）

Tests ──→ 検証対象の各層
~~~

| 利用側 | 利用してよい契約 | 利用してはいけない依存 |
| --- | --- | --- |
| Core（engine/） | standard library、Core内の型・規則 | Runtime、具体的AI、Protocol/UI、学習・データ変換、process/network transport |
| Engine API / AI実装 | CoreのPosition・合法手等、必要なモデル/inference code | GUI実装、Creator画面、対局履歴保存先、具体的transport process管理 |
| Runtime（runtime/） | CoreとEngine API、Runtime内部のclock・history・backend境界 | UI、Protocol固有処理、Creator/Training/Analysis、ファイル形式ごとの学習ロジック |
| Adapter / Transport | Runtimeのbackend/Player契約、Engine API、必要なtransport library | Core状態の直接書換え、Coreの合法性を迂回する経路 |
| Protocol / UI | RuntimeのPlayer/Match API、表示に必要なCore read-only情報 | 自前の合法判定を最終権威にする処理、AI実装内への直接依存 |
| Creator / Training / Analysis | Core/Runtimeの公開API、Dataset/Model/Training契約 | Runtime/Coreから上位toolingへの逆依存 |

この表は、既存のビルド依存を列挙するだけでなく、新規コードの設計規則も示します。既存の便宜的なAPIが規則に反する場合は、利用範囲を限定し、新しい機能で依存を拡大しないでください。

## 現行ツリーとの対応

| 境界 | 現行の主な位置 | 状態・注意 |
| --- | --- | --- |
| Core state / rules | [engine/include/kadoka/position.hpp](../../engine/include/kadoka/position.hpp)、[engine/include/kadoka/movegen.hpp](../../engine/include/kadoka/movegen.hpp)、[engine/include/kadoka/repetition.hpp](../../engine/include/kadoka/repetition.hpp) | canonical positionと規則判定を所有 |
| Engine API | [engine/include/kadoka/engine.hpp](../../engine/include/kadoka/engine.hpp) | Engine、SearchLimits、SearchResultを定義。提案のみを返す |
| Runtime backend / turn | [runtime/include/kadoka/runtime/ai_backend.hpp](../../runtime/include/kadoka/runtime/ai_backend.hpp)、[runtime/include/kadoka/runtime/turn_runner.hpp](../../runtime/include/kadoka/runtime/turn_runner.hpp) | native/共通backend境界と合法手検証を提供 |
| Native adapter | NativeEngineBackend（ai_backend.hpp） | 既存のin-process Engineを共通Runtime経路へ接続 |
| External / script process | [runtime/include/kadoka/runtime/process_ai_backend.hpp](../../runtime/include/kadoka/runtime/process_ai_backend.hpp) | persistent process transport。scriptもこの境界を使える。processの応答はRuntimeが検証 |
| Dynamic library / network | AIBackendKindの列挙値 | 種類名の予約があることは、具体的transport実装の存在を意味しない |
| Player API | [runtime/include/kadoka/runtime/player_api.hpp](../../runtime/include/kadoka/runtime/player_api.hpp) | 観測、行動、結果の変換と検証境界 |
| Headless match | [runtime/include/kadoka/runtime/headless_match.hpp](../../runtime/include/kadoka/runtime/headless_match.hpp) | GUIなし対局とmatch-level policyを調整 |
| Protocol / CLI | [protocol/cli/main.cpp](../../protocol/cli/main.cpp) | 現行mainはbootstrap表示。完成したGUI/対局画面の存在を示さない |
| Metadata / tools | [doc/specifications/AI_MODEL_METADATA.md](../specifications/AI_MODEL_METADATA.md)、tools/family_metadata/、tools/context_route.py | metadata検証や開発補助。Runtime/Coreはtoolingへ依存しない |

現行Runtimeはadapter境界も同じruntime/内に持ち、独立したtop-level adapters/ directoryをまだ持ちません。実装配置の変更は可能ですが、依存方向とCoreの権威は維持してください。

## 実行経路

### native / external / scriptの1手

~~~text
Engine または Adapter
        ↓ SearchResult / PlayerAction（提案）
Runtime TurnRunner
        ↓ Core合法手・Action検証
Coreがcanonical stateを更新
        ↓
RuntimeがActionResult / 次の観測を返す
~~~

違法な移動や不正なActionは盤面を変更しません。NativeEngineBackend、外部process、scriptの違いで合法性検証を分岐させません。Adapterは候補を正規化してRuntimeへ渡し、最終判断をCoreに委譲します。

### headless対局

~~~text
Black Engine ──┐
               ├→ Headless Match Runtime → TurnRunner → Core
White Engine ──┘                              ↑           │
             retry / clock / match result ────┴───────────┘
~~~

一手の合法性はTurnRunner/Core、手番交代・clock・安全停止・対局結果の集約はHeadless Match Runtimeが担当します。headless用の上限や安全停止を将棋規則の裁定として扱いません。詳細は[Headless Match Runtime仕様](../specifications/HEADLESS_MATCH_RUNTIME.md)を参照してください。

## seedと決定性の境界

- 現行のSearchLimitsは時間・node・depth上限を持ちますが、共通seed fieldはありません。PlayerObservationにもseedを含めません。
- Coreは探索や学習の乱数seedを生成・保存しません。Coreの規則結果はcanonical stateと履歴に基づいて決定します。
- 確率的Engine、学習、benchmarkを再現するときは、seedをその実行を所有するEngine/Training/benchmark設定から明示的に渡し、実行記録とModel Metadataのdeterminism等へ保存します。
- seedをPositionや通常のPlayerObservationへ埋め込んだり、game IDから暗黙に導出したりしません。
- 現行共通API・process protocolに共通seed転送がないため、backendをまたぐ同一seed再現性は保証されません。共通契約にseedを追加するときは、各backendへの伝達・記録・再現テストを同時に定義します。

## 禁止する依存・迂回例

1. engine/からruntime/、protocol/、tools/をincludeする。CoreをUIや学習ツールの実行順に結び付けません。
2. runtime/からCreator/Training/Analysisの具体実装を呼び出す。学習処理はmatch-time経路に入れません。
3. 外部processやscriptがCoreのPositionを直接書き換える。processの出力はRuntimeの通常Actionとして受け取り、Coreが検証します。
4. UIやProtocolが独自の合法手判定結果を最終状態へ適用する。表示用の候補計算とcanonical stateの更新を混同しません。
5. 学習・benchmark seedをglobal mutable stateへ置く、または実行metadataに残さない。実行単位の設定と証跡を保持します。
6. AIBackendKind::Network等の列挙値だけを根拠に、該当transportが実装済みと説明する。実装状況は具体的なsourceと検証結果で確認します。

## 変更レビューの確認項目

- 新しいinclude/link依存が上表の許可方向に沿う。
- backend出力からCoreのcanonical state変更までにRuntime/Core検証がある。
- headless/GUI/processなどの実行方法が将棋規則を別実装していない。
- 確率的処理のseed所有者・伝達方法・記録先を説明できる。
- 未実装transportや未検証の決定性を完了済みと記述していない。

関連仕様: [Engine Runtime Contract](../specifications/ENGINE_RUNTIME.md)、[Sibling Project Alignment](SIBLING_PROJECT_ALIGNMENT.md)。
