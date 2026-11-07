#include "kadoka/training/training_recipe.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi::training;

namespace {

// {
//   責務: [check: Release設定でも有効な契約テスト判定を行う]
//   引数: [condition: 成功条件]
//   戻り値: [なし。失敗時はruntime_error]
// }
void check(bool condition) {
    if (!condition)
        throw std::runtime_error("training_recipe_test assertion failed");
}

// {
//   責務: [assert_invalid: 不正入力を渡した処理がinvalid_argumentを送出することを検証する]
//   処理: [処理を実行し、例外を捕捉してcheckする]
//   引数: [function: 実行する処理]
//   戻り値: [なし]
// }
template <class Function> void assert_invalid(Function&& function) {
    bool failed = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    check(failed);
}

// {
//   責務: [sample_recipe: 検証に使う有効なrecipeを作る]
//   処理: [代表的なdataset、source重み、model参照を設定する]
//   引数: [なし]
//   戻り値: [OfficialTrainingRecipe]
// }
OfficialTrainingRecipe sample_recipe() {
    OfficialTrainingRecipe recipe;
    recipe.datasets = {{"dataset-b", "revision-b", 0.25}, {"dataset-a", "revision-a", 1.5}};
    recipe.source_weights = {0.1, 0.0, 0.2, 0.7};
    recipe.architecture_id = "policy-value-v1";
    recipe.architecture_version = "1";
    recipe.effective_config_json = "{\"batch_size\":32,\"learning_rate\":0.01,\"optimizer\":\"adam\"}";
    recipe.config_hash = "sha256:2212b4020bc7b36774309460140f000ce55fc9b7b004c1e968185579c9b355c6";
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
    const std::array<DatasetRegistryReference, 2> registered{
        {{"dataset-a", "revision-a"}, {"dataset-b", "revision-b"}}};
    // Round-trip a valid recipe and check the canonical JSON representation.
    {
        const auto recipe = sample_recipe();
        validate_training_recipe(recipe, registered);
        const std::string serialized = serialize_training_recipe(recipe);
        const auto decoded = deserialize_training_recipe(serialized, registered);
        check(serialize_training_recipe(decoded) == serialized);
        check(decoded.datasets.front().dataset_id == "dataset-a");
        check(decoded.architecture_id == recipe.architecture_id);
        check(decoded.champion_generation == recipe.champion_generation);
        check(decoded.autotune_config_id == recipe.autotune_config_id);
    }

    // Dataset ordering in the input must not affect serialized bytes.
    {
        auto first = sample_recipe();
        auto second = first;
        std::reverse(second.datasets.begin(), second.datasets.end());
        check(serialize_training_recipe(first) == serialize_training_recipe(second));

        const std::string japanese_id("\xe3\x81\x82", 3);
        first.datasets = {{japanese_id, "revision-jp", 0.5}, {"z", "revision-ascii", 0.5}};
        const std::string utf8_ordered = serialize_training_recipe(first);
        check(utf8_ordered.find("\"dataset_id\":\"z\"") < utf8_ordered.find("\"dataset_id\":\"" + japanese_id + "\""));
    }

    // Optional references round-trip as JSON null.
    {
        auto recipe = sample_recipe();
        recipe.autotune_config_id.reset();
        recipe.evaluation_result_id.reset();
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        check(!decoded.autotune_config_id.has_value());
        check(!decoded.evaluation_result_id.has_value());
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
        const std::array<DatasetRegistryReference, 2> wrong_revisions{
            {{"dataset-a", "old-revision"}, {"dataset-b", "old-revision"}}};
        assert_invalid([&] { validate_training_recipe(recipe, wrong_revisions); });
        assert_invalid([&] {
            static_cast<void>(deserialize_training_recipe(serialize_training_recipe(recipe),
                                                          std::span<const DatasetRegistryReference>{}));
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
        recipe.config_hash = "sha256:not-a-digest";
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    {
        auto recipe = sample_recipe();
        recipe.source_weights.self = 0.8;
        assert_invalid([&] { validate_training_recipe(recipe); });
        recipe.source_weights = {0.0, 0.0, 0.0, 1.1};
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    // Generation accepts the full uint64 range and rejects non-integer or overflowing JSON numbers.
    {
        auto recipe = sample_recipe();
        recipe.champion_generation = std::numeric_limits<std::uint64_t>::max();
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        check(decoded.champion_generation == std::numeric_limits<std::uint64_t>::max());

        const std::string serialized = serialize_training_recipe(sample_recipe());
        for (const std::string invalid_generation : {"1.5", "-1", "1e3", "18446744073709551616"}) {
            std::string invalid = serialized;
            const std::string field = "\"champion_generation\":3";
            const auto value = invalid.find(field);
            check(value != std::string::npos);
            invalid.replace(value, field.size(), "\"champion_generation\":" + invalid_generation);
            assert_invalid([&] { static_cast<void>(deserialize_training_recipe(invalid)); });
        }
    }

    // Malformed JSON, numbers, and unsupported versions fail closed.
    {
        const auto recipe = sample_recipe();
        const std::string serialized = serialize_training_recipe(recipe);
        assert_invalid([&] { static_cast<void>(deserialize_training_recipe("{")); });
        std::string invalid_exponent = serialized;
        const auto weight = invalid_exponent.find("0.25");
        check(weight != std::string::npos);
        invalid_exponent.replace(weight, 4, "1e+-2");
        assert_invalid([&] { static_cast<void>(deserialize_training_recipe(invalid_exponent)); });
        std::string unsupported = serialized;
        const auto version = unsupported.find("\"schema_version\":1");
        check(version != std::string::npos);
        unsupported.replace(version, 18, "\"schema_version\":2");
        assert_invalid([&] { static_cast<void>(deserialize_training_recipe(unsupported)); });
    }

    // Non-canonical effective_config input must be rejected before normalization.
    {
        std::string noncanonical = serialize_training_recipe(sample_recipe());
        const std::string marker = "\"effective_config\":";
        const auto value = noncanonical.find(marker);
        check(value != std::string::npos);
        noncanonical.insert(value + marker.size(), " ");
        assert_invalid([&] { static_cast<void>(deserialize_training_recipe(noncanonical)); });
    }

    // JSON container nesting has a fixed limit to prevent stack exhaustion.
    {
        std::string deeply_nested = serialize_training_recipe(sample_recipe());
        const std::string marker = "\"effective_config\":{";
        const auto value = deeply_nested.find(marker);
        check(value != std::string::npos);
        const auto start = value + marker.size() - 1;
        const auto end = deeply_nested.find(",\"config_hash\"", start);
        check(end != std::string::npos);
        std::string nested = "{\"deep\":" + std::string(64, '[') + "0" + std::string(64, ']') + "}";
        deeply_nested.replace(start, end - start, nested);
        assert_invalid([&] { static_cast<void>(deserialize_training_recipe(deeply_nested)); });
    }

    // Invalid UTF-8 is rejected both when reading and writing recipe strings.
    {
        auto invalid_recipe = sample_recipe();
        invalid_recipe.architecture_id = std::string("\xc0\xaf", 2);
        assert_invalid([&] { static_cast<void>(serialize_training_recipe(invalid_recipe)); });

        std::string invalid_json = serialize_training_recipe(sample_recipe());
        const std::string field = "\"architecture_id\":\"policy-value-v1\"";
        const auto value = invalid_json.find(field);
        check(value != std::string::npos);
        invalid_json.replace(value, field.size(),
                             std::string("\"architecture_id\":\"") + std::string("\xc0\xaf", 2) + "\"");
        assert_invalid([&] { static_cast<void>(deserialize_training_recipe(invalid_json)); });
    }

    // Escaped quotes, newlines, and UTF-8 survive serialization.
    {
        auto recipe = sample_recipe();
        recipe.datasets.front().dataset_id = "quoted\"id\n雪";
        const std::vector<DatasetRegistryReference> special_ids{{"quoted\"id\n雪", "revision-b"},
                                                                {"dataset-a", "revision-a"}};
        validate_training_recipe(recipe, special_ids);
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        check(decoded.datasets.front().dataset_id == "dataset-a");
        check(decoded.datasets.back().dataset_id == "quoted\"id\n雪");
    }

    return 0;
}
