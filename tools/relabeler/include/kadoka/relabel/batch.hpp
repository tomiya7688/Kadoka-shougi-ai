#pragma once

#include "kadoka/relabel/relabeler.hpp"

#include <functional>

namespace kadoka::relabel {

// {
//   責務: [RelabelBatchQueue: 再評価する順序付きsample群をrevisionに結び付ける],
//   フィールド: [revision: sample内容・順序・評価設定が変わるたびに更新する識別子]
//     [sources: 重複のない元sample一覧]
// }
struct RelabelBatchQueue {
    std::string revision;
    std::vector<RelabelSource> sources;
};

// {
//   責務: [RelabelBatchCursor: 保存済みqueueの再開位置を識別する],
//   フィールド: [queue_revision: queueと評価設定全体の不変revision] [next_index: 次のsample位置]
// }
struct RelabelBatchCursor {
    std::string queue_revision;
    std::size_t next_index = 0;
};

// {
//   責務: [RelabelBatchRequest: 1回の実行予算と再開位置を指定する],
//   フィールド: [cursor: 再開位置] [max_samples: 今回の保存件数上限]
//     [max_time: 今回の経過時間上限、未指定は無制限]
//     [max_nodes: 今回の総探索node上限、未指定は無制限]
//     [search_limits: 各AIに適用する追加制限]
// }
struct RelabelBatchRequest {
    RelabelBatchCursor cursor;
    std::size_t max_samples = 0;
    std::optional<std::chrono::milliseconds> max_time;
    std::optional<std::uint64_t> max_nodes;
    shogi::SearchLimits search_limits;
};

// {
//   責務: [RelabelBatchStop: batchの停止理由を区別する]
// }
enum class RelabelBatchStop : std::uint8_t {
    completed,
    sample_budget,
    time_budget,
    node_budget,
    unaccounted_nodes,
    persistence_failed
};

// {
//   責務: [RelabelBatchResult: 保存が確定した進捗と未保存recordを返す],
//   フィールド: [cursor: 保存済みの再開位置] [saved_samples: 今回保存した件数]
//     [reported_nodes: 今回AIが報告したnode合計、上限超過時は飽和]
//     [stop: 停止理由] [pending_record: 保存拒否された評価結果]
// }
struct RelabelBatchResult {
    RelabelBatchCursor cursor;
    std::size_t saved_samples = 0;
    std::uint64_t reported_nodes = 0;
    RelabelBatchStop stop = RelabelBatchStop::completed;
    std::optional<RelabelRecord> pending_record;
};

// recordと次cursorを一体として永続化してからtrueを返す。falseなら保存は未確定。
using RelabelBatchSink = std::function<bool(const RelabelRecord&, const RelabelBatchCursor&)>;

// {
//   責務: [evaluate_relabel_batch: queueを予算内で順次評価して保存確認後に進捗を進める],
//   処理: [1: queueと再開位置を検証する] [2: 残予算をAI数で分配する]
//     [3: 単一sample APIで評価する] [4: recordと次cursorの保存確認後に継続する],
//   引数: [queue: 順序を固定した元sample一覧] [evaluators: 評価個体一覧]
//     [request: queue revision、再開位置、今回の予算] [sink: 原子的な保存処理],
//   戻り値: [result: 保存済み進捗、停止理由、保存失敗時の未保存record],
//   エラー: [不正入力とsink例外は伝播する。既に保存済みのcursorから再開できる]
// }
[[nodiscard]] RelabelBatchResult evaluate_relabel_batch(const RelabelBatchQueue& queue,
                                                        std::span<const RelabelEvaluator> evaluators,
                                                        const RelabelBatchRequest& request,
                                                        const RelabelBatchSink& sink);

} // namespace kadoka::relabel
