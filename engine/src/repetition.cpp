#include "kadoka/repetition.hpp"

#include "kadoka/attack.hpp"

#include <vector>

namespace kadoka::shogi {
namespace {

bool same_repetition_state(const Position& lhs, const Position& rhs) {
    if (lhs.side_to_move() != rhs.side_to_move()) {
        return false;
    }
    if (lhs.hands().black != rhs.hands().black || lhs.hands().white != rhs.hands().white) {
        return false;
    }

    const Board& lhs_board = lhs.board();
    const Board& rhs_board = rhs.board();
    for (std::size_t index = 0; index < lhs_board.size(); ++index) {
        const Piece& lhs_piece = lhs_board[index];
        const Piece& rhs_piece = rhs_board[index];
        if (lhs_piece.type != rhs_piece.type) {
            return false;
        }
        if (!lhs_piece.empty() && lhs_piece.color != rhs_piece.color) {
            return false;
        }
    }
    return true;
}

bool checked_on_every_move(
    std::span<const Position> history,
    std::size_t first,
    std::size_t fourth,
    Color checking_side) {
    bool saw_move = false;
    for (std::size_t index = first + 1; index <= fourth; ++index) {
        const Position& after_move = history[index];
        const Color mover = opposite(after_move.side_to_move());
        if (mover != checking_side) {
            continue;
        }

        saw_move = true;
        if (!is_in_check(after_move, after_move.side_to_move())) {
            return false;
        }
    }
    return saw_move;
}

} // namespace

RepetitionResult adjudicate_repetition(std::span<const Position> history) {
    if (history.empty()) {
        return {};
    }

    std::vector<std::size_t> occurrences;
    occurrences.reserve(4);
    const Position& latest = history.back();
    for (std::size_t index = 0; index < history.size(); ++index) {
        if (same_repetition_state(history[index], latest)) {
            occurrences.push_back(index);
        }
    }

    if (occurrences.size() < 4) {
        return {};
    }

    const std::size_t first = occurrences[occurrences.size() - 4];
    const std::size_t fourth = occurrences.back();
    const bool black_continuous = checked_on_every_move(history, first, fourth, Color::Black);
    const bool white_continuous = checked_on_every_move(history, first, fourth, Color::White);

    RepetitionResult result;
    result.first_occurrence = first;
    result.fourth_occurrence = fourth;

    if (black_continuous && !white_continuous) {
        result.status = RepetitionStatus::BlackLosesPerpetualCheck;
    } else if (white_continuous && !black_continuous) {
        result.status = RepetitionStatus::WhiteLosesPerpetualCheck;
    } else {
        // A legal game normally cannot make both players the unique continuous
        // checker. Treat any such malformed/ambiguous history as ordinary
        // repetition rather than assigning an arbitrary loser.
        result.status = RepetitionStatus::Draw;
    }
    return result;
}

} // namespace kadoka::shogi
