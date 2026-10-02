#include "kadoka/selector/move_selector.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kadoka::shogi::selector {
namespace {

struct LegalScore {
    std::string move;
    double score;
};

std::vector<LegalScore> legal_scores(const SelectionRequest& request) {
    std::unordered_set<std::string_view> legal;
    legal.reserve(request.legal_moves.size());
    for (const std::string& move : request.legal_moves) {
        if (!move.empty()) legal.insert(move);
    }

    std::unordered_map<std::string, double> best_score;
    for (const ScoredMove& candidate : request.scored_moves) {
        if (candidate.usi.empty() || !std::isfinite(candidate.score)
            || !legal.contains(candidate.usi)) {
            continue;
        }
        auto [entry, inserted] = best_score.emplace(candidate.usi, candidate.score);
        if (!inserted && candidate.score > entry->second) {
            entry->second = candidate.score;
        }
    }

    std::vector<LegalScore> result;
    result.reserve(best_score.size());
    for (auto& [move, score] : best_score) {
        result.push_back(LegalScore{std::move(move), score});
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.score != rhs.score) return lhs.score > rhs.score;
        return lhs.move < rhs.move;
    });
    return result;
}

MoveSelection fallback(const SelectionRequest& request) {
    if (request.fallback_move.has_value() && !request.fallback_move->empty()) {
        const auto match = std::find(
            request.legal_moves.begin(),
            request.legal_moves.end(),
            *request.fallback_move
        );
        if (match != request.legal_moves.end()) {
            return MoveSelection{*match, true};
        }
    }
    for (const std::string& move : request.legal_moves) {
        if (!move.empty()) return MoveSelection{move, true};
    }
    return {};
}

std::uint64_t next_random(std::uint64_t& state) noexcept {
    state += 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

MoveSelection choose_random(
    const std::vector<LegalScore>& candidates,
    std::uint64_t& state) {
    if (candidates.empty()) return {};
    std::size_t index = 0;
    if (candidates.size() > 1) {
        const std::uint64_t bound = static_cast<std::uint64_t>(candidates.size());
        const std::uint64_t threshold = -bound % bound;
        std::uint64_t value = 0;
        do {
            value = next_random(state);
        } while (value < threshold);
        index = static_cast<std::size_t>(value % bound);
    }
    return MoveSelection{candidates[index].move, false};
}

} // namespace

std::string_view ArgmaxSelector::id() const noexcept {
    return "kadoka.selector.argmax.v1";
}

MoveSelection ArgmaxSelector::select(const SelectionRequest& request) {
    const std::vector<LegalScore> candidates = legal_scores(request);
    if (candidates.empty()) return fallback(request);
    return MoveSelection{candidates.front().move, false};
}

TopKSelector::TopKSelector(std::size_t k, std::uint64_t seed)
    : k_(k), state_(seed) {
    if (k_ == 0) throw std::invalid_argument("top-k must be greater than zero");
}

std::string_view TopKSelector::id() const noexcept {
    return "kadoka.selector.top_k.v1";
}

MoveSelection TopKSelector::select(const SelectionRequest& request) {
    std::vector<LegalScore> candidates = legal_scores(request);
    if (candidates.empty()) return fallback(request);
    if (candidates.size() > k_) candidates.resize(k_);
    return choose_random(candidates, state_);
}

ThresholdSelector::ThresholdSelector(double threshold, std::uint64_t seed)
    : threshold_(threshold), state_(seed) {
    if (!std::isfinite(threshold_)) {
        throw std::invalid_argument("threshold must be finite");
    }
}

std::string_view ThresholdSelector::id() const noexcept {
    return "kadoka.selector.threshold.v1";
}

MoveSelection ThresholdSelector::select(const SelectionRequest& request) {
    std::vector<LegalScore> candidates = legal_scores(request);
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(), [this](const auto& item) {
            return item.score < threshold_;
        }),
        candidates.end()
    );
    if (candidates.empty()) return fallback(request);
    return choose_random(candidates, state_);
}

} // namespace kadoka::shogi::selector
