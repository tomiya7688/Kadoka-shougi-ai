#include "kadoka/selector/move_selector.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi::selector;

int main() {
    const std::vector<std::string> legal{"7g7f", "2g2f", "3g3f"};
    const std::vector<ScoredMove> scores{
        {"5c5d", 100.0}, // Illegal model output must never be selected.
        {"7g7f", 0.8},
        {"7g7f", 0.9}, // Duplicate score keeps the maximum.
        {"2g2f", 0.7},
        {"3g3f", 0.2},
        {"2g2f", std::numeric_limits<double>::quiet_NaN()},
    };
    const SelectionRequest request{scores, legal, std::nullopt};

    ArgmaxSelector argmax;
    assert(argmax.id() == "kadoka.selector.argmax.v1");
    const MoveSelection best = argmax.select(request);
    assert(best.move == "7g7f");
    assert(!best.used_fallback);

    const std::vector<ScoredMove> tied{{"7g7f", 0.5}, {"2g2f", 0.5}};
    const SelectionRequest tie_request{tied, legal, std::nullopt};
    assert(argmax.select(tie_request).move == "2g2f");

    TopKSelector topk_a{2, 42};
    TopKSelector topk_b{2, 42};
    assert(topk_a.id() == "kadoka.selector.top_k.v1");
    for (int i = 0; i < 32; ++i) {
        const MoveSelection a = topk_a.select(request);
        const MoveSelection b = topk_b.select(request);
        assert(a.move == b.move);
        assert(a.move == "7g7f" || a.move == "2g2f");
    }

    ThresholdSelector threshold_a{0.5, 77};
    ThresholdSelector threshold_b{0.5, 77};
    assert(threshold_a.id() == "kadoka.selector.threshold.v1");
    for (int i = 0; i < 32; ++i) {
        const MoveSelection a = threshold_a.select(request);
        const MoveSelection b = threshold_b.select(request);
        assert(a.move == b.move);
        assert(a.move == "7g7f" || a.move == "2g2f");
    }

    const SelectionRequest no_scores{{}, legal, std::string_view{"3g3f"}};
    const MoveSelection requested_fallback = argmax.select(no_scores);
    assert(requested_fallback.move == "3g3f");
    assert(requested_fallback.used_fallback);

    const SelectionRequest invalid_fallback{{}, legal, std::string_view{"5c5d"}};
    const MoveSelection legal_fallback = argmax.select(invalid_fallback);
    assert(legal_fallback.move == "7g7f");
    assert(legal_fallback.used_fallback);

    const std::vector<std::string> legal_with_empty{"", "2g2f"};
    const SelectionRequest empty_fallback{{}, legal_with_empty, std::string_view{""}};
    const MoveSelection non_empty_fallback = argmax.select(empty_fallback);
    assert(non_empty_fallback.move == "2g2f");
    assert(non_empty_fallback.used_fallback);

    const std::vector<ScoredMove> below_threshold{{"7g7f", 0.4}};
    const SelectionRequest threshold_fallback_request{
        below_threshold,
        legal,
        std::string_view{"3g3f"},
    };
    const MoveSelection threshold_fallback =
        threshold_a.select(threshold_fallback_request);
    assert(threshold_fallback.move == "3g3f");
    assert(threshold_fallback.used_fallback);

    const std::vector<std::string> no_legal_moves;
    const SelectionRequest terminal{{}, no_legal_moves, std::string_view{"7g7f"}};
    assert(!argmax.select(terminal).move.has_value());

    bool zero_topk_rejected = false;
    try {
        (void)TopKSelector{0};
    } catch (const std::invalid_argument&) {
        zero_topk_rejected = true;
    }
    assert(zero_topk_rejected);

    bool invalid_threshold_rejected = false;
    try {
        (void)ThresholdSelector{std::numeric_limits<double>::infinity()};
    } catch (const std::invalid_argument&) {
        invalid_threshold_rejected = true;
    }
    assert(invalid_threshold_rejected);

    bool nan_threshold_rejected = false;
    try {
        (void)ThresholdSelector{std::numeric_limits<double>::quiet_NaN()};
    } catch (const std::invalid_argument&) {
        nan_threshold_rejected = true;
    }
    assert(nan_threshold_rejected);
}
