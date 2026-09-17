#include "kadoka/runtime/match_clock.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

int main() {
    {
        MatchClock clock{
            PlayerTimeControl{1000ms, 200ms},
            std::nullopt,
        };
        clock.begin_turn(Color::Black);

        SearchLimits configured;
        const SearchLimits first = clock.effective_search_limits(Color::Black, configured);
        assert(first.time_limit == 1200ms);

        const ClockChargeResult a = clock.charge(Color::Black, 700ms);
        assert(!a.time_forfeit);
        assert(a.elapsed_this_turn == 700ms);
        assert(clock.snapshot().black_main_remaining == 300ms);

        const SearchLimits second = clock.effective_search_limits(Color::Black, configured);
        assert(second.time_limit == 500ms);

        const ClockChargeResult b = clock.charge(Color::Black, 400ms);
        assert(!b.time_forfeit);
        assert(b.elapsed_this_turn == 1100ms);
        assert(clock.snapshot().black_main_remaining == 0ms);

        const SearchLimits third = clock.effective_search_limits(Color::Black, configured);
        assert(third.time_limit == 100ms);

        const ClockChargeResult c = clock.charge(Color::Black, 101ms);
        assert(c.time_forfeit);
        assert(c.elapsed_this_turn == 1201ms);
    }

    {
        MatchClock clock{
            PlayerTimeControl{0ms, 100ms},
            std::nullopt,
        };

        clock.begin_turn(Color::Black);
        assert(!clock.charge(Color::Black, 60ms).time_forfeit);

        // Byoyomi resets only when the next turn for that side begins.
        clock.begin_turn(Color::Black);
        assert(!clock.charge(Color::Black, 60ms).time_forfeit);
        assert(clock.snapshot().black_main_remaining == 0ms);
        assert(clock.snapshot().black_elapsed == 120ms);
    }

    {
        MatchClock clock{
            PlayerTimeControl{500ms, 100ms},
            std::nullopt,
        };
        clock.begin_turn(Color::Black);

        SearchLimits configured;
        configured.time_limit = 50ms;
        const SearchLimits effective =
            clock.effective_search_limits(Color::Black, configured);
        assert(effective.time_limit == 50ms);
    }

    {
        MatchClock clock{std::nullopt, std::nullopt};
        clock.begin_turn(Color::Black);

        SearchLimits configured;
        configured.time_limit = 25ms;
        const SearchLimits effective =
            clock.effective_search_limits(Color::Black, configured);
        assert(effective.time_limit == 25ms);

        assert(!clock.charge(Color::Black, 10ms).time_forfeit);
        const MatchClockSnapshot snapshot = clock.snapshot();
        assert(!snapshot.black_main_remaining.has_value());
        assert(snapshot.black_elapsed == 10ms);
    }

    {
        bool threw = false;
        try {
            (void)MatchClock{
                PlayerTimeControl{-1ms, 0ms},
                std::nullopt,
            };
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }

    return 0;
}
