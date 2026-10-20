#include "kadoka/relabel/priority_sampler.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using kadoka::confidence::DefaultConfidenceScorer;
using kadoka::relabel::ConfidenceBand;
using kadoka::relabel::PrioritySampleChoice;
using kadoka::relabel::PrioritySamplingCandidate;
using kadoka::relabel::PrioritySamplingPolicy;
using kadoka::relabel::sample_relabel_candidates;

// {
//   責務: [require: Releaseでも回帰条件を評価し、失敗理由を報告する],
//   処理: [1: 条件が偽なら例外を送出する],
//   引数: [condition: 期待条件] [message: 失敗理由],
//   戻り値: [],
//   エラー: [期待条件が偽ならruntime_error]
// }
void require(const bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [make_candidate: 実scorerと元ラベルを持つ再評価候補を用意する],
//   処理: [1: 元サンプル識別子とラベルを作る] [2: 結果整合度からconfidenceを計算する],
//   引数: [id: サンプルID] [confidence: 結果整合度として与える信頼度],
//   戻り値: [candidate: sourceとscorer結果を含む候補]
// }
PrioritySamplingCandidate make_candidate(const std::string& id, const double confidence) {
    PrioritySamplingCandidate candidate;
    candidate.source.dataset_id = "dataset";
    candidate.source.dataset_revision = "revision-1";
    candidate.source.sample_id = id;
    candidate.source.position_sfen = "4k4/9/9/9/9/9/9/9/4K4 b - 1";
    candidate.source.observed_move_usi = "5i5h";
    candidate.source.observed_score_cp = 17;
    kadoka::confidence::ConfidenceInputs inputs;
    inputs.result_consistency = confidence;
    candidate.assessment = DefaultConfidenceScorer{}.score(inputs);
    return candidate;
}

// {
//   責務: [selected_ids: 元配列indexをサンプルIDへ戻して抽選順を比較可能にする],
//   処理: [1: 全choiceのindexを検査する] [2: 対応するsample IDを返す],
//   引数: [candidates: 元候補] [choices: 抽選結果],
//   戻り値: [ids: 抽選順のsample ID]
// }
std::vector<std::string> selected_ids(const std::vector<PrioritySamplingCandidate>& candidates,
                                      const std::vector<PrioritySampleChoice>& choices) {
    std::vector<std::string> ids;
    for (const auto& choice : choices) {
        require(choice.candidate_index < candidates.size(), "choice has an invalid input index");
        ids.push_back(candidates[choice.candidate_index].source.sample_id);
    }
    return ids;
}

// {
//   責務: [expect_invalid: 抽選数0でも不正な候補・設定が拒否されることを確認する],
//   処理: [1: 不正入力で抽選する] [2: invalid_argumentで拒否されたことを確認する],
//   引数: [candidates: 検査する候補] [policy: 検査する設定],
//   戻り値: [],
//   エラー: [拒否されない場合はruntime_error]
// }
void expect_invalid(const std::vector<PrioritySamplingCandidate>& candidates,
                    const PrioritySamplingPolicy& policy = {}) {
    bool rejected = false;
    try {
        (void)sample_relabel_candidates(candidates, 0, 1, policy);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "invalid sampler input was accepted");
}

} // namespace

// {
//   責務: [main: seed再現性、抽選分布、除外、入力検証、元ラベル非破壊を確認する],
//   処理: [1: 帯と重みの契約を確認する] [2: 再現性と非復元抽選を確認する]
//         [3: 監査・disagreement加算・無効入力を確認する],
//   引数: [],
//   戻り値: [int: 成功0、失敗1]
// }
int main() {
    try {
        const std::vector<PrioritySamplingCandidate> candidates = {
            make_candidate("low", 0.2), make_candidate("medium", 0.6), make_candidate("high", 0.95)};
        const auto choices = sample_relabel_candidates(candidates, 100, 42);
        const auto ids = selected_ids(candidates, choices);
        require(choices.size() == 3 && std::set<std::string>(ids.begin(), ids.end()).size() == 3,
                "sampling did not cap at eligible candidates without replacement");
        require(ids == selected_ids(candidates, sample_relabel_candidates(candidates, 100, 42)),
                "same seed did not reproduce the draw order");
        const std::vector<std::string> golden = {"high", "low", "medium"};
        require(ids == golden, "seed-42 golden order changed: " + ids[0] + "," + ids[1] + "," + ids[2]);
        auto reversed = candidates;
        std::reverse(reversed.begin(), reversed.end());
        require(ids == selected_ids(reversed, sample_relabel_candidates(reversed, 100, 42)),
                "input iteration order changed the selected sample identities");
        for (const auto& choice : choices) {
            const std::array<std::uint64_t, 3> tickets = {8192, 3072, 1024};
            const std::array<ConfidenceBand, 3> bands = {ConfidenceBand::low, ConfidenceBand::medium,
                                                         ConfidenceBand::high};
            require(choice.tickets == tickets[choice.candidate_index] && choice.band == bands[choice.candidate_index],
                    "default confidence band or integer weight is incorrect");
        }
        require(sample_relabel_candidates(candidates, 0, 42).empty(), "zero count produced choices");
        require(sample_relabel_candidates({}, 5, 42).empty(), "empty input produced choices");

        const std::vector<PrioritySamplingCandidate> boundaries = {make_candidate("low-edge", 0.4),
                                                                   make_candidate("high-edge", 0.8)};
        for (const auto& choice : sample_relabel_candidates(boundaries, 2, 42))
            require(choice.band == (choice.candidate_index == 0 ? ConfidenceBand::medium : ConfidenceBand::high),
                    "confidence threshold equality was classified incorrectly");

        PrioritySamplingPolicy policy;
        policy.low_weight = 0;
        policy.medium_weight = 0;
        policy.high_audit_weight = 5;
        const auto audit_only = sample_relabel_candidates(candidates, 10, 42, policy);
        require(audit_only.size() == 1 && audit_only.front().candidate_index == 2 && audit_only.front().tickets == 5120,
                "custom weights did not exclude disabled bands");
        require(sample_relabel_candidates(std::span(candidates).first(2), 10, 42, policy).empty(),
                "disabled candidates were selected");

        std::vector<PrioritySamplingCandidate> sparse_pool;
        for (std::size_t index = 0; index < 19; ++index)
            sparse_pool.push_back(make_candidate("sample-" + std::to_string(index), index % 3 == 0 ? 0.95 : 0.2));
        policy = {};
        policy.low_weight = 0;
        const auto sparse_choices = sample_relabel_candidates(sparse_pool, 100, 19, policy);
        const auto sparse_ids = selected_ids(sparse_pool, sparse_choices);
        require(sparse_choices.size() == 7 && std::set<std::string>(sparse_ids.begin(), sparse_ids.end()).size() == 7,
                "sparse non-power-of-two pool was not exhausted without duplicates");
        for (const auto& choice : sparse_choices)
            require(choice.candidate_index % 3 == 0, "sparse pool included a zero-weight candidate");

        std::array<std::size_t, 3> counts{};
        for (std::uint64_t seed = 0; seed < 4096; ++seed)
            ++counts[sample_relabel_candidates(candidates, 1, seed).front().candidate_index];
        require(counts[0] > 2 * counts[1] && counts[1] > 2 * counts[2] && counts[2] > 0,
                "low-confidence priority, medium mixing or random high-confidence audit is missing");

        auto boost_candidates = std::vector{make_candidate("plain", 0.5), make_candidate("disagreement", 0.5)};
        kadoka::confidence::ConfidenceInputs plain_inputs;
        plain_inputs.engine_disagreement = 0.0;
        plain_inputs.result_consistency = 0.0;
        kadoka::confidence::ConfidenceInputs disagreement_inputs;
        disagreement_inputs.engine_disagreement = 1.0;
        disagreement_inputs.result_consistency = 1.0;
        boost_candidates[0].assessment = DefaultConfidenceScorer{}.score(plain_inputs);
        boost_candidates[1].assessment = DefaultConfidenceScorer{}.score(disagreement_inputs);
        std::array<std::size_t, 2> boost_counts{};
        for (std::uint64_t seed = 0; seed < 4096; ++seed)
            ++boost_counts[sample_relabel_candidates(boost_candidates, 1, seed).front().candidate_index];
        require(boost_counts[1] > 2 * boost_counts[0], "disagreement did not increase selection probability");
        for (const auto& choice : sample_relabel_candidates(boost_candidates, 2, 42))
            require(choice.tickets == (choice.candidate_index == 0 ? 3072 : 7168),
                    "disagreement tickets do not match the documented scale");
        policy = {};
        policy.medium_weight = std::numeric_limits<std::uint32_t>::max();
        policy.disagreement_boost = std::numeric_limits<std::uint32_t>::max();
        for (const auto& choice : sample_relabel_candidates(boost_candidates, 2, 42, policy)) {
            const std::uint64_t scale = choice.candidate_index == 0 ? 1024 : 2048;
            require(choice.tickets == static_cast<std::uint64_t>(policy.medium_weight) * scale,
                    "maximum configured weight overflowed before widening to uint64_t");
        }
        auto half_ticket = std::vector{make_candidate("half-ticket", 0.5)};
        kadoka::confidence::ConfidenceInputs half_ticket_inputs;
        half_ticket_inputs.engine_disagreement = 1.0 / 2048.0;
        half_ticket_inputs.result_consistency = 1.0 / 2048.0;
        half_ticket.front().assessment = DefaultConfidenceScorer{}.score(half_ticket_inputs);
        require(sample_relabel_candidates(half_ticket, 1, 42).front().tickets == 3076,
                "half-ticket disagreement was not rounded upward");
        policy = {};
        policy.medium_weight = 0;
        require(sample_relabel_candidates(boost_candidates, 2, 42, policy).empty(),
                "disagreement boost re-enabled a disabled band");

        auto duplicate = candidates;
        duplicate.push_back(candidates.front());
        expect_invalid(duplicate);
        duplicate.back().source.dataset_revision = "revision-2";
        require(sample_relabel_candidates(duplicate, 100, 1).size() == 4, "distinct dataset revisions were conflated");
        duplicate.back().source.dataset_revision = "revision-1";
        duplicate.back().source.dataset_id = "other-dataset";
        require(sample_relabel_candidates(duplicate, 100, 1).size() == 4, "distinct dataset IDs were conflated");
        auto invalid = candidates;
        invalid.front().source.sample_id.clear();
        expect_invalid(invalid);
        invalid = candidates;
        invalid.front().assessment.evidence_count = 0;
        expect_invalid(invalid);
        invalid = candidates;
        invalid.front().assessment.uncertainty = 0.1;
        expect_invalid(invalid);
        for (const double bad :
             {-0.1, 1.1, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            invalid = candidates;
            invalid.front().assessment.confidence = bad;
            expect_invalid(invalid);
            invalid = candidates;
            invalid.front().assessment.inputs.engine_disagreement = bad;
            expect_invalid(invalid);
            policy = {};
            policy.low_confidence_threshold = bad;
            expect_invalid(candidates, policy);
        }
        policy = {};
        policy.high_confidence_threshold = policy.low_confidence_threshold;
        expect_invalid(candidates, policy);
        policy = {};
        policy.low_weight = policy.medium_weight = policy.high_audit_weight = 0;
        expect_invalid(candidates, policy);

        require(candidates[0].source.observed_move_usi == "5i5h" && candidates[0].source.observed_score_cp == 17 &&
                    candidates[0].assessment.confidence == 0.2 &&
                    candidates[0].assessment.inputs.result_consistency == 0.2,
                "sampling changed source labels or confidence evidence");
        std::cout << "kadoka_priority_sampler_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
