#include "kadoka/impasse.hpp"

#include <cassert>
#include <vector>

using namespace kadoka::shogi;

int main() {
    {
        assert(!is_mutual_impasse_agreement_position(Position::startpos()));

        const Position entered = Position::from_sfen(
            "4K4/9/9/9/9/9/9/9/4k4 b - 1"
        );
        assert(is_mutual_impasse_agreement_position(entered));
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2R2BP 1"
        );
        const EnteringKingDeclarationResult result =
            adjudicate_entering_king_declaration(position, Color::Black);
        assert(result.verdict == EnteringKingDeclarationVerdict::Win);
        assert(result.analysis.king_in_enemy_camp);
        assert(result.analysis.enemy_camp_piece_count == 10);
        assert(result.analysis.declaration_points == 31);
        assert(!result.analysis.in_check);
        assert(result.declarer_to_move);
        assert(result.before_500_moves);
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2RB 1"
        );
        const EnteringKingDeclarationResult result =
            adjudicate_entering_king_declaration(position, Color::Black);
        assert(result.verdict == EnteringKingDeclarationVerdict::Replay);
        assert(result.analysis.declaration_points == 25);
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b RB 1"
        );
        const EnteringKingDeclarationResult result =
            adjudicate_entering_king_declaration(position, Color::Black);
        assert(result.verdict == EnteringKingDeclarationVerdict::Loss);
        assert(result.analysis.declaration_points == 20);
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/4r4/9/9/9/9/4k4 b 2R2BP 1"
        );
        const EnteringKingDeclarationResult result =
            adjudicate_entering_king_declaration(position, Color::Black);
        assert(result.verdict == EnteringKingDeclarationVerdict::Loss);
        assert(result.analysis.in_check);
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 w 2R2BP 1"
        );
        const EnteringKingDeclarationResult result =
            adjudicate_entering_king_declaration(position, Color::Black);
        assert(result.verdict == EnteringKingDeclarationVerdict::Loss);
        assert(!result.declarer_to_move);
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2R2BP 501"
        );
        const EnteringKingDeclarationResult result =
            adjudicate_entering_king_declaration(position, Color::Black);
        assert(result.verdict == EnteringKingDeclarationVerdict::Loss);
        assert(!result.before_500_moves);
    }

    {
        const Position position = Position::from_sfen(
            "4k4/9/9/9/9/9/9/9/4K4 b 2R2B3P2r2b4p 1"
        );
        const MutualImpasseResult result = adjudicate_mutual_impasse_points(position);
        assert(result.black_points == 23);
        assert(result.white_points == 24);
        assert(result.verdict == MutualImpasseVerdict::BlackLoses);
    }

    {
        const Position position = Position::startpos();
        const MutualImpasseResult standard = adjudicate_mutual_impasse_points(position);
        assert(standard.black_points == 27);
        assert(standard.white_points == 27);
        assert(standard.verdict == MutualImpasseVerdict::Replay);

        const MutualImpasseResult replay_tie = adjudicate_mutual_impasse_points(
            position,
            MutualImpassePolicy::Tournament27PointReplayTie
        );
        assert(replay_tie.verdict == MutualImpasseVerdict::Replay);

        const MutualImpasseResult white_wins_tie = adjudicate_mutual_impasse_points(
            position,
            MutualImpassePolicy::Tournament27PointWhiteWinsTie
        );
        assert(white_wins_tie.verdict == MutualImpasseVerdict::BlackLoses);
    }

    {
        const Position initial = Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b - 500");
        const Position after_500 = initial.after_move(
            Move{Square{5, 9}, Square{4, 9}, PieceType::None, false}
        );
        const std::vector<Position> history{initial, after_500};
        assert(adjudicate_500_move_impasse(history) == Move500ImpasseStatus::Replay);
    }

    {
        const Position initial = Position::from_sfen("4k4/5R3/9/9/9/9/9/9/K8 b - 500");
        const Position check_500 = initial.after_move(
            Move{Square{4, 2}, Square{5, 2}, PieceType::None, false}
        );
        std::vector<Position> history{initial, check_500};
        assert(adjudicate_500_move_impasse(history) == Move500ImpasseStatus::None);

        const Position white_escape = check_500.after_move(
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false}
        );
        history.push_back(white_escape);
        assert(adjudicate_500_move_impasse(history) == Move500ImpasseStatus::None);

        const Position continued_check = white_escape.after_move(
            Move{Square{5, 2}, Square{4, 2}, PieceType::None, false}
        );
        history.push_back(continued_check);
        assert(adjudicate_500_move_impasse(history) == Move500ImpasseStatus::None);

        const Position second_escape = continued_check.after_move(
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false}
        );
        history.push_back(second_escape);
        assert(adjudicate_500_move_impasse(history) == Move500ImpasseStatus::None);

        const Position check_broken = second_escape.after_move(
            Move{Square{4, 2}, Square{3, 2}, PieceType::None, false}
        );
        history.push_back(check_broken);
        assert(adjudicate_500_move_impasse(history) == Move500ImpasseStatus::Replay);
    }

    return 0;
}
