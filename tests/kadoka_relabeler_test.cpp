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
//   責務: [make_participant_id: テスト用の一意なParticipantIdを作る],
//   処理: [1: 64桁hex suffixとsha256 prefixを組み立てる],
//   引数: [digit: suffixの構成文字],
//   戻り値: [participant: 再現可能なテストID]
// }
kadoka::best::ParticipantId make_participant_id(const char digit) {
    return {"sha256:" + std::string(64, digit)};
}

// {
//   責務: [make_backend: 指定着手と評価値を返すテストbackendを作る],
//   処理: [1: 1つの決定関数を持つFunctionAIBackendを構成する],
//   引数: [name: backend名] [move_usi: 合法な着手] [score: 返す評価値],
//   戻り値: [backend: 決定論的なテストbackend]
// }
kadoka::shogi::runtime::FunctionAIBackend make_backend(
    std::string name,
    std::string move_usi,
    const std::int32_t score) {
    return {
        kadoka::shogi::runtime::AIBackendKind::Native,
        std::move(name),
        [move_usi = std::move(move_usi), score](
            const kadoka::shogi::Position&,
            const kadoka::shogi::SearchLimits&) {
            kadoka::shogi::SearchResult result;
            result.best_move = kadoka::shogi::runtime::move_from_usi(move_usi);
            result.score_cp = score;
            result.nodes = 100;
            result.depth = 4;
            return result;
        }};
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

        auto first_backend = make_backend("first", "7g7f", 35);
        auto second_backend = make_backend("second", "3c3d", -12);
        runtime::FunctionAIBackend failing_backend(
            runtime::AIBackendKind::Script,
            "failing",
            [](const shogi::Position&, const shogi::SearchLimits&) -> shogi::SearchResult {
                throw std::runtime_error("evaluation transport failed");
            });

        const std::vector<relabel::RelabelEvaluator> evaluators{
            {make_participant_id('1'), "model-v1", "config-a", &first_backend},
            {make_participant_id('2'), "model-v2", "config-b", &second_backend},
        };
        const auto record = relabel::evaluate_relabel_sample(source, evaluators);
        check(record.evaluations.size() == 2, "both evaluator results should be appended");

        const bool evaluations_completed =
            record.evaluations[0].status == relabel::RelabelEvaluationStatus::completed &&
            record.evaluations[1].status == relabel::RelabelEvaluationStatus::completed;
        check(evaluations_completed, "successful backends should complete");

        const bool scores_remain_distinct =
            record.evaluations[0].search_result->score_cp == 35 &&
            record.evaluations[1].search_result->score_cp == -12;
        check(scores_remain_distinct, "per-engine scores should remain distinct");

        const bool moves_remain_distinct =
            runtime::move_to_usi(record.evaluations[0].search_result->best_move) !=
            runtime::move_to_usi(record.evaluations[1].search_result->best_move);
        check(moves_remain_distinct, "engine disagreement should remain observable in the appended results");

        const bool source_is_preserved =
            record.source.sample_id == source.sample_id &&
            record.source.dataset_revision == source.dataset_revision &&
            record.source.observed_move_usi == source.observed_move_usi &&
            record.source.observed_score_cp == source.observed_score_cp &&
            record.source.position_sfen == source.position_sfen;
        check(source_is_preserved, "relabel output should preserve the original sample and provenance");

        const std::vector<relabel::RelabelEvaluator> with_failure{
            evaluators[0],
            {make_participant_id('3'), "model-v3", "config-c", &failing_backend},
            evaluators[1],
        };
        const auto partial = relabel::evaluate_relabel_sample(source, with_failure);
        check(partial.evaluations.size() == 3, "one backend failure should not discard other results");

        const bool failure_is_stored =
            partial.evaluations[1].status == relabel::RelabelEvaluationStatus::failed &&
            partial.evaluations[1].error == "evaluation transport failed";
        check(failure_is_stored, "backend failure should be retained with evaluator provenance");
        check(partial.evaluations[2].status == relabel::RelabelEvaluationStatus::completed,
              "evaluation should continue after a backend failure");

        const std::vector<relabel::RelabelEvaluator> one_evaluator{evaluators[0]};
        const std::vector<relabel::RelabelEvaluator> duplicate_evaluators{
            evaluators[0], evaluators[0]};
        expect_invalid(source, one_evaluator);
        expect_invalid(source, duplicate_evaluators);

        auto missing_backend = evaluators;
        missing_backend[1].backend = nullptr;
        expect_invalid(source, missing_backend);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
