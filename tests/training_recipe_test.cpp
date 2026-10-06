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

OfficialTrainingRecipe sample_recipe() {
    OfficialTrainingRecipe recipe;
    recipe.datasets = {{"dataset-b", 0.25}, {"dataset-a", 1.5}};
    recipe.source_weights = {0.1, 0.0, 0.2, 0.7};
    recipe.architecture_id = "policy-value-v1";
    recipe.architecture_version = "1";
    recipe.config_hash = "sha256:abc123";
    recipe.champion_generation = 3;
    recipe.autotune_config_id = "autotune-7";
    recipe.evaluation_result_id = "evaluation-9";
    return recipe;
}

} // namespace

int main() {
    const std::array<std::string, 2> registered{"dataset-a", "dataset-b"};
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

    {
        auto first = sample_recipe();
        auto second = first;
        std::reverse(second.datasets.begin(), second.datasets.end());
        assert(serialize_training_recipe(first) == serialize_training_recipe(second));
    }

    {
        auto recipe = sample_recipe();
        recipe.autotune_config_id.reset();
        recipe.evaluation_result_id.reset();
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        assert(!decoded.autotune_config_id.has_value());
        assert(!decoded.evaluation_result_id.has_value());
    }

    {
        auto recipe = sample_recipe();
        recipe.datasets.push_back({"dataset-a", 1.0});
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    {
        auto recipe = sample_recipe();
        assert_invalid([&] { validate_training_recipe(recipe, std::span<const std::string>{}); });
        assert_invalid([&] {
            static_cast<void>(deserialize_training_recipe(
                serialize_training_recipe(recipe), std::span<const std::string>{}
            ));
        });
    }

    {
        auto recipe = sample_recipe();
        recipe.datasets.front().weight = 0.0;
        assert_invalid([&] { validate_training_recipe(recipe); });
        recipe.datasets.front().weight = std::numeric_limits<double>::infinity();
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

    {
        auto recipe = sample_recipe();
        recipe.source_weights.self = 0.8;
        assert_invalid([&] { validate_training_recipe(recipe); });
        recipe.source_weights = {0.0, 0.0, 0.0, 1.1};
        assert_invalid([&] { validate_training_recipe(recipe); });
    }

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

    {
        auto recipe = sample_recipe();
        recipe.datasets.front().dataset_id = "quoted\"id\n雪";
        const std::vector<std::string> special_ids{"quoted\"id\n雪", "dataset-a"};
        validate_training_recipe(recipe, special_ids);
        const auto decoded = deserialize_training_recipe(serialize_training_recipe(recipe));
        assert(decoded.datasets.front().dataset_id == "dataset-a");
        assert(decoded.datasets.back().dataset_id == "quoted\"id\n雪");
    }

    return 0;
}
