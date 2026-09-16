#include "kadoka/terminal.hpp"

#include "kadoka/attack.hpp"
#include "kadoka/movegen.hpp"

namespace kadoka::shogi {

TerminalPositionResult adjudicate_terminal_position(const Position& position) {
    if (!generate_legal_moves(position).empty()) {
        return {};
    }

    const Color side = position.side_to_move();
    if (is_in_check(position, side)) {
        return TerminalPositionResult{
            TerminalPositionStatus::Checkmate,
            opposite(side),
            side,
        };
    }

    // Keep this distinct from checkmate. It is useful as a diagnostic for
    // malformed/non-standard positions and avoids inventing a winner when the
    // authoritative rules do not establish one from check status alone.
    return TerminalPositionResult{
        TerminalPositionStatus::NoLegalMoves,
        std::nullopt,
        std::nullopt,
    };
}

} // namespace kadoka::shogi
