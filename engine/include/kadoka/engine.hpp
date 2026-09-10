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

struct SearchResult {
    Move best_move{};
    std::int32_t score_cp{0};
    std::uint64_t nodes{0};
    unsigned depth{0};
    std::string info{};
};

class Engine {
public:
    virtual ~Engine() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual SearchResult search(
        const Position& position,
        const SearchLimits& limits
    ) = 0;
};

} // namespace kadoka::shogi
