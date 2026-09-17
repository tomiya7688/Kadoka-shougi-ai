#pragma once

#include "kadoka/position.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace kadoka::shogi {

struct SearchLimits {
    std::optional<std::chrono::milliseconds> time_limit{};
    std::optional<std::uint64_t> node_limit{};
    std::optional<unsigned> depth_limit{};
};

enum class EngineAction : std::uint8_t {
    Move,
    Resign,
    DeclareEnteringKing,
    OfferMutualImpasse,
};

enum class MutualImpasseResponse : std::uint8_t {
    Decline,
    Accept,
};

struct SearchResult {
    Move best_move{};
    std::int32_t score_cp{0};
    std::uint64_t nodes{0};
    unsigned depth{0};
    std::string info{};
    // Defaults to Move so existing engines remain source-compatible.
    EngineAction action{EngineAction::Move};
};

class Engine {
public:
    virtual ~Engine() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual SearchResult search(
        const Position& position,
        const SearchLimits& limits
    ) = 0;

    // Agreement is intentionally out-of-band from the opponent's normal turn.
    // Existing engines decline by default and therefore remain compatible.
    [[nodiscard]] virtual MutualImpasseResponse respond_to_mutual_impasse_offer(
        const Position&
    ) {
        return MutualImpasseResponse::Decline;
    }
};

} // namespace kadoka::shogi
