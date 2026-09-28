#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace kadoka::shogi::training {

struct TrainingExample {
    std::string input{};
    std::string target{};
};

struct TrainingConfig {
    std::uint64_t seed{0};
    std::size_t epochs{1};
    std::size_t checkpoint_interval_epochs{1};
};

struct EvaluationMetrics {
    std::size_t samples{0};
    std::size_t correct{0};
    double accuracy{0.0};
};

struct TrainingProgress {
    std::size_t epoch{0};
    EvaluationMetrics training{};
    EvaluationMetrics validation{};
};

struct TrainerCheckpoint {
    std::string trainer_id{};
    std::size_t epoch{0};
    std::string payload{};
};

struct TrainerCallbacks {
    std::function<void(const TrainingProgress&)> on_progress{};
    std::function<void(const TrainerCheckpoint&)> on_checkpoint{};
};

struct TrainingResult {
    std::vector<TrainingProgress> history{};
};

class Trainer {
public:
    virtual ~Trainer() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual TrainingResult train(
        std::span<const TrainingExample> training,
        std::span<const TrainingExample> validation,
        const TrainingConfig& config,
        const TrainerCallbacks& callbacks = {}
    ) = 0;
    [[nodiscard]] virtual EvaluationMetrics evaluate(
        std::span<const TrainingExample> examples
    ) const = 0;
};

[[nodiscard]] std::string serialize_training_config(
    const TrainingConfig& config
);

class ReferenceTrainer final : public Trainer {
public:
    [[nodiscard]] std::string id() const override;
    [[nodiscard]] TrainingResult train(
        std::span<const TrainingExample> training,
        std::span<const TrainingExample> validation,
        const TrainingConfig& config,
        const TrainerCallbacks& callbacks = {}
    ) override;
    [[nodiscard]] EvaluationMetrics evaluate(
        std::span<const TrainingExample> examples
    ) const override;

    [[nodiscard]] std::string serialize_checkpoint() const;

private:
    std::map<std::string, std::map<std::string, std::size_t>> counts_{};
};

} // namespace kadoka::shogi::training
