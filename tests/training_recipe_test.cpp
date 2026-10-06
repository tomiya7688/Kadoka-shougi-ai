#include "kadoka/training/training_recipe.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi::training;

namespace {

// {
//   責務: [assert_invalid: 不正入力を渡した処理がinvalid_argumentを送出することを検証する]
//   処理: [処理を実行し、例外を捕捉してassertする]
//   引数: [function: 実行する処理]
//   戻り値: [なし]
// }
template <class Function>
void assert_invalid(Function&& function) {
    bool failed = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);
}

// {
//   責務: [sample_recipe: 検証に使う有効なrecipeを作る]
//   処理: [代表的なdataset、source重み、model参照を設定する]
//   引数: [なし]
//   戻り値: [OfficialTrainingRecipe]
// }
OfficialTrainingRecipe sample_recipe() {
    OfficialTrainingRecipe recipe;
    recipe.datasets = {
        {"dataset-b", "revision-b", 0.25},
        {"dataset-a", "revision-a", 1.5}
    };
    recipe.source_weights = {0.1, 0.0, 0.2, 0.7};
    recipe.architecture_id = "policy-value-v1";
    recipe.architecture_version = "1";
    recipe.effective_config_json = "{}";
    recipe.config_hash = "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";
    recipe.champion_generation = 3;
    recipe.autotune_config_id = "autotune-7";
    recipe.evaluation_result_id = "evaluation-9";
    return recipe;
}

} // namespace

// {
//   責務: [main: training recipe契約の代表ケースと異常系を検証する]
//   処理: [往復変換、正規化、任意値、重複、不正重み、registry参照、JSON構文とversionを確認する]
//   引数: [なし]
//   戻り値: [int: 成功時0]
// }
int main() {
    const std::array<DatasetRegistryReference, 2> registered{{
        {"dataset-a", "revision-a"},
        {"dataset-b", "revision-b"}
    }};
    // Round-trip a valid recipe and check the canonical JSON representation.
    {
        const auto recipe = sample_recipe();
        validate_training_recipe(recipe, registered);
        const std::string serialized = serialize_training_recipe(recipe);
        const auto decoded = deserialize_training_recipe(serialized, registered);
        assert(serialize_training_recipe(decoded) == serialized);
        assert(decoded.datasets.front().dataset_id == "dataset-a");
        assert(decoded.architecture_id == recipe.architecture_id);
        assert(decoded.champion_generation == recipe.champion_generation);
        assert(decoded.autotune_config_id == recipe.autotune_config_id);
    }

    // Dataset ordering in the input must not affect serialized bytes.
    {
        auto first = sample_recipe();
        auto second = first;
        std::reverse(second.datasets.begin(), second.datasets.end());
        assert(serialize_training_recipe(first) == serialize_training_recipe(second));
    }

    // Optional references round-trip as JSON null.
    {
        auto recipe = sample_recipe();
        recipe.autotune_config_id.reset();
        recipe.evaluation_result_id.reset();
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        assert(!decoded.autotune_config_id.has_value());
        assert(!decoded.evaluation_result_id.has_value());
    }

    // Duplicate dataset IDs are invalid even when each weight is valid.
    {
        auto recipe = sample_recipe();
        recipe.datasets.push_back({"dataset-a", "revision-a", 1.0});
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    // Registry-aware validation rejects absent dataset references.
    {
        auto recipe = sample_recipe();
        assert_invalid([&] { validate_training_recipe(recipe, std::span<const DatasetRegistryReference>{}); });
        assert_invalid([&] {
            static_cast<void>(deserialize_training_recipe(
                serialize_training_recipe(recipe), std::span<const std::string>{}
            ));
        });
    }

    // Dataset weights must be finite and strictly positive.
    {
        auto recipe = sample_recipe();
        recipe.datasets.front().weight = 0.0;
        assert_invalid([&] { validate_training_recipe(recipe); });
        recipe.datasets.front().weight = std::numeric_limits<double>::infinity();
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    // Source weights must stay in range and sum to one.
    {
        auto recipe = sample_recipe();
        recipe.effective_config_json = "{ \"b\": 1, \"a\": 2 }";
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    {
        auto recipe = sample_recipe();
        recipe.source_weights.self = 0.8;
        assert_invalid([&] { validate_training_recipe(recipe); });
        recipe.source_weights = {0.0, 0.0, 0.0, 1.1};
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    // Malformed JSON, numbers, and unsupported versions fail closed.
    {
        const auto recipe = sample_recipe();
        const std::string serialized = serialize_training_recipe(recipe);
        assert_invalid([&] {
            static_cast<void>(deserialize_training_recipe("{"));
        });
        std::string invalid_exponent = serialized;
        const auto weight = invalid_exponent.find("0.25");
        assert(weight != std::string::npos);
        invalid_exponent.replace(weight, 4, "1e+-2");
        assert_invalid([&] {
            static_cast<void>(deserialize_training_recipe(invalid_exponent));
        });
        std::string unsupported = serialized;
        const auto version = unsupported.find("\"schema_version\":1");
        assert(version != std::string::npos);
        unsupported.replace(version, 18, "\"schema_version\":2");
        assert_invalid([&] {
            static_cast<void>(deserialize_training_recipe(unsupported));
        });
    }

    // Escaped quotes, newlines, and UTF-8 survive serialization.
    {
        auto recipe = sample_recipe();
        recipe.datasets.front().dataset_id = "quoted\"id\n雪";
        const std::vector<DatasetRegistryReference> special_ids{
            {"quoted\"id\n雪", "revision-b"},
            {"dataset-a", "revision-a"}
        };
        validate_training_recipe(recipe, special_ids);
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        assert(decoded.datasets.front().dataset_id == "dataset-a");
        assert(decoded.datasets.back().dataset_id == "quoted\"id\n雪");
    }

    return 0;
}
