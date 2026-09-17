#include "kadoka/runtime/match_clock.hpp"

#include <algorithm>
#include <stdexcept>

namespace kadoka::shogi::runtime {
namespace {

using Nanoseconds = std::chrono::nanoseconds;
using Milliseconds = std::chrono::milliseconds;

[[nodiscard]] Nanoseconds nonnegative(Nanoseconds value) noexcept {
    return std::max(value, Nanoseconds::zero());
}

[[nodiscard]] Milliseconds ceil_milliseconds(Nanoseconds value) {
    if (value <= Nanoseconds::zero()) {
        return Milliseconds::zero();
    }
    return std::chrono::ceil<Milliseconds>(value);
}

} // namespace

MatchClock::SideState MatchClock::make_side(std::optional<PlayerTimeControl> control) {
    SideState state;
    state.control = control;
    if (!control.has_value()) {
        return state;
    }
    if (control->main_time < Milliseconds::zero()
        || control->byoyomi < Milliseconds::zero()) {
        throw std::invalid_argument("match clock durations must be non-negative");
    }
    state.main_remaining =
        std::chrono::duration_cast<Nanoseconds>(control->main_time);
    return state;
}

MatchClock::MatchClock(
    std::optional<PlayerTimeControl> black,
    std::optional<PlayerTimeControl> white)
    : black_(make_side(black)),
      white_(make_side(white)) {}

MatchClock::SideState& MatchClock::side(Color color) noexcept {
    return color == Color::Black ? black_ : white_;
}

const MatchClock::SideState& MatchClock::side(Color color) const noexcept {
    return color == Color::Black ? black_ : white_;
}

void MatchClock::begin_turn(Color color) noexcept {
    SideState& state = side(color);
    state.byoyomi_used_this_turn = Nanoseconds::zero();
    state.elapsed_this_turn = Nanoseconds::zero();
}

SearchLimits MatchClock::effective_search_limits(
    Color color,
    const SearchLimits& configured) const {
    SearchLimits effective = configured;
    const SideState& state = side(color);
    if (!state.control.has_value()) {
        return effective;
    }

    const Nanoseconds byoyomi =
        std::chrono::duration_cast<Nanoseconds>(state.control->byoyomi);
    const Nanoseconds byoyomi_remaining =
        nonnegative(byoyomi - state.byoyomi_used_this_turn);
    const Milliseconds clock_budget =
        ceil_milliseconds(state.main_remaining + byoyomi_remaining);

    if (!effective.time_limit.has_value()
        || clock_budget < *effective.time_limit) {
        effective.time_limit = clock_budget;
    }
    return effective;
}

ClockChargeResult MatchClock::charge(
    Color color,
    Nanoseconds elapsed) noexcept {
    SideState& state = side(color);
    elapsed = nonnegative(elapsed);
    state.total_elapsed += elapsed;
    state.elapsed_this_turn += elapsed;

    if (!state.control.has_value()) {
        return ClockChargeResult{false, state.elapsed_this_turn};
    }

    const Nanoseconds from_main = std::min(state.main_remaining, elapsed);
    state.main_remaining -= from_main;
    const Nanoseconds overflow = elapsed - from_main;
    state.byoyomi_used_this_turn += overflow;

    const Nanoseconds byoyomi =
        std::chrono::duration_cast<Nanoseconds>(state.control->byoyomi);
    return ClockChargeResult{
        state.byoyomi_used_this_turn > byoyomi,
        state.elapsed_this_turn,
    };
}

MatchClockSnapshot MatchClock::snapshot() const noexcept {
    MatchClockSnapshot result;
    if (black_.control.has_value()) {
        result.black_main_remaining =
            std::chrono::duration_cast<Milliseconds>(black_.main_remaining);
    }
    if (white_.control.has_value()) {
        result.white_main_remaining =
            std::chrono::duration_cast<Milliseconds>(white_.main_remaining);
    }
    result.black_elapsed = black_.total_elapsed;
    result.white_elapsed = white_.total_elapsed;
    return result;
}

} // namespace kadoka::shogi::runtime
