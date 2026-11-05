#include "kadoka/relabel/relabeler.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace kadoka::relabel {
namespace {

// {
//   責務: [validate_relabel_inputs: source provenanceとevaluator群の一意性を守る],
//   処理: [
//     1: 元レコード識別情報を検証する,
//     2: evaluator数、backend、version、config、ID重複を検証する
//   ],
//   引数: [source: 評価対象の元サンプル] [evaluators: 評価を依頼する個体一覧],
//   戻り値: [],
//   エラー: [契約違反をstd::invalid_argumentで通知する]
// }
void validate_relabel_inputs(const RelabelSource& source, std::span<const RelabelEvaluator> evaluators) {
    if (source.dataset_id.empty() || source.dataset_revision.empty() || source.sample_id.empty() ||
        source.position_sfen.empty()) {
        throw std::invalid_argument("relabel source identifiers and position are required");
    }
    if (evaluators.size() < 2)
        throw std::invalid_argument("at least two relabel evaluators are required");

    std::set<std::string> participant_ids;
    for (const RelabelEvaluator& evaluator : evaluators) {
        if (evaluator.participant.value.empty() || evaluator.model_version.empty() || evaluator.config_hash.empty() ||
            evaluator.backend == nullptr) {
            throw std::invalid_argument("each evaluator requires an ID, version, config hash, and backend");
        }
        if (!participant_ids.insert(evaluator.participant.value).second)
            throw std::invalid_argument("relabel evaluator participant IDs must be unique");
    }
}

} // namespace

// {
//   責務: [evaluate_relabel_sample: 各AIの再評価を独立して記録する],
//   処理: [1: 入力を検証する] [2: 同じ局面を各AIへ渡す] [3: 成否を記録する],
//   引数: [source: 元サンプル] [evaluators: 評価個体] [limits: 探索制限],
//   戻り値: [元サンプルと各個体の評価結果],
//   エラー: [入力不正時は例外を送出する]
// }
RelabelRecord evaluate_relabel_sample(const RelabelSource& source, std::span<const RelabelEvaluator> evaluators,
                                      const shogi::SearchLimits& limits) {
    validate_relabel_inputs(source, evaluators);
    const shogi::Position position = shogi::Position::from_sfen(source.position_sfen);

    RelabelRecord record{source, {}, limits};
    record.evaluations.reserve(evaluators.size());
    for (const RelabelEvaluator& evaluator : evaluators) {
        RelabelEvaluation evaluation{
            .participant = evaluator.participant,
            .model_version = evaluator.model_version,
            .config_hash = evaluator.config_hash,
        };
        try {
            const shogi::runtime::TurnResult result = shogi::runtime::run_ai_turn(*evaluator.backend, position, limits);
            evaluation.turn_status = result.status;
            evaluation.search_result = result.search_result;
            evaluation.decision_time = result.decision_time;
            if (result.status == shogi::runtime::TurnStatus::IllegalMove) {
                evaluation.status = RelabelEvaluationStatus::failed;
                evaluation.error = "backend proposed an illegal move";
            } else if (result.status == shogi::runtime::TurnStatus::NoLegalMoves) {
                evaluation.status = RelabelEvaluationStatus::failed;
                evaluation.error = "position has no legal moves; backend was not called";
            } else {
                evaluation.status = RelabelEvaluationStatus::completed;
            }
        } catch (const std::exception& error) {
            evaluation.status = RelabelEvaluationStatus::failed;
            evaluation.error = error.what();
        } catch (...) {
            evaluation.status = RelabelEvaluationStatus::failed;
            evaluation.error = "backend failed with a non-standard exception";
        }

        // 1個体ごとの失敗をrecordへ残し、後続evaluatorの結果を失わない。
        record.evaluations.push_back(std::move(evaluation));
    }
    return record;
}

} // namespace kadoka::relabel
