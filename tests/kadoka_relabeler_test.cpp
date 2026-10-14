#include "kadoka/relabel/relabeler.hpp"

#include "kadoka/runtime/ai_backend.hpp"
#include "kadoka/runtime/player_api.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
//   責務: [expect_invalid: 評価入力の契約違反を検出する],
//   処理: [1: APIを呼び、invalid_argumentを確認する],
//   引数: [source: 元サンプル] [evaluators: 評価個体一覧],
//   戻り値: []
// }
void expect_invalid(const kadoka::relabel::RelabelSource& source,
                    const std::vector<kadoka::relabel::RelabelEvaluator>& evaluators) {
    try {
        (void)kadoka::relabel::evaluate_relabel_sample(source, evaluators);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid relabel input should be rejected");
}

// {
//   責務: [main: 複数AIの追記、元ラベル保持、個別失敗後の継続を検査する],
//   処理: [
//     1: 2つのAI結果が別々に追加されることを確認する,
//     2: 失敗したAIの後も他AIの結果が残ることを確認する,
//     3: 元ラベル、重複ID、評価個体数の契約を確認する
//   ],
//   引数: [],
//   戻り値: [0: 成功] [1: 検査失敗]
// }
int main() {
    using namespace kadoka;
    try {
        const shogi::Position initial = shogi::Position::startpos();
        const relabel::RelabelSource source{
            .dataset_id = "dataset-2026-01",
            .dataset_revision = "revision-7",
            .sample_id = "game-42-ply-10",
            .position_sfen = initial.to_sfen(),
            .observed_move_usi = "2g2f",
            .observed_score_cp = 24,
        };

        runtime::FunctionAIBackend first_backend(
            runtime::AIBackendKind::Native,
            "first",
            [](const shogi::Position&, const shogi::SearchLimits&) {
                shogi::SearchResult result;
                result.best_move = runtime::move_from_usi("7g7f");
                result.score_cp = 35;
                result.nodes = 100;
                result.depth = 4;
                return result;
            });
        runtime::FunctionAIBackend second_backend(
            runtime::AIBackendKind::ExternalProcess,
            "second",
            [](const shogi::Position&, const shogi::SearchLimits&) {
                shogi::SearchResult result;
                result.best_move = runtime::move_from_usi("3c3d");
                result.score_cp = -12;
                result.nodes = 250;
                result.depth = 5;
                return result;
            });
        runtime::FunctionAIBackend failing_backend(
            runtime::AIBackendKind::Script,
            "failing",
            [](const shogi::Position&, const shogi::SearchLimits&) -> shogi::SearchResult {
                throw std::runtime_error("evaluation transport failed");
            });

        const std::vector<relabel::RelabelEvaluator> evaluators{
            {best::ParticipantId{"sha256:1111111111111111111111111111111111111111111111111111111111111111"},
             "model-v1",
             "config-a",
             &first_backend},
            {best::ParticipantId{"sha256:2222222222222222222222222222222222222222222222222222222222222222"},
             "model-v2",
             "config-b",
             &second_backend},
        };
        const auto record = relabel::evaluate_relabel_sample(source, evaluators);
        check(record.evaluations.size() == 2, "both evaluator results should be appended");
        check(record.evaluations[0].status == relabel::RelabelEvaluationStatus::completed
                  && record.evaluations[1].status == relabel::RelabelEvaluationStatus::completed,
              "successful backends should complete");
        check(record.evaluations[0].search_result->score_cp == 35
                  && record.evaluations[1].search_result->score_cp == -12,
              "per-engine scores should remain distinct");
        check(runtime::move_to_usi(record.evaluations[0].search_result->best_move)
                  != runtime::move_to_usi(record.evaluations[1].search_result->best_move),
              "engine disagreement should remain observable in the appended results");
        check(record.source.sample_id == source.sample_id && record.source.dataset_revision == source.dataset_revision
                  && record.source.observed_move_usi == source.observed_move_usi
                  && record.source.observed_score_cp == source.observed_score_cp
                  && record.source.position_sfen == source.position_sfen,
              "relabel output should preserve the original sample and provenance");

        const std::vector<relabel::RelabelEvaluator> with_failure{
            evaluators[0],
            {best::ParticipantId{"sha256:3333333333333333333333333333333333333333333333333333333333333333"},
             "model-v3",
             "config-c",
             &failing_backend},
            evaluators[1],
        };
        const auto partial = relabel::evaluate_relabel_sample(source, with_failure);
        check(partial.evaluations.size() == 3, "one backend failure should not discard other results");
        check(partial.evaluations[1].status == relabel::RelabelEvaluationStatus::failed
                  && partial.evaluations[1].error == "evaluation transport failed",
              "backend failure should be retained with evaluator provenance");
        check(partial.evaluations[2].status == relabel::RelabelEvaluationStatus::completed,
              "evaluation should continue after a backend failure");

        expect_invalid(source, {evaluators[0]});
        expect_invalid(source, {evaluators[0], evaluators[0]});

        auto missing_backend = evaluators;
        missing_backend[1].backend = nullptr;
        expect_invalid(source, missing_backend);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
