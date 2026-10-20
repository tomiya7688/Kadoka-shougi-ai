#pragma once

#include "kadoka/confidence/confidence_scorer.hpp"
#include "kadoka/relabel/relabeler.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace kadoka::relabel {

// {
//   責務: [ConfidenceBand: 再評価対象を低信頼・中信頼・高信頼監査へ分類する],
//   フィールド: [low: 低信頼] [medium: 中信頼] [high: 高信頼の監査対象]
// }
enum class ConfidenceBand : std::uint8_t { low, medium, high };

// {
//   責務: [PrioritySamplingRequest: 抽選数とseedを名前付きで指定し、引数の取り違えを防ぐ],
//   フィールド: [count: 最大抽選数] [seed: 再現用の乱数初期値]
// }
struct PrioritySamplingRequest {
    std::size_t count = 0;
    std::uint64_t seed = 0;
};

// {
//   責務: [PrioritySamplingPolicy: 信頼度の境界と再評価抽選の重みを設定する],
//   フィールド: [
//     low_confidence_threshold: この値未満を低信頼とする境界,
//     high_confidence_threshold: この値以上を高信頼とする境界,
//     low_weight: 低信頼の基本重み。0は除外,
//     medium_weight: 中信頼を混入する基本重み。0は除外,
//     high_audit_weight: 高信頼を無作為監査する基本重み。0は除外,
//     disagreement_boost: disagreementが1のとき加える重み
//   ]
// }
struct PrioritySamplingPolicy {
    double low_confidence_threshold = 0.4;
    double high_confidence_threshold = 0.8;
    std::uint32_t low_weight = 8;
    std::uint32_t medium_weight = 3;
    std::uint32_t high_audit_weight = 1;
    std::uint32_t disagreement_boost = 4;
};

// {
//   責務: [PrioritySamplingCandidate: 元サンプルと保存済みの信頼度根拠を結び付ける],
//   フィールド: [source: 再評価APIへ渡す元サンプル] [assessment: scorerの結果と元入力]
// }
struct PrioritySamplingCandidate {
    RelabelSource source;
    confidence::ConfidenceResult assessment;
};

// {
//   責務: [PrioritySampleChoice: 抽選結果を元配列へ結び付け、採用理由の重みを保持する],
//   フィールド: [candidate_index: 元配列のindex] [band: 信頼度帯] [tickets: 抽選時の整数重み]
// }
struct PrioritySampleChoice {
    std::size_t candidate_index = 0;
    ConfidenceBand band = ConfidenceBand::low;
    std::uint64_t tickets = 0;
};

// {
//   責務: [sample_relabel_candidates: 信頼度とdisagreementに従い再評価対象を重複なく抽選する],
//   処理: [
//     1: 設定と全候補を検証しdataset・revision・sample ID順へ並べる,
//     2: 信頼度帯とdisagreementから整数の抽選券を割り当てる,
//     3: seed固定の重み付き非復元抽選で元配列indexを返す
//   ],
//   引数: [candidates: 元ラベルと信頼度を保持する候補] [request: 抽選数と再現用seed]
//         [policy: 信頼度境界と抽選重み],
//   戻り値: [choices: 抽選順の候補index・帯・抽選券数。正重み候補が足りなければその全件],
//   エラー: [不正値・空ID・同一dataset/revision/sampleの重複はinvalid_argument]
//           [抽選券の総和がuint64_tを超える場合はoverflow_error],
//   副作用: [元候補・ラベル・信頼度入力を変更しない]
// }
[[nodiscard]] std::vector<PrioritySampleChoice>
sample_relabel_candidates(std::span<const PrioritySamplingCandidate> candidates, const PrioritySamplingRequest& request,
                          const PrioritySamplingPolicy& policy = {});

} // namespace kadoka::relabel
