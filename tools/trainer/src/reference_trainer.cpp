#include "kadoka/training/trainer.hpp"

#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace kadoka::shogi::training {
namespace {

void validate_config(const TrainingConfig& config) {
    if (config.epochs == 0) {
        throw std::invalid_argument("training epochs must be greater than zero");
    }
    if (config.checkpoint_interval_epochs == 0) {
        throw std::invalid_argument("checkpoint interval must be greater than zero");
    }
}

void validate_examples(std::span<const TrainingExample> examples) {
    for (const TrainingExample& example : examples) {
        if (example.input.empty() || example.target.empty()) {
            throw std::invalid_argument("training example input and target must not be empty");
        }
    }
}

std::string escape_json(std::string_view value) {
    std::string escaped;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (ch < 0x20) {
                constexpr char digits[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped += digits[ch >> 4];
                escaped += digits[ch & 0x0f];
            } else {
                escaped += static_cast<char>(ch);
            }
        }
    }
    return escaped;
}

} // namespace

std::string serialize_training_config(const TrainingConfig& config) {
    validate_config(config);
    std::ostringstream out;
    out << "{\"schema\":\"kadoka.training_config\",\"version\":1,\"seed\":"
        << config.seed << ",\"epochs\":" << config.epochs
        << ",\"checkpoint_interval_epochs\":"
        << config.checkpoint_interval_epochs << '}';
    return out.str();
}

std::string ReferenceTrainer::id() const {
    return "kadoka.reference_memorizer.v1";
}

TrainingResult ReferenceTrainer::train(
    std::span<const TrainingExample> training,
    std::span<const TrainingExample> validation,
    const TrainingConfig& config,
    const TrainerCallbacks& callbacks) {
    validate_config(config);
    validate_examples(training);
    validate_examples(validation);
    if (training.empty()) {
        throw std::invalid_argument("training dataset must not be empty");
    }

    counts_.clear();
    TrainingResult result;
    result.history.reserve(config.epochs);
    for (std::size_t epoch = 1; epoch <= config.epochs; ++epoch) {
        for (const TrainingExample& example : training) {
            ++counts_[example.input][example.target];
        }

        TrainingProgress progress{
            epoch,
            evaluate(training),
            evaluate(validation),
        };
        result.history.push_back(progress);
        if (callbacks.on_progress) callbacks.on_progress(progress);

        if (callbacks.on_checkpoint
            && (epoch % config.checkpoint_interval_epochs == 0
                || epoch == config.epochs)) {
            callbacks.on_checkpoint(TrainerCheckpoint{
                id(),
                epoch,
                serialize_checkpoint(),
            });
        }
    }
    return result;
}

EvaluationMetrics ReferenceTrainer::evaluate(
    std::span<const TrainingExample> examples) const {
    validate_examples(examples);
    EvaluationMetrics metrics;
    metrics.samples = examples.size();
    for (const TrainingExample& example : examples) {
        const auto input = counts_.find(example.input);
        if (input == counts_.end() || input->second.empty()) continue;

        auto best = input->second.begin();
        for (auto candidate = std::next(best); candidate != input->second.end(); ++candidate) {
            if (candidate->second > best->second) best = candidate;
        }
        if (best->first == example.target) ++metrics.correct;
    }
    if (metrics.samples != 0) {
        metrics.accuracy = static_cast<double>(metrics.correct)
            / static_cast<double>(metrics.samples);
    }
    return metrics;
}

std::string ReferenceTrainer::serialize_checkpoint() const {
    std::ostringstream out;
    out << "{\"schema\":\"kadoka.reference_trainer_checkpoint\",\"version\":1,\"model\":{";
    bool first_input = true;
    for (const auto& [input, targets] : counts_) {
        if (!first_input) out << ',';
        first_input = false;
        out << '"' << escape_json(input) << "\":{";
        bool first_target = true;
        for (const auto& [target, count] : targets) {
            if (!first_target) out << ',';
            first_target = false;
            out << '"' << escape_json(target) << "\":" << count;
        }
        out << '}';
    }
    out << "}}";
    return out.str();
}

} // namespace kadoka::shogi::training
