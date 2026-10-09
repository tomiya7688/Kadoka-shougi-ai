#include "kadoka/ai_package/path_history.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace kadoka::shogi::ai_package;

namespace {

constexpr const char* kStartSfen =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
constexpr const char* kAfterFirstMove =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2";
constexpr const char* kAfterSecondMove =
    "lnsgkgsnl/1r5b1/ppppppppp/9/2p6/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3";

template <class Callback>
void assert_invalid(Callback&& callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main() {
    FullMovePathHistory full{kStartSfen};
    assert(full.moves_usi().empty());
    full.record("7g7f", kAfterFirstMove);
    full.record("3c3d", kAfterSecondMove);

    const FullMovePathHistory restored_full =
        deserialize_full_move_path_history(full.serialize());
    assert(restored_full.current_position_sfen() == kAfterSecondMove);
    assert((restored_full.moves_usi() == std::vector<std::string>{"7g7f", "3c3d"}));
    assert(restored_full.identity() == full.identity());

    FullMovePathHistory different_path{kStartSfen};
    different_path.record("2g2f", kAfterFirstMove);
    different_path.record("3c3d", kAfterSecondMove);
    assert(different_path.current_position_sfen() == full.current_position_sfen());
    assert(different_path.identity() != full.identity());

    LastNMovePathHistory recent{2, kStartSfen};
    recent.record("7g7f", kAfterFirstMove);
    recent.record("3c3d", kAfterSecondMove);
    recent.record("8h2b+", kAfterSecondMove);
    assert(recent.moves_usi().size() == 2);
    assert((recent.moves_usi() == std::vector<std::string>{"3c3d", "8h2b+"}));

    const auto restored_recent = deserialize_path_history(recent.serialize());
    const auto* typed_recent = dynamic_cast<const LastNMovePathHistory*>(
        restored_recent.get()
    );
    assert(typed_recent != nullptr);
    assert(typed_recent->capacity() == 2);
    assert(typed_recent->current_position_sfen() == kAfterSecondMove);
    assert(typed_recent->moves_usi() == recent.moves_usi());
    assert(typed_recent->identity() == recent.identity());

    LastNMovePathHistory no_moves{0, kStartSfen};
    no_moves.record("7g7f", kAfterFirstMove);
    assert(no_moves.moves_usi().empty());
    assert(no_moves.current_position_sfen() == kAfterFirstMove);

    assert_invalid([&] { full.record("", kAfterFirstMove); });
    assert_invalid([&] { full.record("7g7f", ""); });
    assert_invalid([&] {
        (void)deserialize_path_history(full.serialize() + "trailing");
    });
    assert_invalid([&] {
        (void)deserialize_path_history(std::string{"KPH\0", 4});
    });
}

