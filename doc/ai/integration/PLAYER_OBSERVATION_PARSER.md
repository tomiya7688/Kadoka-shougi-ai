# AI Package PlayerObservation Parser

## 目的

Kadoka Shougi の通常Player APIが出力する `PlayerObservation` JSONを、
AI Packageが自分の内部盤面へ変換できるreference経路を提供する。

JSONは観測入力形式の1つであり、AI探索内部の盤面形式ではない。

## 層

```text
Kadoka PlayerObservation JSON
            ↓
JsonPlayerObservationParser
            ↓
ObservedGameState
            ↓
InternalBoardConverter
        ├─ ReferenceInternalBoard
        └─ AI固有高速盤面
            ↓
          Search
```

将来の画面認識入力も、認識完了後に同じ `ObservedGameState` を生成すれば、
`InternalBoardConverter` 以降を再利用できる。

```text
Player JSON ─────────┐
                     ├→ ObservedGameState → AI internal board → Search
Screen recognition ─┘
```

## ObservedGameState

```cpp
struct ObservedGameState {
    std::string sfen;
    Color side_to_move;
    runtime::PlayerClock clock;
};
```

含む情報は通常プレイヤーから見える情報だけ。

- board / hands: SFEN内
- side-to-move
- visible clock

含めない情報:

- legal move list
- check flag
- repetition history
- search depth / nodes
- policy/value/Q
- game_id / match_id
- dataset metadata

## JSON parser

```cpp
JsonPlayerObservationParser parser;

ObservedGameState state = parser.parse(json);
```

内部では #84 の正本codec
`runtime::deserialize_player_observation()`
を利用する。

したがってschema/version、必須field、SFEN、時間値、
SFENとside-to-moveの整合性はPlayer APIと同一ルールで検証する。

未知optional fieldはPlayer API v1のforward compatibility方針に従って無視する。

## InternalBoardConverter

```cpp
class InternalBoardConverter {
public:
    virtual ~InternalBoardConverter() = default;

    virtual void assign_observed_state(
        const ObservedGameState& state
    ) = 0;
};
```

AI Packageはこのinterfaceを実装し、SFEN等から任意の内部表現へ変換できる。

例えば:

- bitboard
- piece-list
- NN入力tensor
- incremental board
- 独自hash付き盤面

JSONを探索ループまで持ち込む必要はない。

## reference internal board

```cpp
ReferenceInternalBoard board =
    parser.parse_reference(json);

const Position& position = board.position();
```

reference実装はプロジェクトの `Position` をAI内部盤面として再利用する。

これはreferenceであり、全AIへ `Position` の採用を強制しない。

SFENには持ち駒も含まれるため、board / hands / side-to-moveを
一度の変換で復元する。

## parse_into

```cpp
MyFastBoard converter;
parser.parse_into(json, converter);
```

`parse_into()` は:

1. JSONをPlayerObservationとして検証
2. `ObservedGameState` へ正規化
3. `InternalBoardConverter` へ渡す

という処理を行う。

## error

JSON、schema/version、SFEN、必須field、内部変換に失敗した場合、
public parser APIは `PlayerObservationParseError` を返す。

AI Packageはこの例外を、対局transportやUIとは独立して処理できる。

## 画面認識との接続

#86 の画面認識AdapterはJSON parserを経由する必要はない。

画面から得た:

- board
- hands
- side-to-move
- visible clock

を正規化して `ObservedGameState` を生成し、
同じ `InternalBoardConverter` へ渡す。

これによりAPI経路だけに最適化されたAIを作らず、
他の将棋ソフトへも持ち出しやすい構造にする。

## Package配置

実装:

```text
ai_package/
  include/kadoka/ai_package/
    observation.hpp
    json_player_observation_parser.hpp
  src/
    observation.cpp
    json_player_observation_parser.cpp
```

CMake target:

```text
kadoka_shogi_ai_package
```

依存方向:

```text
ai_package → runtime → engine/core
```

逆依存は禁止する。
