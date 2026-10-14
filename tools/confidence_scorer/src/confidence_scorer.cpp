#include "kadoka/confidence/confidence_scorer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace kadoka::confidence {

ConfidenceResult DefaultConfidenceScorer::score(const ConfidenceInputs& inputs) const {
    const auto require_finite_nonnegative = [](const std::optional<double>& value,
                                               const std::string_view name) {
        if (value && (!std::isfinite(*value) || *value < 0.0))
            throw std::invalid_argument(std::string(name) + " must be finite and nonnegative");
    };
    const auto require_probability = [](const std::optional<double>& value, const std::string_view name) {
        if (value && (!std::isfinite(*value) || *value < 0.0 || *value > 1.0))
            throw std::invalid_argument(std::string(name) + " must be within [0, 1]");
    };

    if (inputs.source_rating && !std::isfinite(*inputs.source_rating))
        throw std::invalid_argument("source_rating must be finite");
    require_finite_nonnegative(inputs.source_uncertainty, "source_uncertainty");
    require_finite_nonnegative(inputs.candidate_gap_cp, "candidate_gap_cp");
    require_probability(inputs.policy_entropy, "policy_entropy");
    require_probability(inputs.engine_disagreement, "engine_disagreement");
    require_probability(inputs.result_consistency, "result_consistency");

    double total = 0.0;
    std::size_t evidence_count = 0;
    const auto add_evidence = [&total, &evidence_count](const double value) {
        total += std::clamp(value, 0.0, 1.0);
        ++evidence_count;
    };

    // rating自体は強さを示し、信頼性は示さないため、RDだけを根拠にする。
    if (inputs.source_uncertainty)
        add_evidence(1.0 / (1.0 + *inputs.source_uncertainty / 350.0));

    // 異なる単位の探索量を、参照値で飽和する0から1の指標へ揃える。
    if (inputs.search_depth) {
        const double depth = static_cast<double>(*inputs.search_depth);
        add_evidence(depth / (depth + 8.0));
    }
    if (inputs.search_nodes) {
        constexpr double node_reference = 1'000'000.0;
        add_evidence(std::log1p(static_cast<double>(*inputs.search_nodes)) / std::log1p(node_reference));
    }
    if (inputs.search_time_ms) {
        constexpr double time_reference_ms = 30'000.0;
        add_evidence(std::log1p(static_cast<double>(*inputs.search_time_ms)) / std::log1p(time_reference_ms));
    }
    if (inputs.candidate_gap_cp) {
        const double gap = *inputs.candidate_gap_cp;
        add_evidence(gap == 0.0 ? 0.0 : 1.0 / (1.0 + 100.0 / gap));
    }
    if (inputs.policy_entropy)
        add_evidence(1.0 - *inputs.policy_entropy);
    if (inputs.engine_disagreement)
        add_evidence(1.0 - *inputs.engine_disagreement);
    if (inputs.result_consistency)
        add_evidence(*inputs.result_consistency);

    if (evidence_count == 0)
        throw std::invalid_argument("at least one confidence evidence input is required");

    const double confidence = std::clamp(total / static_cast<double>(evidence_count), 0.0, 1.0);
    return {inputs, confidence, 1.0 - confidence, evidence_count};
}

} // namespace kadoka::confidence
