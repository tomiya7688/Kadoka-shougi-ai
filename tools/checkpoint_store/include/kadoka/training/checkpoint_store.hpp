#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace kadoka::training {

// {
//   責務: [CheckpointState: 学習再開に必要な状態と再現用メタデータを保持する],
//   フィールド: [epoch: 学習epoch] [step: optimizer更新step] [weights: モデル重み] [optimizer_state: optimizer状態]
//              [scheduler_state: scheduler状態] [rng_state: 乱数生成器状態] [dataset_cursor: dataset読取位置]
//              [training_config_hash: 学習設定hash] [dataset_version: dataset識別子] [metrics: 保存時の指標]
// }
struct CheckpointState {
    std::uint64_t epoch = 0;
    std::uint64_t step = 0;
    std::vector<std::uint8_t> weights;
    std::vector<std::uint8_t> optimizer_state;
    std::vector<std::uint8_t> scheduler_state;
    std::vector<std::uint8_t> rng_state;
    std::vector<std::uint8_t> dataset_cursor;
    std::string training_config_hash;
    std::string dataset_version;
    std::map<std::string, double> metrics;
};

// {
//   責務: [CheckpointCompatibility: load時に要求する学習設定とdataset版を指定する],
//   フィールド: [training_config_hash: 一致が必要な場合の設定hash] [dataset_version: 一致が必要な場合のdataset版]
// }
struct CheckpointCompatibility {
    std::string training_config_hash;
    std::string dataset_version;
};

// {
//   責務: [save_checkpoint: 完成したcheckpointファイルを原子的に公開する],
//   処理: [1: 状態を検証してversion付き形式へ符号化する] [2: 同じdirectoryの一時領域へ全量を書き出す]
//         [3: 同じfilesystem上で完成ファイルを新規targetへhard linkで公開する],
//   引数: [path: 新規作成するcheckpointファイル] [state: 保存する学習状態],
//   戻り値: [],
//   エラー: [targetが既存、親directoryがない、hard link非対応の場合はfilesystem_error]
//           [状態が不正ならinvalid_argument]
// }
void save_checkpoint(const std::filesystem::path& path, const CheckpointState& state);

// {
//   責務: [load_checkpoint: checkpointを読み取り互換性と破損の有無を検証する],
//   処理: [1: file sizeと形式versionを検証する] [2: checksumと全fieldを復元する]
//         [3: 指定されたtraining configとdataset版を照合する],
//   引数: [path: 読み込むcheckpoint file] [expected: 任意の互換性条件],
//   戻り値: [CheckpointState: 復元した学習状態],
//   エラー: [破損・非対応version・互換性不一致はruntime_error] [file失敗はfilesystem_error]
// }
CheckpointState load_checkpoint(const std::filesystem::path& path, const CheckpointCompatibility& expected = {});

} // namespace kadoka::training
