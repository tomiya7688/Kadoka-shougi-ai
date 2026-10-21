#include "kadoka/relabel/batch_file_store.hpp"
#include "kadoka/runtime/ai_backend.hpp"
#include "kadoka/runtime/player_api.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// {
//   責務: [check: 条件違反を理由付きテスト失敗にする],
//   処理: [1: falseなら例外を投げる],
//   引数: [condition: 検査結果] [message: 失敗理由],
//   戻り値: []
// }
void check(const bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [TestDirectory: テスト専用一時ディレクトリを片付ける],
//   フィールド: [path: このテストが作成した専用ディレクトリ]
// }
struct TestDirectory {
    std::filesystem::path path;

    // {
    //   責務: [TestDirectory: 一意な一時保存先を作る],
    //   処理: [1: 時刻から専用名を組み立てて作成する],
    //   引数: [],
    //   戻り値: [directory: 専用保存先]
    // }
    TestDirectory() {
        path = std::filesystem::temp_directory_path() /
               ("kadoka-relabel-batch-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(path))
            throw std::runtime_error("temporary batch directory already exists");
    }

    // {
    //   責務: [~TestDirectory: テストが作ったディレクトリを片付ける],
    //   処理: [1: 一時保存先を削除する],
    //   引数: [],
    //   戻り値: []
    // }
    ~TestDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

// {
//   責務: [make_queue: 再評価用の一意な3 sampleを作る],
//   処理: [1: 同じ合法局面に異なるIDと元ラベルを付ける],
//   引数: [],
//   戻り値: [queue: 3件の固定queue]
// }
kadoka::relabel::RelabelBatchQueue make_queue() {
    kadoka::relabel::RelabelBatchQueue queue{"batch-revision-1", {}};
    const auto sfen = kadoka::shogi::Position::startpos().to_sfen();
    for (int index = 0; index < 3; ++index) {
        queue.sources.push_back({.dataset_id = "dataset",
                                 .dataset_revision = "source-revision",
                                 .sample_id = "sample-" + std::to_string(index),
                                 .position_sfen = sfen,
                                 .observed_move_usi = "7g7f\nquote\"",
                                 .observed_score_cp = 42});
    }
    return queue;
}

// {
//   責務: [test_resume_and_budgets: 保存後の再開とnode・時間予算の停止を確認する],
//   処理: [1: 2件保存して再起動する] [2: 残件を復元cursorから処理する]
//     [3: nodeと時間が不足した場合の停止を確認する],
//   引数: [],
//   戻り値: []
// }
void test_resume_and_budgets() {
    using namespace kadoka;
    TestDirectory directory;
    auto queue = make_queue();
    std::vector<std::uint64_t> observed_limits;
    shogi::runtime::FunctionAIBackend first(
        shogi::runtime::AIBackendKind::Native, "first",
        [&observed_limits](const shogi::Position&, const shogi::SearchLimits& limits) {
            observed_limits.push_back(limits.node_limit.value_or(0));
            shogi::SearchResult result;
            result.best_move = shogi::runtime::move_from_usi("7g7f");
            result.nodes = 10;
            result.score_cp = 27;
            result.depth = 3;
            result.info = "line\nquote\"\\";
            return result;
        });
    shogi::runtime::FunctionAIBackend second(
        shogi::runtime::AIBackendKind::Native, "second",
        [&observed_limits](const shogi::Position&, const shogi::SearchLimits& limits) {
            observed_limits.push_back(limits.node_limit.value_or(0));
            shogi::SearchResult result;
            result.best_move = shogi::runtime::move_from_usi("2g2f");
            result.nodes = 10;
            result.score_cp = -14;
            return result;
        });
    const std::vector<relabel::RelabelEvaluator> evaluators{
        {{"sha256:" + std::string(64, '1')}, "v1", "cfg1", &first},
        {{"sha256:" + std::string(64, '2')}, "v2", "cfg2", &second},
    };
    relabel::RelabelBatchFileStore store(directory.path / "main", queue.revision);
    relabel::RelabelBatchRequest request;
    request.cursor = store.cursor();
    request.max_samples = 2;
    const auto save = [&store](const relabel::RelabelRecord& record, const relabel::RelabelBatchCursor& next) {
        return store.save(record, next);
    };
    const auto first_run = relabel::evaluate_relabel_batch(queue, evaluators, request, save);
    check(first_run.stop == relabel::RelabelBatchStop::sample_budget && first_run.saved_samples == 2 &&
              first_run.cursor.next_index == 2,
          "sample budget must stop after two saved records");
    relabel::RelabelBatchFileStore reopened(directory.path / "main", queue.revision);
    check(reopened.cursor().next_index == 2, "restart must recover committed cursor");
    const auto saved = reopened.load(0);
    check(saved.evaluations.size() == 2, "both evaluator records must persist");
    const auto& first_search = saved.evaluations[0].search_result;
    const auto& second_search = saved.evaluations[1].search_result;
    if (!first_search || !second_search)
        throw std::runtime_error("both search results must persist");
    check(saved.source.observed_move_usi == queue.sources[0].observed_move_usi &&
              saved.source.observed_score_cp == 42 && first_search->info == "line\nquote\"\\" &&
              first_search->score_cp == 27 && second_search->score_cp == -14,
          "file store must preserve source labels and both search results");
    request.cursor = reopened.cursor();
    const auto remaining = relabel::evaluate_relabel_batch(
        queue, evaluators, request,
        [&reopened](const relabel::RelabelRecord& record, const relabel::RelabelBatchCursor& next) {
            return reopened.save(record, next);
        });
    check(remaining.stop == relabel::RelabelBatchStop::completed && reopened.cursor().next_index == 3,
          "resume must process only the remaining sample");

    relabel::RelabelBatchFileStore node_store(directory.path / "nodes", queue.revision);
    request.cursor = node_store.cursor();
    request.max_samples = 3;
    request.max_nodes = 20;
    observed_limits.clear();
    const auto nodes = relabel::evaluate_relabel_batch(
        queue, evaluators, request,
        [&node_store](const relabel::RelabelRecord& record, const relabel::RelabelBatchCursor& next) {
            return node_store.save(record, next);
        });
    check(nodes.stop == relabel::RelabelBatchStop::node_budget && nodes.saved_samples == 1 &&
              nodes.reported_nodes == 20 && observed_limits == std::vector<std::uint64_t>{10, 10},
          "node budget must be shared and stop before another sample");
    request.cursor = {queue.revision, 0};
    request.max_time = std::chrono::milliseconds{0};
    const auto time = relabel::evaluate_relabel_batch(
        queue, evaluators, request,
        [](const relabel::RelabelRecord&, const relabel::RelabelBatchCursor&) { return true; });
    check(time.stop == relabel::RelabelBatchStop::time_budget && time.saved_samples == 0,
          "zero time budget must avoid backend calls");
}

// {
//   責務: [test_failure_and_corruption: 未保存resultと破損ファイルの拒否を確認する],
//   処理: [1: sink失敗時のcursorを確認する] [2: 不正revision・欠番・破損を拒否する],
//   引数: [],
//   戻り値: []
// }
void test_failure_and_corruption() {
    using namespace kadoka;
    TestDirectory directory;
    auto queue = make_queue();
    shogi::runtime::FunctionAIBackend good(shogi::runtime::AIBackendKind::Native, "good",
                                           [](const shogi::Position&, const shogi::SearchLimits&) {
                                               shogi::SearchResult result;
                                               result.best_move = shogi::runtime::move_from_usi("7g7f");
                                               result.nodes = 5;
                                               return result;
                                           });
    shogi::runtime::FunctionAIBackend failing(
        shogi::runtime::AIBackendKind::Native, "failing",
        [](const shogi::Position&, const shogi::SearchLimits&) -> shogi::SearchResult {
            throw std::runtime_error("backend unavailable");
        });
    const std::vector<relabel::RelabelEvaluator> evaluators{
        {{"sha256:" + std::string(64, '3')}, "v1", "cfg1", &good},
        {{"sha256:" + std::string(64, '4')}, "v2", "cfg2", &failing},
    };
    relabel::RelabelBatchFileStore store(directory.path / "failure", queue.revision);
    relabel::RelabelBatchRequest request;
    request.cursor = store.cursor();
    request.max_samples = 3;
    const auto rejected = relabel::evaluate_relabel_batch(
        queue, evaluators, request,
        [&store](const relabel::RelabelRecord& record, const relabel::RelabelBatchCursor& next) {
            return next.next_index == 1 ? store.save(record, next) : false;
        });
    check(rejected.stop == relabel::RelabelBatchStop::persistence_failed && rejected.cursor.next_index == 1 &&
              rejected.pending_record && rejected.pending_record->source.sample_id == "sample-1" &&
              store.cursor().next_index == 1,
          "failed sink must keep cursor at the last saved record");
    const auto persisted = store.load(0);
    check(persisted.evaluations[0].status == relabel::RelabelEvaluationStatus::completed &&
              persisted.evaluations[1].status == relabel::RelabelEvaluationStatus::failed &&
              persisted.evaluations[1].error == "backend unavailable",
          "partial engine success and error must persist together");
    relabel::RelabelBatchFileStore reopened(directory.path / "failure", queue.revision);
    check(reopened.cursor().next_index == 1, "sink rejection must not persist the pending cursor");
    request.cursor = reopened.cursor();
    const auto resumed = relabel::evaluate_relabel_batch(
        queue, evaluators, request,
        [&reopened](const relabel::RelabelRecord& record, const relabel::RelabelBatchCursor& next) {
            return reopened.save(record, next);
        });
    check(resumed.stop == relabel::RelabelBatchStop::completed && resumed.saved_samples == 2 &&
              reopened.cursor().next_index == 3,
          "a rejected sample must be evaluated again from the persisted cursor");
    relabel::RelabelBatchFileStore unknown_store(directory.path / "unknown", queue.revision);
    request.cursor = unknown_store.cursor();
    request.max_nodes = 20;
    const auto unknown = relabel::evaluate_relabel_batch(
        queue, evaluators, request,
        [&unknown_store](const relabel::RelabelRecord& record, const relabel::RelabelBatchCursor& next) {
            return unknown_store.save(record, next);
        });
    check(unknown.stop == relabel::RelabelBatchStop::unaccounted_nodes && unknown.saved_samples == 1 &&
              unknown_store.cursor().next_index == 1,
          "unknown backend node use must stop after persisting its partial result");
    relabel::RelabelBatchFileStore io_store(directory.path / "io", queue.revision);
    const auto blocked_temp = directory.path / "io" / "0000000000000000.batch.tmp";
    std::filesystem::create_directory(blocked_temp);
    check(!io_store.save(persisted, {queue.revision, 1}) && io_store.cursor().next_index == 0,
          "file write failure must not advance the cursor");
    std::filesystem::remove(blocked_temp);
    check(io_store.save(persisted, {queue.revision, 1}) && io_store.cursor().next_index == 1,
          "retry after file write failure must succeed");
    bool wrong_revision_rejected = false;
    try {
        relabel::RelabelBatchFileStore wrong(directory.path / "failure", "changed-revision");
    } catch (const std::runtime_error&) {
        wrong_revision_rejected = true;
    }
    check(wrong_revision_rejected, "revision mismatch must fail recovery");
    const auto gap = directory.path / "gap";
    std::filesystem::create_directory(gap);
    std::filesystem::copy_file(directory.path / "failure" / "0000000000000000.batch", gap / "0000000000000002.batch");
    bool gap_rejected = false;
    try {
        relabel::RelabelBatchFileStore broken(gap, queue.revision);
    } catch (const std::runtime_error&) {
        gap_rejected = true;
    }
    check(gap_rejected, "missing prior record must fail recovery");
    {
        std::ofstream damage(directory.path / "failure" / "0000000000000000.batch", std::ios::binary | std::ios::trunc);
        damage << "KRB1 corrupt";
    }
    bool corruption_rejected = false;
    try {
        relabel::RelabelBatchFileStore broken(directory.path / "failure", queue.revision);
    } catch (const std::runtime_error&) {
        corruption_rejected = true;
    }
    check(corruption_rejected, "malformed record must fail recovery");
}

} // namespace

// {
//   責務: [main: バッチ予算・再開・部分保存の公開契約を検査する],
//   処理: [1: 正常な保存と再開を確認する] [2: 予算と保存障害を確認する],
//   引数: [],
//   戻り値: [0: 成功] [1: 検証失敗]
// }
int main() {
    try {
        test_resume_and_budgets();
        test_failure_and_corruption();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
