#pragma once

#include "kadoka/position.hpp"

#include <cstdint>
#include <vector>

namespace kadoka::shogi {

struct ImpasseAnalysis {
    std::uint32_t total_points{0};
    std::uint32_t declaration_points{0};
    std::uint32_t enemy_camp_piece_count{0};
    bool king_in_enemy_camp{false};
    bool in_check{false};
};

enum class EnteringKingDeclarationVerdict : std::uint8_t {
    Win,
    Replay,
    Loss,
};

struct EnteringKingDeclarationResult {
    EnteringKingDeclarationVerdict verdict{EnteringKingDeclarationVerdict::Loss};
    ImpasseAnalysis analysis{};
    bool declarer_to_move{false};
    bool before_500_moves{false};
};

enum class MutualImpassePolicy : std::uint8_t {
    Jsa24Point,
    Tournament27PointReplayTie,
    Tournament27PointWhiteWinsTie,
};

enum class MutualImpasseVerdict : std::uint8_t {
    Replay,
    BlackLoses,
    WhiteLoses,
    InvalidMaterial,
};

struct MutualImpasseResult {
    MutualImpasseVerdict verdict{MutualImpasseVerdict::InvalidMaterial};
    std::uint32_t black_points{0};
    std::uint32_t white_points{0};
};

enum class Move500ImpasseStatus : std::uint8_t {
    None,
    Replay,
};

// Computes both ordinary impasse material and the narrower entering-king
// declaration material for one side.
[[nodiscard]] ImpasseAnalysis analyze_impasse(const Position& position, Color color);

// Adjudicates the board/turn/move-count conditions of an entering-king
// declaration attempt. Failing any represented declaration condition is a loss
// by declarer. Clock validity and prior agreement state belong to the runtime or
// protocol layer because Position does not contain match-clock state.
[[nodiscard]] EnteringKingDeclarationResult adjudicate_entering_king_declaration(
    const Position& position,
    Color declarer
);

// Point calculation for a mutually agreed impasse procedure. The caller is
// responsible for establishing agreement and any non-material eligibility.
// The default is the JSA 24-point rule; optional tournament 27-point policies
// are explicit so league/benchmark configuration can reproduce event rules.
[[nodiscard]] MutualImpasseResult adjudicate_mutual_impasse_points(
    const Position& position,
    MutualImpassePolicy policy = MutualImpassePolicy::Jsa24Point
);

// Detects the automatic 500-move impasse rule from canonical position history.
// When move 500 ends in check, adjudication is deferred until that checking
// side first makes a move that does not continue the check.
[[nodiscard]] Move500ImpasseStatus adjudicate_500_move_impasse(
    const std::vector<Position>& history
);

} // namespace kadoka::shogi
