#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace kadoka::confidence {

// {
//   責務: [ConfidenceInputs: 元データ由来の信頼度要素を、未加工値のまま保持する]
//   フィールド: [
//     source_rating: 出典個体のrating。結果へ保持するが信頼度計算には使わない,
//     source_uncertainty: 出典個体のrating deviation,
//     search_depth: 評価を得た探索深さ,
//     search_nodes: 評価を得た探索ノード数,
//     search_time_ms: 評価に使った時間（ミリ秒）,
//     candidate_gap_cp: 最善候補と次点候補の評価値差（centipawn）,
//     policy_entropy: 正規化済みpolicy entropy（0から1）,
//     engine_disagreement: 正規化済みengine disagreement（0から1）,
//     result_consistency: 結果との整合度（0から1）
//   ]
// }
struct ConfidenceInputs {
    std::optional<double> source_rating;
    std::optional<double> source_uncertainty;
    std::optional<std::uint32_t> search_depth;
    std::optional<std::uint64_t> search_nodes;
    std::optional<std::uint64_t> search_time_ms;
    std::optional<double> candidate_gap_cp;
    std::optional<double> policy_entropy;
    std::optional<double> engine_disagreement;
    std::optional<double> result_consistency;
};

// {
//   責務: [ConfidenceResult: 信頼度計算結果と計算元の全入力を保持する]
//   フィールド: [
//     inputs: 再利用・監査向けに保持する元入力,
//     confidence: 0から1の信頼度,
//     uncertainty: confidenceの補数,
//     evidence_count: 計算に寄与した要素数
//   ]
// }
struct ConfidenceResult {
    ConfidenceInputs inputs;
    double confidence = 0.0;
    double uncertainty = 1.0;
    std::size_t evidence_count = 0;
};

// {
//   責務: [ConfidenceScorer: 信頼度計算方式を差し替える境界を提供する]
//   処理: [1: 入力要素から再現可能なConfidenceResultを計算する],
//   フィールド: []
// }
class ConfidenceScorer {
  public:
    // {
    //   責務: [~ConfidenceScorer: 派生scorerを基底型経由で安全に破棄する],
    //   処理: [],
    //   引数: [],
    //   戻り値: []
    // }
    virtual ~ConfidenceScorer() = default;

    // {
    //   責務: [score: 元入力を保持したconfidenceとuncertaintyを返す],
    //   処理: [
    //     1: 入力範囲を検証する,
    //     2: 使用可能な根拠を集約する,
    //     3: 根拠がなければ失敗し、結果を返す
    //   ],
    //   引数: [inputs: スコア対象の根拠と出典情報],
    //   戻り値: [result: 0から1のconfidence、補数uncertainty、元入力],
    //   エラー: [無効値または信頼度根拠がない場合はstd::invalid_argument]
    // }
    [[nodiscard]] virtual ConfidenceResult score(const ConfidenceInputs& inputs) const = 0;
};

// {
//   責務: [DefaultConfidenceScorer: 探索・評価指標を等重みで集約するv1実装],
//   フィールド: []
// }
class DefaultConfidenceScorer final : public ConfidenceScorer {
  public:
    // {
    //   責務: [score: 固定式でConfidenceInputsを決定論的に集約する],
    //   処理: [
    //     1: 全入力の有限性と範囲を検証する,
    //     2: 利用可能な指標を0から1へ正規化する,
    //     3: 指標の単純平均とその補数を返す
    //   ],
    //   引数: [inputs: 信頼度根拠と保持する出典rating],
    //   戻り値: [result: 入力を保持したconfidence計算結果],
    //   エラー: [無効値または信頼度根拠がない場合はstd::invalid_argument]
    // }
    [[nodiscard]] ConfidenceResult score(const ConfidenceInputs& inputs) const override;
};

} // namespace kadoka::confidence
