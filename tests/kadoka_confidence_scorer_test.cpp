#include "kadoka/confidence/confidence_scorer.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

// {
//   責務: [check: 条件がfalseならテストを失敗させる],
//   処理: [1: 失敗理由を例外で通知する],
//   引数: [condition: 検証条件] [message: 失敗理由],
//   戻り値: []
// }
void check(const bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [expect_invalid: scorerが不正入力を拒否することを検証する],
//   処理: [1: scoreを呼び、invalid_argumentが出ることを確認する],
//   引数: [scorer: 検証対象のscorer] [inputs: 不正入力],
//   戻り値: []
// }
void expect_invalid(const kadoka::confidence::ConfidenceScorer& scorer,
                    const kadoka::confidence::ConfidenceInputs& inputs) {
    try {
        (void)scorer.score(inputs);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid input should be rejected");
}

// {
//   責務: [main: confidence v1の式、元入力保持、入力検証を検査する],
//   処理: [
//     1: 指標平均と補数のgolden値を確認する,
//     2: source ratingがスコアを歪めないことを確認する,
//     3: 欠損根拠と不正値の拒否を確認する
//   ],
//   引数: [],
//   戻り値: [0: 成功] [1: 検査失敗]
// }
int main() {
    using kadoka::confidence::ConfidenceInputs;
    using kadoka::confidence::DefaultConfidenceScorer;

    try {
        const DefaultConfidenceScorer scorer;
        const ConfidenceInputs inputs{
            .source_rating = 1800.0,
            .source_uncertainty = 350.0,
            .search_depth = 8,
            .search_nodes = 1'000'000,
            .search_time_ms = 30'000,
            .candidate_gap_cp = 100.0,
            .policy_entropy = 0.2,
            .engine_disagreement = 0.1,
            .result_consistency = 0.75,
        };
        const auto result = scorer.score(inputs);
        check(result.evidence_count == 8, "all eight confidence signals should contribute");
        check(std::abs(result.confidence - 0.74375) < 1e-12, "equal-weight golden confidence changed");
        check(std::abs(result.uncertainty - (1.0 - result.confidence)) < 1e-12,
              "uncertainty should complement confidence");
        check(result.inputs.source_rating == inputs.source_rating
                  && result.inputs.candidate_gap_cp == inputs.candidate_gap_cp,
              "result should retain the original input values");

        auto changed_rating = inputs;
        changed_rating.source_rating = -120.0;
        check(scorer.score(changed_rating).confidence == result.confidence,
              "source rating level should be retained but must not imply reliability");

        const auto sparse = scorer.score(ConfidenceInputs{.search_depth = 0});
        check(sparse.evidence_count == 1 && sparse.confidence == 0.0,
              "zero search depth should remain valid, explicit evidence");

        expect_invalid(scorer, ConfidenceInputs{.source_rating = 1500.0});
        expect_invalid(scorer, ConfidenceInputs{.candidate_gap_cp = -1.0});
        expect_invalid(scorer, ConfidenceInputs{.policy_entropy = 1.1});
        expect_invalid(scorer,
                       ConfidenceInputs{.engine_disagreement = std::numeric_limits<double>::quiet_NaN()});
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
