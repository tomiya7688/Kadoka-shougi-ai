#pragma once

#include "kadoka/engine.hpp"
#include "kadoka/types.hpp"

#include <chrono>
#include <optional>

namespace kadoka::shogi::runtime {

struct PlayerTimeControl {
    std::chrono::milliseconds main_time{0};
    std::chrono::milliseconds byoyomi{0};
};

struct MatchClockSnapshot {
    std::optional<std::chrono::milliseconds> black_main_remaining{};
    std::optional<std::chrono::milliseconds> white_main_remaining{};
    std::chrono::nanoseconds black_elapsed{0};
    std::chrono::nanoseconds white_elapsed{0};
};

struct ClockChargeResult {
    bool time_forfeit{false};
    std::chrono::nanoseconds elapsed_this_turn{0};
};

class MatchClock {
public:
    MatchClock(
        std::optional<PlayerTimeControl> black,
        std::optional<PlayerTimeControl> white
    );

    void begin_turn(Color side) noexcept;

    [[nodiscard]] SearchLimits effective_search_limits(
        Color side,
        const SearchLimits& configured
    ) const;

    [[nodiscard]] ClockChargeResult charge(
        Color side,
        std::chrono::nanoseconds elapsed
    ) noexcept;

    [[nodiscard]] MatchClockSnapshot snapshot() const noexcept;

private:
    struct SideState {
        std::optional<PlayerTimeControl> control{};
        std::chrono::nanoseconds main_remaining{0};
        std::chrono::nanoseconds byoyomi_used_this_turn{0};
        std::chrono::nanoseconds total_elapsed{0};
    };

    [[nodiscard]] SideState& side(Color color) noexcept;
    [[nodiscard]] const SideState& side(Color color) const noexcept;
    [[nodiscard]] static SideState make_side(std::optional<PlayerTimeControl> control);

    SideState black_{};
    SideState white_{};
};

} // namespace kadoka::shogi::runtime
