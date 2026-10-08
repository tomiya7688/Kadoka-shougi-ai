#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::training {

// {
//   責務: [DatasetRecipeWeight: 学習recipeに使うdataset revisionと重みを表す]
//   フィールド: [dataset_id: dataset識別子 / dataset_revision: 固定されたrevision / weight: 学習重み]
// }
struct DatasetRecipeWeight {
    std::string dataset_id{};
    std::string dataset_revision{};
    double weight{1.0};
};

// {
//   責務: [DatasetRegistryReference: registry内の固定dataset revisionを識別する]
//   フィールド: [dataset_id: dataset識別子 / dataset_revision: 不変revision識別子]
// }
struct DatasetRegistryReference {
    std::string dataset_id{};
    std::string dataset_revision{};
};

// {
//   責務: [TrainingSourceWeights: source別の学習重みを保持する]
//   フィールド: [external/human/league/self: 各sourceの比率]
// }
struct TrainingSourceWeights {
    double external{0.0};
    double human{0.0};
    double league{0.0};
    double self{1.0};
};

// {
//   責務: [OfficialTrainingRecipe: 再現可能な学習recipeのversion付き項目を保持する]
//   フィールド: [schema_version / datasets / source_weights / architecture_idとversion / effective_config_json: 再現可能な有効設定JSON / config_hash / generation / 任意の調整・評価参照]
// }
struct OfficialTrainingRecipe {
    std::uint32_t schema_version{1};
    std::vector<DatasetRecipeWeight> datasets{};
    TrainingSourceWeights source_weights{};
    std::string architecture_id{};
    std::string architecture_version{};
    std::string effective_config_json{};
    std::string config_hash{};
    std::uint64_t champion_generation{0};
    std::optional<std::string> autotune_config_id{};
    std::optional<std::string> evaluation_result_id{};
};

// {
//   責務: [validate_training_recipe: recipeのschemaと値を検証する]
//   処理: [必須値、重み、重複dataset、schema版を確認する]
//   引数: [recipe: 検証対象]
//   戻り値: [void。不正ならinvalid_argumentを送出する]
// }
void validate_training_recipe(const OfficialTrainingRecipe& recipe);

// {
//   責務: [validate_training_recipe: recipeを検証しIDとrevisionを照合する]
//   処理: [recipe検証後、各IDをregistry viewのIDとrevisionに照合する]
//   引数: [recipe: 検証対象 / registered_datasets: 利用可能なIDとrevision一覧]
//   戻り値: [void。不正または未登録ならinvalid_argumentを送出する]
// }
void validate_training_recipe(
    const OfficialTrainingRecipe& recipe,
    std::span<const DatasetRegistryReference> registered_datasets
);

// {
//   責務: [serialize_training_recipe: recipeを決定的なJSONに変換する]
//   処理: [recipe検証、dataset順序の正規化、JSON生成]
//   引数: [recipe: serialize対象]
//   戻り値: [JSON文字列]
// }
[[nodiscard]] std::string serialize_training_recipe(
    const OfficialTrainingRecipe& recipe
);

// Parsing rejects malformed JSON and unsupported schema versions.
// {
//   責務: [deserialize_training_recipe: JSONからrecipeを復元して検証する]
//   処理: [JSON解析、項目復元、schema・値検証]
//   引数: [json: 読み込むJSON]
//   戻り値: [復元したrecipe。不正ならinvalid_argumentを送出する]
// }
[[nodiscard]] OfficialTrainingRecipe deserialize_training_recipe(
    std::string_view json
);

// {
//   責務: [deserialize_training_recipe: JSONからrecipeを復元しdataset参照を照合する]
//   処理: [JSON解析、recipe検証、registry照合]
//   引数: [json: 読み込むJSON / registered_datasets: 利用可能IDとrevision一覧]
//   戻り値: [復元したrecipe。不正または未登録ならinvalid_argumentを送出する]
// }
[[nodiscard]] OfficialTrainingRecipe deserialize_training_recipe(
    std::string_view json,
    std::span<const DatasetRegistryReference> registered_datasets
);

} // namespace kadoka::shogi::training
