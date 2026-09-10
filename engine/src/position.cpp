#include "kadoka/position.hpp"

#include <stdexcept>

namespace kadoka::shogi {
namespace {

char sfen_piece_letter(PieceType type) {
    switch (type) {
    case PieceType::Pawn: return 'P';
    case PieceType::Lance: return 'L';
    case PieceType::Knight: return 'N';
    case PieceType::Silver: return 'S';
    case PieceType::Gold: return 'G';
    case PieceType::Bishop: return 'B';
    case PieceType::Rook: return 'R';
    case PieceType::King: return 'K';
    case PieceType::ProPawn: return 'P';
    case PieceType::ProLance: return 'L';
    case PieceType::ProKnight: return 'N';
    case PieceType::ProSilver: return 'S';
    case PieceType::Horse: return 'B';
    case PieceType::Dragon: return 'R';
    case PieceType::None: break;
    }
    throw std::logic_error("piece has no SFEN letter");
}

bool is_promoted(PieceType type) {
    switch (type) {
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
    case PieceType::Horse:
    case PieceType::Dragon:
        return true;
    default:
        return false;
    }
}

void place(Board& board, std::uint8_t file, std::uint8_t rank, PieceType type, Color color) {
    board[(static_cast<std::size_t>(rank) - 1U) * 9U + (static_cast<std::size_t>(file) - 1U)] = Piece{type, color};
}

} // namespace

Position::Position() = default;

Position Position::startpos() {
    Position position;

    // SFEN rank 1: lnsgkgsnl (White camp)
    const PieceType back_rank[] = {
        PieceType::Lance, PieceType::Knight, PieceType::Silver,
        PieceType::Gold, PieceType::King, PieceType::Gold,
        PieceType::Silver, PieceType::Knight, PieceType::Lance,
    };

    for (std::uint8_t file = 1; file <= 9; ++file) {
        place(position.board_, file, 1, back_rank[file - 1], Color::White);
        place(position.board_, file, 3, PieceType::Pawn, Color::White);
        place(position.board_, file, 7, PieceType::Pawn, Color::Black);
        place(position.board_, file, 9, back_rank[file - 1], Color::Black);
    }

    place(position.board_, 8, 2, PieceType::Rook, Color::White);
    place(position.board_, 2, 2, PieceType::Bishop, Color::White);
    place(position.board_, 8, 8, PieceType::Bishop, Color::Black);
    place(position.board_, 2, 8, PieceType::Rook, Color::Black);

    position.side_to_move_ = Color::Black;
    position.ply_ = 1;
    return position;
}

std::size_t Position::index_of(Square square) {
    if (!square.valid()) {
        throw std::out_of_range("square must be inside the 9x9 board");
    }
    return (static_cast<std::size_t>(square.rank) - 1U) * 9U
         + (static_cast<std::size_t>(square.file) - 1U);
}

const Piece& Position::at(Square square) const {
    return board_.at(index_of(square));
}

std::string Position::to_sfen() const {
    std::string out;

    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        unsigned empty = 0;
        for (int file = 9; file >= 1; --file) {
            const Piece& piece = at(Square{static_cast<std::uint8_t>(file), rank});
            if (piece.empty()) {
                ++empty;
                continue;
            }

            if (empty != 0) {
                out += static_cast<char>('0' + empty);
                empty = 0;
            }
            if (is_promoted(piece.type)) {
                out += '+';
            }
            char letter = sfen_piece_letter(piece.type);
            if (piece.color == Color::White) {
                letter = static_cast<char>(letter - 'A' + 'a');
            }
            out += letter;
        }
        if (empty != 0) {
            out += static_cast<char>('0' + empty);
        }
        if (rank != 9) {
            out += '/';
        }
    }

    out += side_to_move_ == Color::Black ? " b " : " w ";

    // Hands are empty in the bootstrap implementation. Parsing and complete
    // hand serialization land with move/drop support.
    bool hands_empty = true;
    for (auto count : hands_.black) hands_empty = hands_empty && count == 0;
    for (auto count : hands_.white) hands_empty = hands_empty && count == 0;
    out += hands_empty ? "-" : "?";
    out += ' ';
    out += std::to_string(ply_);
    return out;
}

} // namespace kadoka::shogi
