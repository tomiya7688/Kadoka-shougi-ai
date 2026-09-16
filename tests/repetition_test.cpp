#include "kadoka/repetition.hpp"

#include <cassert>
#include <vector>

using namespace kadoka::shogi;

namespace {

void append(std::vector<Position>& history, Move move) {
    history.push_back(history.back().after_move(move));
}

void append_quiet_king_cycle(std::vector<Position>& history) {
    append(history, Move{Square{5, 9}, Square{4, 9}, PieceType::None, false});
    append(history, Move{Square{5, 1}, Square{4, 1}, PieceType::None, false});
    append(history, Move{Square{4, 9}, Square{5, 9}, PieceType::None, false});
    append(history, Move{Square{4, 1}, Square{5, 1}, PieceType::None, false});
}

void append_black_check_cycle(std::vector<Position>& history) {
    append(history, Move{Square{4, 2}, Square{5, 2}, PieceType::None, false});
    append(history, Move{Square{5, 1}, Square{4, 1}, PieceType::None, false});
    append(history, Move{Square{5, 2}, Square{4, 2}, PieceType::None, false});
    append(history, Move{Square{4, 1}, Square{5, 1}, PieceType::None, false});
}

void append_white_check_cycle(std::vector<Position>& history) {
    append(history, Move{Square{4, 8}, Square{5, 8}, PieceType::None, false});
    append(history, Move{Square{5, 9}, Square{4, 9}, PieceType::None, false});
    append(history, Move{Square{5, 8}, Square{4, 8}, PieceType::None, false});
    append(history, Move{Square{4, 9}, Square{5, 9}, PieceType::None, false});
}

} // namespace

int main() {
    {
        std::vector<Position> history{
            Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b - 1")
        };

        append_quiet_king_cycle(history);
        append_quiet_king_cycle(history);
        assert(adjudicate_repetition(history).status == RepetitionStatus::None);

        append_quiet_king_cycle(history);
        const RepetitionResult result = adjudicate_repetition(history);
        assert(result.status == RepetitionStatus::Draw);
        assert(result.first_occurrence == 0);
        assert(result.fourth_occurrence == 12);
    }

    {
        std::vector<Position> history{
            Position::from_sfen("4k4/5R3/9/9/9/9/9/9/K8 b - 1")
        };

        append_black_check_cycle(history);
        append_black_check_cycle(history);
        append_black_check_cycle(history);

        const RepetitionResult result = adjudicate_repetition(history);
        assert(result.status == RepetitionStatus::BlackLosesPerpetualCheck);
        assert(result.first_occurrence == 0);
        assert(result.fourth_occurrence == 12);
    }

    {
        std::vector<Position> history{
            Position::from_sfen("k8/9/9/9/9/9/9/5r3/4K4 w - 1")
        };

        append_white_check_cycle(history);
        append_white_check_cycle(history);
        append_white_check_cycle(history);

        const RepetitionResult result = adjudicate_repetition(history);
        assert(result.status == RepetitionStatus::WhiteLosesPerpetualCheck);
        assert(result.first_occurrence == 0);
        assert(result.fourth_occurrence == 12);
    }

    {
        const Position no_hand = Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b - 1");
        const Position pawn_in_hand = Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b P 5");
        std::vector<Position> history{no_hand, pawn_in_hand, no_hand, pawn_in_hand, no_hand, pawn_in_hand};
        assert(adjudicate_repetition(history).status == RepetitionStatus::None);
    }

    return 0;
}
