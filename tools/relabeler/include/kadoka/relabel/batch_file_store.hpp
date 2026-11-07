#pragma once

#include "kadoka/relabel/batch.hpp"

#include <filesystem>

namespace kadoka::relabel {

// {
//   責務: [RelabelBatchFileStore: 1つのqueueのrecordと再開cursorを同じファイルで確定する],
//   フィールド: [directory_: batch専用保存先] [revision_: queue revision]
//     [next_index_: 連続して確定したsample数]
// }
class RelabelBatchFileStore {
  public:
    // {
    //   責務: [RelabelBatchFileStore: 保存済みrecordを検証して再開位置を復元する],
    //   処理: [1: 保存先を作成する] [2: 連続したrecordを検証する],
    //   引数: [directory: queue専用ディレクトリ] [revision: 不変queue revision],
    //   戻り値: [store: 保存済みcursorを持つ保存先]
    // }
    RelabelBatchFileStore(std::filesystem::path directory, std::string revision);

    // {
    //   責務: [cursor: 保存済みsampleの直後を指す],
    //   処理: [1: revisionと確定済み件数からcursorを作る],
    //   引数: [],
    //   戻り値: [cursor: 再開位置]
    // }
    [[nodiscard]] RelabelBatchCursor cursor() const;

    // {
    //   責務: [save: recordと次cursorを1つの確定ファイルへ保存する],
    //   処理: [1: 次indexとrevisionを検証する] [2: 一時ファイルに完全に書く]
    //     [3: renameで確定する],
    //   引数: [record: 評価結果] [next: 保存成功後のcursor],
    //   戻り値: [saved: 保存確定に成功したらtrue、I/O失敗ならfalse]
    // }
    [[nodiscard]] bool save(const RelabelRecord& record, const RelabelBatchCursor& next);

    // {
    //   責務: [load: 保存済みrecordを欠落なく復元する],
    //   処理: [1: indexを検証する] [2: recordファイルを読んで形式とrevisionを検証する],
    //   引数: [index: 0起点の保存済みsample位置],
    //   戻り値: [record: 元sourceと全評価結果]
    // }
    [[nodiscard]] RelabelRecord load(std::size_t index) const;

  private:
    std::filesystem::path directory_;
    std::string revision_;
    std::size_t next_index_ = 0;
};

} // namespace kadoka::relabel
