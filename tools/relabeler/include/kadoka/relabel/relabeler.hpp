#pragma once

#include "kadoka/best/participant_registry.hpp"
#include "kadoka/runtime/turn_runner.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kadoka::relabel {

// {
//   責務: [RelabelSource: 元サンプル識別子、盤面、既存ラベル、出典revisionを保持する],
//   フィールド: [
//     dataset_id: 元データ集合ID,
//     dataset_revision: 上書きされない元revision,
//     sample_id: 元サンプルID,
//     position_sfen: 再評価対象の局面,
//     observed_move_usi: 元サンプルの着手ラベル,
//     observed_score_cp: 元サンプルの評価ラベル
//   ]
// }
struct RelabelSource {
    std::string dataset_id;
    std::string dataset_revision;
    std::string sample_id;
    std::string position_sfen;
    std::optional<std::string> observed_move_usi;
    std::optional<std::int32_t> observed_score_cp;
};

// {
//   責務: [RelabelEvaluationStatus: 個別評価の成功と失敗を識別する]
// }
enum class RelabelEvaluationStatus : std::uint8_t { completed, failed };

// {
//   責務: [RelabelEvaluator: 評価を依頼する個体と実行backend、再現情報を結び付ける],
//   フィールド: [
//     participant: evaluatorの安定個体ID,
//     model_version: 評価時のmodel version,
//     config_hash: 評価設定のhash,
//     backend: 評価要求を実行するruntime backend
//   ]
// }
struct RelabelEvaluator {
    best::ParticipantId participant;
    std::string model_version;
    std::string config_hash;
    runtime::AIBackend* backend = nullptr;
};

// {
//   責務: [RelabelEvaluation: 1 evaluatorの検索出力と実行時provenanceを追記する],
//   フィールド: [
//     participant: 評価個体ID,
//     model_version: 使用model version,
//     config_hash: 使用設定hash,
//     status: 評価の完了状態,
//     turn_status: Runtimeが返した合法性・終局状態,
//     search_result: 着手、評価値、depth、nodesなど,
//     decision_time: backendの意思決定時間,
//     error: backend失敗または違法手の説明
//   ]
// }
struct RelabelEvaluation {
    best::ParticipantId participant;
    std::string model_version;
    std::string config_hash;
    RelabelEvaluationStatus status = RelabelEvaluationStatus::failed;
    std::optional<runtime::TurnStatus> turn_status;
    std::optional<shogi::SearchResult> search_result;
    std::chrono::nanoseconds decision_time{0};
    std::optional<std::string> error;
};

// {
//   責務: [RelabelRecord: 元サンプルを保持したまま複数evaluatorの結果を追加する],
//   フィールド: [
//     source: 評価前の識別子・盤面・元ラベル・provenance,
//     evaluations: 個体ごとに追記する再評価結果
//   ]
// }
struct RelabelRecord {
    RelabelSource source;
    std::vector<RelabelEvaluation> evaluations;
};

// {
//   責務: [evaluate_relabel_sample: 同一局面を2個以上のAIへ評価依頼して追記可能なrecordを返す],
//   処理: [
//     1: 元局面と評価個体一覧を検証する,
//     2: 各AIの結果を元ラベルを変更せず追加する,
//     3: 1個体の失敗を記録して残りの評価を続行する
//   ],
//   引数: [source: 上書きしない元サンプル] [evaluators: 一意な2個以上の評価個体]
//         [limits: 全backendへ渡す探索制限],
//   戻り値: [record: 元サンプルと全個体の成功・失敗結果],
//   エラー: [入力の不足・重複・不正局面はstd::invalid_argument等で失敗]
//
// }
// 
[[nodiscard]] RelabelRecord evaluate_relabel_sample(
    const RelabelSource& source,
    std::span<const RelabelEvaluator> evaluators,
    const shogi::SearchLimits& limits = {}
);

} // namespace kadoka::relabel
