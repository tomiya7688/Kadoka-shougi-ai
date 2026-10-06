#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::training {

struct DatasetRecipeWeight {
    std::string dataset_id{};
    double weight{1.0};
};

struct TrainingSourceWeights {
    double external{0.0};
    double human{0.0};
    double league{0.0};
    double self{1.0};
};

struct OfficialTrainingRecipe {
    std::uint32_t schema_version{1};
    std::vector<DatasetRecipeWeight> datasets{};
    TrainingSourceWeights source_weights{};
    std::string architecture_id{};
    std::string architecture_version{};
    std::string config_hash{};
    std::uint64_t champion_generation{0};
    std::optional<std::string> autotune_config_id{};
    std::optional<std::string> evaluation_result_id{};
};

// Throws std::invalid_argument when a field, weight, or schema invariant fails.
void validate_training_recipe(const OfficialTrainingRecipe& recipe);

// Additionally checks each dataset reference against the supplied registry view.
void validate_training_recipe(
    const OfficialTrainingRecipe& recipe,
    std::span<const std::string> registered_dataset_ids
);

[[nodiscard]] std::string serialize_training_recipe(
    const OfficialTrainingRecipe& recipe
);

// Parsing rejects malformed JSON and unsupported schema versions.
[[nodiscard]] OfficialTrainingRecipe deserialize_training_recipe(
    std::string_view json
);

[[nodiscard]] OfficialTrainingRecipe deserialize_training_recipe(
    std::string_view json,
    std::span<const std::string> registered_dataset_ids
);

} // namespace kadoka::shogi::training
