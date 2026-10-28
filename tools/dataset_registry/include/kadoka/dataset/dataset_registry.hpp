#ifndef KADOKA_DATASET_DATASET_REGISTRY_HPP
#define KADOKA_DATASET_DATASET_REGISTRY_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kadoka::dataset {

// {
//   責務: [DatasetRole: 学習・検証・評価の用途を分離する],
//   値: [training: 学習] [validation: 検証] [benchmark: 固定評価]
//     [league_eval: リーグ評価]
// }
enum class DatasetRole : std::uint8_t { training, validation, benchmark, league_eval };

// {
//   責務: [DatasetMetadata: Datasetの識別と出自を保持する],
//   フィールド: [dataset_id: 安定ID] [source_type: 生成元の種類]
//     [generator_model_id/generator_model_version: 生成モデル]
//     [opponent_model_id/opponent_model_version: 対戦相手]
//     [parent_dataset_id: 派生元] [generation: 派生世代]
//     [search_config_hash/training_config_hash: 設定SHA-256]
//     [created_at: UTC作成日時] [license: 利用条件] [role: 用途]
// }
struct DatasetMetadata {
    std::string dataset_id;
    std::string source_type;
    std::string generator_model_id;
    std::string generator_model_version;
    std::string opponent_model_id;
    std::string opponent_model_version;
    std::string parent_dataset_id;
    std::uint32_t generation = 0;
    std::string search_config_hash;
    std::string training_config_hash;
    std::string created_at;
    std::string license;
    DatasetRole role = DatasetRole::training;
};

// {
//   責務: [validate_metadata: 識別子・出自・用途と値の形式を検証する],
//   引数: [metadata: 検証対象],
//   戻り値: [],
//   エラー: [違反時にstd::invalid_argument]
// }
void validate_metadata(const DatasetMetadata& metadata);

// {
//   責務: [DatasetRegistry: Datasetの出自をローカルに永続化する],
//   フィールド: [directory_: registry専用保存先]
// }
class DatasetRegistry {
  public:
    // {
    //   責務: [DatasetRegistry: registry専用ディレクトリを準備する],
    //   引数: [directory: 保存先],
    //   戻り値: [registry: 使用可能なregistry]
    // }
    explicit DatasetRegistry(std::filesystem::path directory);

    // {
    //   責務: [add: 重複を拒否し親を確認して登録する],
    //   引数: [metadata: 登録するDataset],
    //   戻り値: [added: 新規登録した場合true、同じIDがあればfalse]
    // }
    [[nodiscard]] bool add(const DatasetMetadata& metadata);

    // {
    //   責務: [remove: 子DatasetがないIDだけ削除する],
    //   引数: [dataset_id: 削除するID],
    //   戻り値: [removed: 存在して削除した場合true]
    // }
    [[nodiscard]] bool remove(const std::string& dataset_id);

    // {
    //   責務: [get: 保存済みDatasetのID・祖先参照・世代を照合する],
    //   引数: [dataset_id: 検索するID],
    //   戻り値: [metadata: 存在する場合の内容]
    // }
    [[nodiscard]] std::optional<DatasetMetadata> get(const std::string& dataset_id) const;

    // {
    //   責務: [list: ID順に全Datasetを列挙する],
    //   戻り値: [datasets: 保存済みDataset]
    // }
    [[nodiscard]] std::vector<DatasetMetadata> list() const;

    // {
    //   責務: [list_by_role: 指定用途だけをID順に列挙する],
    //   引数: [role: 抽出する用途],
    //   戻り値: [datasets: 指定用途のDataset]
    // }
    [[nodiscard]] std::vector<DatasetMetadata> list_by_role(DatasetRole role) const;

  private:
    std::filesystem::path directory_;
};

} // namespace kadoka::dataset

#endif // KADOKA_DATASET_DATASET_REGISTRY_HPP
