# Board Recognition Adapter

## 目的

AI PackageがKadoka ShougiのPlayer JSONだけでなく、他の将棋ソフトの
画面・画像・動画フレーム等からも同じ探索ロジックを再利用できるようにする。

この層は画像認識モデルそのものではなく、入力境界と失敗契約だけを定義する。

## 全体構造

```text
OS capture / image / video
          ↓
   ObservationSource
          ↓
    ObservationFrame
          ↓
BoardRecognitionAdapter
          ↓
 BoardRecognitionResult
          ↓
confidence / state validation
          ↓
   ObservedGameState
          ↓
 InternalBoardConverter
          ↓
 AI internal board
          ↓
       Search
```

API経路は同じ `ObservedGameState` へ合流する。

```text
PlayerObservation JSON
        ↓
JsonPlayerObservationParser ─┐
                             ├→ ObservedGameState
Screen/Image recognition ────┘
                                  ↓
                         InternalBoardConverter
                                  ↓
                              AI Search
```

探索本体は入力がAPIだったか画像だったかを知る必要がない。

## ObservationSource

```cpp
class ObservationSource {
public:
    virtual ~ObservationSource() = default;

    virtual ObservationCaptureResult capture() = 0;
};
```

責務:

- 画面、画像、動画等から1フレームを取得する
- raw pixel frameを返す
- capture失敗を明示する

非責務:

- 将棋盤認識
- OCR
- 駒認識
- 合法手生成
- hidden/internal game data取得

OS固有のscreen capture実装はこのinterfaceの外部実装とする。

## ObservationFrame

```cpp
struct ObservationFrame {
    std::uint32_t width;
    std::uint32_t height;
    std::size_t row_stride_bytes;
    ObservationPixelFormat pixel_format;
    std::vector<std::uint8_t> pixels;
};
```

v1で定義するpixel format:

- `Gray8`
- `Rgb8`
- `Rgba8`
- `Bgra8`

`row_stride_bytes` を独立fieldにしているため、paddingを持つcapture bufferも
不要なcopyなしで表現できる。

pipelineはrecognition前に:

- width > 0
- height > 0
- stride >= width * bytes_per_pixel
- pixel buffer >= stride * height

を検証する。

## capture failure

```cpp
enum class ObservationCaptureFailure {
    None,
    Unavailable,
    PermissionDenied,
    EndOfStream,
    InvalidFrame,
    Other,
};
```

例えば:

- windowが存在しない
- OS permission拒否
- video終了
- capture backend故障

をrecognition failureと区別する。

## BoardRecognitionAdapter

```cpp
class BoardRecognitionAdapter {
public:
    virtual ~BoardRecognitionAdapter() = default;

    virtual BoardRecognitionResult recognize(
        const ObservationFrame& frame
    ) = 0;
};
```

具体的な実装は任意。

例:

- classical CV
- template matching
- OCR
- CNN / ViT
- GUI専用recognizer
- 外部process/model service

このrepositoryの共通I/Fは特定認識手法を要求しない。

## recognition result

成功時:

```cpp
struct BoardRecognitionResult {
    std::optional<ObservedGameState> state;
    RecognitionConfidence confidence;
    BoardRecognitionFailure failure;
    std::string message;
};
```

`state` は既存の共通境界をそのまま利用する。

```cpp
struct ObservedGameState {
    std::string sfen;
    Color side_to_move;
    runtime::PlayerClock clock;
};
```

SFENに:

- board
- hands
- side-to-move

が含まれる。

`side_to_move` はconsumerの利便性のため明示fieldでも保持するが、
pipelineはSFEN内部の手番と一致することを検証する。

visible clockはPlayer APIと同じ `PlayerClock` を使用する。
認識できない時間fieldは `nullopt` として表現できる。

## recognition failure

```cpp
enum class BoardRecognitionFailure {
    None,
    BoardNotFound,
    AmbiguousBoard,
    MissingSideToMove,
    InvalidState,
    UnsupportedFrame,
    Other,
};
```

「画像取得できなかった」と「画像は取得できたが盤面を認識できなかった」を
別statusとして扱う。

## confidence

```cpp
struct RecognitionConfidence {
    double overall;
    std::optional<double> board;
    std::optional<double> hands;
    std::optional<double> side_to_move;
    std::optional<double> clock;
};
```

値域はすべて `[0, 1]`。

component confidenceは認識Adapterが提供可能な場合だけ設定する。

confidenceは共通の数値範囲を持つが、異なるrecognizer間で同じ0.9が
同じ統計的意味を持つことまでは保証しない。
各認識モデルは必要に応じてcalibration方法を別途metadataで定義する。

## acceptance policy

```cpp
struct RecognitionAcceptancePolicy {
    double minimum_overall_confidence;
    std::optional<double> minimum_board_confidence;
    std::optional<double> minimum_hands_confidence;
    std::optional<double> minimum_side_to_move_confidence;
    std::optional<double> minimum_clock_confidence;
};
```

component thresholdが `nullopt` の場合、そのcomponent confidenceは必須でない。

例えば盤・持ち駒・手番は高confidenceを要求するが、時計を利用しないAIでは
clock confidenceを要求しない設定が可能。

低confidenceの場合、認識stateは診断用resultに残るが
`InternalBoardConverter` は呼ばれない。

## observe_into

reference pipeline:

```cpp
ObservationPipelineResult result = observe_into(
    source,
    recognizer,
    internal_board,
    policy
);
```

処理順:

1. policy検証
2. capture
3. frame shape検証
4. recognition
5. confidence値域検証
6. `ObservedGameState` 検証
7. acceptance threshold判定
8. `InternalBoardConverter` へ反映

status:

```cpp
enum class ObservationPipelineStatus {
    Converted,
    CaptureFailed,
    RecognitionFailed,
    LowConfidence,
    ConversionFailed,
};
```

pipeline resultはgeneric statusに加えて:

- `capture_failure`
- `recognition_failure`

も保持する。したがって `CaptureFailed` の中でも
`PermissionDenied` と `Unavailable` を区別でき、
`RecognitionFailed` の中でも `BoardNotFound`、
`MissingSideToMove`、`InvalidState` 等を区別できる。

この区別によりUIやautomationは:

- capture permissionを再要求する
- recognition対象windowを選び直す
- low confidence時だけ再captureする
- AI内部変換errorをpackage不具合として扱う

など別々の回復処理を行える。

## normalized state validation

`validate_observed_game_state()` はAPI経路と画面認識経路で共有する。

検証:

- SFENがvalid
- SFENのside-to-moveと明示fieldが一致
- clock値が非負整数またはnull

認識Adapterが不正なstateを返しても探索盤面へは到達させない。

## hidden dataを使わない

画面認識経路が生成してよいのは通常プレイヤーから観測可能な情報だけ。

- 盤面
- 持ち駒
- 手番
- 表示されている時計

次はこの共通stateに含めない。

- engine内部評価値
- legal move list
- check flagの内部判定結果
- repetition内部履歴
- hidden timer state
- game_id / dataset metadata
- opponent modelの内部状態

必要な合法手・check・履歴等はAI Packageが観測済み盤面から自前で計算・維持できる。

## 層境界

```text
OS / image source
       ↓
ObservationSource
       ↓
BoardRecognitionAdapter
       ↓
ObservedGameState
       ↓
AI Package internal board
       ↓
Search

Core / Runtime
   ↑
依存されてもよい

Core / Runtime
   ✕
screen recognitionへ依存しない
```

ゲームCoreにはcapture/OCR/CV依存を入れない。

## 非対象

このIssueでは実装しない。

- 特定将棋GUI向けrecognizer
- OCR model
- screen capture backend
- window locator
- mouse操作
- GUI automation
- Model Hub UI

これらは本interfaceのconsumer/pluginとして追加する。
