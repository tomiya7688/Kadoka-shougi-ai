#include "kadoka/relabel/priority_sampler.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

namespace kadoka::relabel {
namespace {

constexpr std::uint64_t kTicketScale = 1024;

// {
//   責務: [require_probability: 信頼度・境界・disagreementを有限な0から1へ制限する],
//   処理: [1: 有限性と閉区間の範囲を検証する],
//   引数: [value: 検査値] [name: エラーに含める項目名],
//   戻り値: [],
//   エラー: [範囲外または非有限値ならinvalid_argument]
// }
void require_probability(const double value, const std::string_view name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0)
        throw std::invalid_argument(std::string(name) + " must be finite and within [0, 1]");
}

// {
//   責務: [make_choice: 候補の入力を検証し信頼度帯と整数の抽選券を割り当てる],
//   処理: [1: 識別子・信頼度・根拠を検証する] [2: 帯を決めてdisagreement加算を量子化する],
//   引数: [candidate: 元サンプルとscorer結果] [index: 元配列index] [policy: 抽選設定],
//   戻り値: [choice: 元index・帯・抽選券数],
//   エラー: [識別子や信頼度が不正ならinvalid_argument]
// }
PrioritySampleChoice make_choice(const PrioritySamplingCandidate& candidate, const std::size_t index,
                                 const PrioritySamplingPolicy& policy) {
    const auto& source = candidate.source;
    if (source.dataset_id.empty() || source.dataset_revision.empty() || source.sample_id.empty())
        throw std::invalid_argument("sampling candidate requires dataset, revision and sample IDs");
    const auto& assessment = candidate.assessment;
    require_probability(assessment.confidence, "confidence");
    require_probability(assessment.uncertainty, "uncertainty");
    if (assessment.evidence_count == 0 || std::abs(assessment.confidence + assessment.uncertainty - 1.0) > 1e-12)
        throw std::invalid_argument("sampling candidate requires scored evidence and complementary uncertainty");
    const double disagreement = assessment.inputs.engine_disagreement.value_or(0.0);
    require_probability(disagreement, "engine_disagreement");

    ConfidenceBand band = ConfidenceBand::high;
    std::uint32_t weight = policy.high_audit_weight;
    if (assessment.confidence < policy.low_confidence_threshold) {
        band = ConfidenceBand::low;
        weight = policy.low_weight;
    } else if (assessment.confidence < policy.high_confidence_threshold) {
        band = ConfidenceBand::medium;
        weight = policy.medium_weight;
    }

    // 重み0の帯をboostで復活させず、設定された除外を優先する。
    std::uint64_t tickets = 0;
    if (weight != 0) {
        const auto disagreement_units = static_cast<std::uint64_t>(std::round(disagreement * kTicketScale));
        tickets = static_cast<std::uint64_t>(weight) * kTicketScale +
                  static_cast<std::uint64_t>(policy.disagreement_boost) * disagreement_units;
    }
    return {index, band, tickets};
}

// {
//   責務: [TicketTree: 重み付き抽選と選択済み候補の除外を対数時間で行う],
//   フィールド: [entries_: 抽選中に変更しない候補への非所有view]
//               [tree_: 抽選券数のFenwick tree] [total_: 未選択候補の抽選券総数]
// }
class TicketTree {
  public:
    // {
    //   責務: [TicketTree: 整数抽選券の総和を検証し累積木を構築する],
    //   処理: [1: uint64_t総和のoverflowを拒否する] [2: 部分和を線形時間で構築する],
    //   引数: [entries: 識別子順の候補。木の生存中は元配列を保持する],
    //   戻り値: [],
    //   エラー: [抽選券総和が表現不能ならoverflow_error]
    // }
    explicit TicketTree(const std::span<const PrioritySampleChoice> entries)
        : entries_(entries), tree_(entries.size() + 1, 0) {
        for (const auto& entry : entries_) {
            if (entry.tickets > std::numeric_limits<std::uint64_t>::max() - total_)
                throw std::overflow_error("priority sampling ticket total exceeds uint64_t");
            total_ += entry.tickets;
        }
        for (std::size_t index = 1; index < tree_.size(); ++index) {
            tree_[index] += entries_[index - 1].tickets;
            const std::size_t parent = index + (index & (~index + 1));
            if (parent < tree_.size())
                tree_[parent] += tree_[index];
        }
    }

    // {
    //   責務: [total: 未選択候補に残る抽選券数を返す],
    //   処理: [1: 現在の総和を返す],
    //   引数: [],
    //   戻り値: [uint64_t: 残存抽選券数]
    // }
    [[nodiscard]] std::uint64_t total() const {
        return total_;
    }

    // {
    //   責務: [take: 指定抽選券を所有する候補を選び、その重みを木から除去する],
    //   処理: [1: 部分和から候補を探索する] [2: 重みを除去して重複選択を防ぐ],
    //   引数: [ticket: 0以上total未満の抽選券番号],
    //   戻り値: [size_t: entries内の選択index],
    //   エラー: [内部呼出しの範囲違反はlogic_error]
    // }
    std::size_t take(std::uint64_t ticket) {
        if (ticket >= total_)
            throw std::logic_error("priority sampling ticket is outside the remaining range");
        std::size_t index = 0;
        for (std::size_t stride = std::bit_floor(entries_.size()); stride != 0; stride >>= 1) {
            const std::size_t next = index + stride;
            if (next < tree_.size() && tree_[next] <= ticket) {
                ticket -= tree_[next];
                index = next;
            }
        }
        const std::uint64_t weight = entries_[index].tickets;
        for (std::size_t node = index + 1; node < tree_.size(); node += node & (~node + 1))
            tree_[node] -= weight;
        total_ -= weight;
        return index;
    }

  private:
    std::span<const PrioritySampleChoice> entries_;
    std::vector<std::uint64_t> tree_;
    std::uint64_t total_ = 0;
};

// {
//   責務: [draw_below: 標準乱数出力を剰余偏りのない整数範囲へ写す],
//   処理: [1: 2の64乗で均等に割れない余り領域を棄却する] [2: 有効出力の剰余を返す],
//   引数: [generator: seed固定の標準乱数生成器] [bound: 正の排他的上限],
//   戻り値: [uint64_t: 0以上bound未満の一様乱数]
// }
std::uint64_t draw_below(std::mt19937_64& generator, const std::uint64_t bound) {
    const std::uint64_t rejection_threshold = (std::uint64_t{0} - bound) % bound;
    std::uint64_t value = 0;
    do {
        value = generator();
    } while (value < rejection_threshold);
    return value % bound;
}

} // namespace

// {
//   責務: [sample_relabel_candidates: 元サンプルを変更せずseed付きで再評価対象を選ぶ],
//   処理: [1: 設定と全候補を検証する] [2: 一意な元識別子順へ正規化する]
//         [3: 整数の重み付き非復元抽選を行い、元配列へのindexを返す],
//   引数: [candidates: 元データと信頼度] [request: 最大抽選数と再現用seed] [policy: 抽選設定],
//   戻り値: [choices: 抽選順の結果。候補不足なら正重み候補全件],
//   エラー: [不正設定・入力・識別子重複はinvalid_argument、総重みoverflowはoverflow_error]
// }
std::vector<PrioritySampleChoice> sample_relabel_candidates(const std::span<const PrioritySamplingCandidate> candidates,
                                                            const PrioritySamplingRequest& request,
                                                            const PrioritySamplingPolicy& policy) {
    require_probability(policy.low_confidence_threshold, "low_confidence_threshold");
    require_probability(policy.high_confidence_threshold, "high_confidence_threshold");
    if (policy.low_confidence_threshold >= policy.high_confidence_threshold)
        throw std::invalid_argument("low confidence threshold must be below high confidence threshold");
    if (policy.low_weight == 0 && policy.medium_weight == 0 && policy.high_audit_weight == 0)
        throw std::invalid_argument("at least one confidence band must have positive weight");

    std::vector<PrioritySampleChoice> ordered;
    ordered.reserve(candidates.size());
    std::size_t eligible_count = 0;
    std::uint64_t ticket_total = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto choice = make_choice(candidates[index], index, policy);
        if (choice.tickets > std::numeric_limits<std::uint64_t>::max() - ticket_total)
            throw std::overflow_error("priority sampling ticket total exceeds uint64_t");
        ticket_total += choice.tickets;
        ordered.push_back(choice);
        if (choice.tickets != 0)
            ++eligible_count;
    }
    std::sort(ordered.begin(), ordered.end(), [&candidates](const auto& left, const auto& right) {
        const auto& a = candidates[left.candidate_index].source;
        const auto& b = candidates[right.candidate_index].source;
        return std::tie(a.dataset_id, a.dataset_revision, a.sample_id) <
               std::tie(b.dataset_id, b.dataset_revision, b.sample_id);
    });
    for (std::size_t index = 1; index < ordered.size(); ++index) {
        const auto& a = candidates[ordered[index - 1].candidate_index].source;
        const auto& b = candidates[ordered[index].candidate_index].source;
        if (std::tie(a.dataset_id, a.dataset_revision, a.sample_id) ==
            std::tie(b.dataset_id, b.dataset_revision, b.sample_id))
            throw std::invalid_argument("duplicate dataset/revision/sample identity in sampling candidates");
    }

    std::vector<PrioritySampleChoice> choices;
    const std::size_t limit = std::min(request.count, eligible_count);
    choices.reserve(limit);
    if (limit == 0)
        return choices;
    TicketTree tree(ordered);
    std::mt19937_64 generator(request.seed);
    for (std::size_t selected = 0; selected < limit; ++selected) {
        const std::size_t index = tree.take(draw_below(generator, tree.total()));
        choices.push_back(ordered[index]);
    }
    return choices;
}

} // namespace kadoka::relabel
