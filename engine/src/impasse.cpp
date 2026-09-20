#include "kadoka/impasse.hpp"

#include "kadoka/attack.hpp"

#include <array>
#include <cstddef>

namespace kadoka::shogi {
namespace {

constexpr std::array<PieceType, 7> hand_piece_types{
    PieceType::Pawn,
    PieceType::Lance,
    PieceType::Knight,
    PieceType::Silver,
    PieceType::Gold,
    PieceType::Bishop,
    PieceType::Rook,
};

[[nodiscard]] constexpr bool is_enemy_camp(Color color, std::uint8_t rank) noexcept {
    return color == Color::Black ? rank <= 3 : rank >= 7;
}

[[nodiscard]] constexpr std::uint32_t impasse_piece_points(PieceType type) noexcept {
    switch (type) {
    case PieceType::Bishop:
    case PieceType::Rook:
    case PieceType::Horse:
    case PieceType::Dragon:
        return 5;
    case PieceType::Pawn:
    case PieceType::Lance:
    case PieceType::Knight:
    case PieceType::Silver:
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        return 1;
    case PieceType::King:
    case PieceType::None:
        return 0;
    }
    return 0;
}

[[nodiscard]] std::uint32_t hand_points(const Position& position, Color color) {
    std::uint32_t points = 0;
    for (const PieceType type : hand_piece_types) {
        points += static_cast<std::uint32_t>(position.hand_count(color, type)) * impasse_piece_points(type);
    }
    return points;
}

[[nodiscard]] MutualImpasseVerdict adjudicate_24_point(
    std::uint32_t black_points,
    std::uint32_t white_points) {
    const bool black_has_24 = black_points >= 24;
    const bool white_has_24 = white_points >= 24;
    if (black_has_24 && white_has_24) {
        return MutualImpasseVerdict::Replay;
    }
    if (!black_has_24 && white_has_24) {
        return MutualImpasseVerdict::BlackLoses;
    }
    if (black_has_24 && !white_has_24) {
        return MutualImpasseVerdict::WhiteLoses;
    }
    return MutualImpasseVerdict::InvalidMaterial;
}

[[nodiscard]] MutualImpasseVerdict adjudicate_27_point(
    std::uint32_t black_points,
    std::uint32_t white_points,
    bool white_wins_tie) {
    const bool black_qualifies = black_points >= 27;
    const bool white_qualifies = white_points >= 27;

    if (black_points == 27 && white_points == 27) {
        return white_wins_tie ? MutualImpasseVerdict::BlackLoses : MutualImpasseVerdict::Replay;
    }
    if (black_qualifies && !white_qualifies) {
        return MutualImpasseVerdict::WhiteLoses;
    }
    if (!black_qualifies && white_qualifies) {
        return MutualImpasseVerdict::BlackLoses;
    }
    return MutualImpasseVerdict::InvalidMaterial;
}

} // namespace

ImpasseAnalysis analyze_impasse(const Position& position, Color color) {
    ImpasseAnalysis analysis;
    const std::uint32_t held_points = hand_points(position, color);
    analysis.total_points = held_points;
    analysis.declaration_points = held_points;

    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        for (std::uint8_t file = 1; file <= 9; ++file) {
            const Piece piece = position.at(Square{file, rank});
            if (piece.empty() || piece.color != color) {
                continue;
            }

            const bool in_enemy_camp = is_enemy_camp(color, rank);
            if (piece.type == PieceType::King) {
                if (in_enemy_camp) {
                    analysis.king_in_enemy_camp = true;
                }
                continue;
            }

            const std::uint32_t points = impasse_piece_points(piece.type);
            analysis.total_points += points;
            if (in_enemy_camp) {
                analysis.declaration_points += points;
                ++analysis.enemy_camp_piece_count;
            }
        }
    }

    analysis.in_check = is_in_check(position, color);
    return analysis;
}

EnteringKingDeclarationResult adjudicate_entering_king_declaration(
    const Position& position,
    Color declarer) {
    EnteringKingDeclarationResult result;
    result.analysis = analyze_impasse(position, declarer);
    result.declarer_to_move = position.side_to_move() == declarer;
    result.before_500_moves = position.ply() <= 500;

    const bool structural_conditions = result.declarer_to_move
        && result.before_500_moves
        && result.analysis.king_in_enemy_camp
        && result.analysis.enemy_camp_piece_count >= 10
        && !result.analysis.in_check;

    if (!structural_conditions) {
        result.verdict = EnteringKingDeclarationVerdict::Loss;
        return result;
    }

    if (result.analysis.declaration_points >= 31) {
        result.verdict = EnteringKingDeclarationVerdict::Win;
    } else if (result.analysis.declaration_points >= 24) {
        result.verdict = EnteringKingDeclarationVerdict::Replay;
    } else {
        result.verdict = EnteringKingDeclarationVerdict::Loss;
    }
    return result;
}

bool is_mutual_impasse_agreement_position(const Position& position) {
    return analyze_impasse(position, Color::Black).king_in_enemy_camp
        || analyze_impasse(position, Color::White).king_in_enemy_camp;
}

MutualImpasseResult adjudicate_mutual_impasse_points(
    const Position& position,
    MutualImpassePolicy policy) {
    MutualImpasseResult result;
    result.black_points = analyze_impasse(position, Color::Black).total_points;
    result.white_points = analyze_impasse(position, Color::White).total_points;

    switch (policy) {
    case MutualImpassePolicy::Jsa24Point:
        result.verdict = adjudicate_24_point(result.black_points, result.white_points);
        break;
    case MutualImpassePolicy::Tournament27PointReplayTie:
        result.verdict = adjudicate_27_point(result.black_points, result.white_points, false);
        break;
    case MutualImpassePolicy::Tournament27PointWhiteWinsTie:
        result.verdict = adjudicate_27_point(result.black_points, result.white_points, true);
        break;
    }
    return result;
}

Move500ImpasseStatus adjudicate_500_move_impasse(const std::vector<Position>& history) {
    if (history.empty()) {
        return Move500ImpasseStatus::None;
    }

    std::size_t threshold_index = history.size();
    for (std::size_t index = 0; index < history.size(); ++index) {
        if (history[index].ply() == 501) {
            threshold_index = index;
            break;
        }
    }

    // Without the exact position after move 500, the Core cannot know whether
    // a check sequence was already in progress. Never guess from a later SFEN.
    if (threshold_index == history.size()) {
        return Move500ImpasseStatus::None;
    }

    const Position& threshold = history[threshold_index];
    if (!is_in_check(threshold, threshold.side_to_move())) {
        return Move500ImpasseStatus::Replay;
    }

    const Color checking_side = opposite(threshold.side_to_move());
    for (std::size_t index = threshold_index + 1; index < history.size(); ++index) {
        const Color mover = history[index - 1].side_to_move();
        if (mover != checking_side) {
            continue;
        }

        const Position& after = history[index];
        if (!is_in_check(after, after.side_to_move())) {
            return Move500ImpasseStatus::Replay;
        }
    }

    return Move500ImpasseStatus::None;
}

} // namespace kadoka::shogi
