#include "kadoka/tree/tree_search_strategy.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace kadoka::shogi::tree {
namespace {

void validate_context(
    const PureTreeNode& parent,
    std::span<const PureTreeEdge> candidates) {
    if (parent.id.empty()) {
        throw std::invalid_argument("parent node id must not be empty");
    }
    if (!std::isfinite(parent.mean_value)
        || parent.mean_value < -1.0 || parent.mean_value > 1.0) {
        throw std::invalid_argument("parent mean_value must be finite and in [-1, 1]");
    }
    if (parent.wins > parent.visits
        || parent.losses > parent.visits - parent.wins
        || parent.draws > parent.visits - parent.wins - parent.losses) {
        throw std::invalid_argument("parent outcome counts exceed visits");
    }
    std::unordered_set<std::string_view> ids;
    ids.reserve(candidates.size());
    for (const PureTreeEdge& edge : candidates) {
        if (edge.id.empty() || edge.parent_node_id != parent.id
            || edge.child_node_id.empty() || edge.move_usi.empty()) {
            throw std::invalid_argument("candidate edge is incomplete or has another parent");
        }
        if (!ids.insert(edge.id).second) {
            throw std::invalid_argument("candidate edge ids must be unique");
        }
        if (!std::isfinite(edge.mean_value)
            || edge.mean_value < -1.0 || edge.mean_value > 1.0) {
            throw std::invalid_argument("edge mean_value must be finite and in [-1, 1]");
        }
        if (edge.wins > edge.visits
            || edge.losses > edge.visits - edge.wins
            || edge.draws > edge.visits - edge.wins - edge.losses) {
            throw std::invalid_argument("edge outcome counts exceed visits");
        }
    }
}

void validate_exploration(double exploration) {
    if (!std::isfinite(exploration) || exploration < 0.0) {
        throw std::invalid_argument("exploration must be finite and non-negative");
    }
}

} // namespace

std::optional<std::string> TreeSearchStrategy::choose_edge(
    const PureTreeNode& parent,
    std::span<const PureTreeEdge> candidates) const {
    std::vector<TreeSearchScore> scores = score_edges(parent, candidates);
    if (scores.empty()) return std::nullopt;
    const auto best = std::max_element(
        scores.begin(),
        scores.end(),
        [](const TreeSearchScore& lhs, const TreeSearchScore& rhs) {
            if (lhs.score != rhs.score) return lhs.score < rhs.score;
            return lhs.edge_id > rhs.edge_id;
        }
    );
    return best->edge_id;
}

UctStrategy::UctStrategy(double exploration)
    : exploration_(exploration) {
    validate_exploration(exploration_);
}

std::string_view UctStrategy::id() const noexcept {
    return "kadoka.tree_search.uct.v1";
}

std::vector<TreeSearchScore> UctStrategy::score_edges(
    const PureTreeNode& parent,
    std::span<const PureTreeEdge> candidates) const {
    validate_context(parent, candidates);
    std::vector<TreeSearchScore> result;
    result.reserve(candidates.size());
    const double parent_term = std::log(
        static_cast<double>(parent.visits) + 1.0
    );
    for (const PureTreeEdge& edge : candidates) {
        const double score = edge.visits == 0
            ? std::numeric_limits<double>::infinity()
            : edge.mean_value + exploration_
                * std::sqrt(parent_term / static_cast<double>(edge.visits));
        if (edge.visits != 0 && !std::isfinite(score)) {
            throw std::overflow_error("UCT score overflow");
        }
        result.push_back(TreeSearchScore{edge.id, score});
    }
    return result;
}

PuctStrategy::PuctStrategy(
    double exploration,
    TreePriorFunction prior_function)
    : exploration_(exploration), prior_function_(std::move(prior_function)) {
    validate_exploration(exploration_);
    if (!prior_function_) {
        throw std::invalid_argument("PUCT requires a prior function");
    }
}

std::string_view PuctStrategy::id() const noexcept {
    return "kadoka.tree_search.puct.v1";
}

std::vector<TreeSearchScore> PuctStrategy::score_edges(
    const PureTreeNode& parent,
    std::span<const PureTreeEdge> candidates) const {
    validate_context(parent, candidates);
    std::vector<TreeSearchScore> result;
    result.reserve(candidates.size());
    if (candidates.empty()) return result;

    std::vector<double> priors;
    priors.reserve(candidates.size());
    long double total_prior = 0.0L;
    for (const PureTreeEdge& edge : candidates) {
        const double prior = prior_function_(parent, edge);
        if (!std::isfinite(prior) || prior < 0.0) {
            throw std::invalid_argument("PUCT prior must be finite and non-negative");
        }
        priors.push_back(prior);
        total_prior += static_cast<long double>(prior);
    }
    if (!(total_prior > 0.0L) || !std::isfinite(total_prior)) {
        throw std::invalid_argument("PUCT priors must have a finite positive sum");
    }

    const double parent_scale = std::sqrt(static_cast<double>(parent.visits));
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const PureTreeEdge& edge = candidates[index];
        const double normalized_prior = static_cast<double>(
            static_cast<long double>(priors[index]) / total_prior
        );
        const double exploration = exploration_ * normalized_prior * parent_scale
            / (1.0 + static_cast<double>(edge.visits));
        const double score = edge.mean_value + exploration;
        if (!std::isfinite(score)) {
            throw std::overflow_error("PUCT score overflow");
        }
        result.push_back(TreeSearchScore{edge.id, score});
    }
    return result;
}

} // namespace kadoka::shogi::tree
