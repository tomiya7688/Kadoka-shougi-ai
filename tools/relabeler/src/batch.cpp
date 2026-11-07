#include "kadoka/relabel/batch.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace kadoka::relabel {
namespace {

// {
//   責務: [validate_batch: backend実行前にqueue全体と予算の契約を検証する],
//   処理: [1: cursorと時間の範囲を検証する] [2: 個体の必須情報を検証する]
//     [3: 元sampleの一意性と局面を検証する],
//   引数: [queue: 固定queue] [evaluators: 評価個体] [request: 実行要求] [sink: 保存先],
//   戻り値: []
// }
void validate_batch(const RelabelBatchQueue& queue, std::span<const RelabelEvaluator> evaluators,
                    const RelabelBatchRequest& request, const RelabelBatchSink& sink) {
    if (queue.revision.empty() || request.cursor.queue_revision != queue.revision ||
        request.cursor.next_index > queue.sources.size() || !sink)
        throw std::invalid_argument("batch requires a revision, valid cursor, and persistence sink");
    if ((request.max_time && request.max_time->count() < 0) ||
        (request.search_limits.time_limit && request.search_limits.time_limit->count() <= 0) ||
        (request.search_limits.node_limit && *request.search_limits.node_limit == 0))
        throw std::invalid_argument("invalid batch search budget");
    if (evaluators.size() < 2)
        throw std::invalid_argument("batch requires at least two evaluators");
    std::set<std::string> participants;
    for (const auto& evaluator : evaluators) {
        if (evaluator.participant.value.empty() || evaluator.model_version.empty() || evaluator.config_hash.empty() ||
            evaluator.backend == nullptr || !participants.insert(evaluator.participant.value).second)
            throw std::invalid_argument("batch evaluator metadata must be complete and unique");
    }
    std::set<std::tuple<std::string, std::string, std::string>> samples;
    for (const auto& source : queue.sources) {
        if (source.dataset_id.empty() || source.dataset_revision.empty() || source.sample_id.empty() ||
            !samples.emplace(source.dataset_id, source.dataset_revision, source.sample_id).second)
            throw std::invalid_argument("batch source identifiers must be complete and unique");
        (void)shogi::Position::from_sfen(source.position_sfen);
    }
}

} // namespace

// {
//   責務: [evaluate_relabel_batch: 保存を進捗の確定点として予算付き再評価を実行する],
//   処理: [1: 入力検証] [2: 残予算を分配] [3: 評価結果を保存] [4: 確定cursorを更新],
//   引数: [queue: 固定queue] [evaluators: 評価個体] [request: 今回の予算] [sink: 保存先],
//   戻り値: [result: 保存済み進捗と停止理由]
// }
RelabelBatchResult evaluate_relabel_batch(const RelabelBatchQueue& queue, std::span<const RelabelEvaluator> evaluators,
                                          const RelabelBatchRequest& request, const RelabelBatchSink& sink) {
    const auto started = std::chrono::steady_clock::now();
    validate_batch(queue, evaluators, request, sink);
    RelabelBatchResult result;
    result.cursor = request.cursor;
    while (result.cursor.next_index < queue.sources.size()) {
        if (result.saved_samples >= request.max_samples) {
            result.stop = RelabelBatchStop::sample_budget;
            break;
        }
        auto limits = request.search_limits;
        if (request.max_time) {
            const auto elapsed =
                std::chrono::ceil<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
            // msへ切り上げず、残時間を保守的に各AIへ割り当てる。
            const auto remaining = *request.max_time - elapsed;
            const auto share = remaining.count() / static_cast<std::int64_t>(evaluators.size());
            if (share <= 0) {
                result.stop = RelabelBatchStop::time_budget;
                break;
            }
            const auto allocation = std::chrono::milliseconds{share};
            limits.time_limit = limits.time_limit ? std::min(*limits.time_limit, allocation) : allocation;
        }
        if (request.max_nodes) {
            const auto remaining = *request.max_nodes - std::min(*request.max_nodes, result.reported_nodes);
            const auto allocation = remaining / evaluators.size();
            if (allocation == 0) {
                result.stop = RelabelBatchStop::node_budget;
                break;
            }
            limits.node_limit = limits.node_limit ? std::min(*limits.node_limit, allocation) : allocation;
        }
        auto record = evaluate_relabel_sample(queue.sources[result.cursor.next_index], evaluators, limits);
        bool unaccounted_nodes = false;
        for (const auto& evaluation : record.evaluations) {
            if (!evaluation.search_result) {
                unaccounted_nodes = true;
                continue;
            }
            const auto nodes = evaluation.search_result->nodes;
            const auto capacity = std::numeric_limits<std::uint64_t>::max() - result.reported_nodes;
            result.reported_nodes += std::min(nodes, capacity);
        }
        auto next = result.cursor;
        ++next.next_index;
        if (!sink(record, next)) {
            result.pending_record = std::move(record);
            result.stop = RelabelBatchStop::persistence_failed;
            break;
        }
        result.cursor = std::move(next);
        ++result.saved_samples;
        // 例外終了したAIの消費nodeは不明。上限指定中は推測で続行しない。
        if (request.max_nodes && unaccounted_nodes) {
            result.stop = RelabelBatchStop::unaccounted_nodes;
            break;
        }
    }
    return result;
}

} // namespace kadoka::relabel
