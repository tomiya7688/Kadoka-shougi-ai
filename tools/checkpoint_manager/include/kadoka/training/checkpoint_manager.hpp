#pragma once

#include "kadoka/training/checkpoint_store.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kadoka::training {

// {
//   責務: [CheckpointManager: latest・periodic・best checkpointとretentionを管理する],
//   フィールド: [directory: checkpointの保存先] [periodic_keep: 保持するperiodic checkpoint数]
// }
class CheckpointManager {
  public:
    // {
    //   責務: [CheckpointManager: 保存directoryを準備しretention設定を保持する],
    //   引数: [directory: checkpointを格納するdirectory] [periodic_keep: 保持するperiodic数]
    //   戻り値: [],
    //   エラー: [directoryを作成できない場合はfilesystem_error]
    // }
    explicit CheckpointManager(std::filesystem::path directory, std::size_t periodic_keep);

    // {
    //   責務: [save_latest: 新checkpointを保存しlatest pointerを更新する],
    //   引数: [state: 保存する学習状態],
    //   戻り値: [path: 作成したimmutable checkpoint]
    // }
    std::filesystem::path save_latest(const CheckpointState& state);

    // {
    //   責務: [save_periodic: periodic checkpointを保存しlatest更新後にretentionを適用する],
    //   引数: [state: 保存する学習状態],
    //   戻り値: [path: 作成したperiodic checkpoint]
    // }
    std::filesystem::path save_periodic(const CheckpointState& state);

    // {
    //   責務: [consider_best: 指定metricが改善した場合にbest checkpointとmetadataを更新する],
    //   引数: [state: 保存する学習状態] [metric_name: 比較するmetric] [metric_value: finiteなmetric値]
    //         [lower_is_better: trueなら小さい値を改善とする]
    //   戻り値: [bool: 改善して保存した場合true]
    // }
    bool consider_best(const CheckpointState& state, const std::string& metric_name, double metric_value,
                       bool lower_is_better);

    // {
    //   責務: [latest_path: 有効なlatest pointerがあればcheckpoint pathを返す],
    //   引数: [],
    //   戻り値: [optional<path>: latest checkpoint、未保存なら空]
    // }
    std::optional<std::filesystem::path> latest_path() const;

    // {
    //   責務: [best_path: 有効なbest pointerがあればcheckpoint pathを返す],
    //   引数: [],
    //   戻り値: [optional<path>: best checkpoint、未保存なら空]
    // }
    std::optional<std::filesystem::path> best_path() const;

    // {
    //   責務: [periodic_paths: 保存済みperiodic checkpointを新しい順に返す],
    //   引数: [],
    //   戻り値: [vector<path>: periodic checkpoint paths]
    // }
    std::vector<std::filesystem::path> periodic_paths() const;

  private:
    // {
    //   責務: [save_snapshot: 一意な名前でcheckpointを保存する],
    //   引数: [prefix: filename prefix] [state: 保存する学習状態],
    //   戻り値: [path: 作成したcheckpoint]
    // }
    std::filesystem::path save_snapshot(const std::string& prefix, const CheckpointState& state);

    // {
    //   責務: [publish_pointer: pointer recordを一時fileから世代付きfileへ公開する],
    //   引数: [kind: latestまたはbest] [filename: checkpoint filename] [metadata: 追加metadata]
    //   戻り値: [],
    //   エラー: [書込みまたはrename失敗時はfilesystem_error]
    // }
    void publish_pointer(const std::string& kind, const std::string& filename, const std::string& metadata);

    std::filesystem::path directory_;
    std::size_t periodic_keep_;
    std::uint64_t generation_;
};

} // namespace kadoka::training
