#include "kadoka/position.hpp"

#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

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

PieceType piece_type_from_letter(char letter) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(letter)))) {
    case 'P': return PieceType::Pawn;
    case 'L': return PieceType::Lance;
    case 'N': return PieceType::Knight;
    case 'S': return PieceType::Silver;
    case 'G': return PieceType::Gold;
    case 'B': return PieceType::Bishop;
    case 'R': return PieceType::Rook;
    case 'K': return PieceType::King;
    default: throw std::invalid_argument("invalid SFEN piece letter");
    }
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

PieceType promote_type(PieceType type) {
    switch (type) {
    case PieceType::Pawn: return PieceType::ProPawn;
    case PieceType::Lance: return PieceType::ProLance;
    case PieceType::Knight: return PieceType::ProKnight;
    case PieceType::Silver: return PieceType::ProSilver;
    case PieceType::Bishop: return PieceType::Horse;
    case PieceType::Rook: return PieceType::Dragon;
    default: throw std::invalid_argument("piece cannot be promoted");
    }
}

PieceType unpromote_type(PieceType type) {
    switch (type) {
    case PieceType::ProPawn: return PieceType::Pawn;
    case PieceType::ProLance: return PieceType::Lance;
    case PieceType::ProKnight: return PieceType::Knight;
    case PieceType::ProSilver: return PieceType::Silver;
    case PieceType::Horse: return PieceType::Bishop;
    case PieceType::Dragon: return PieceType::Rook;
    default: return type;
    }
}

std::size_t hand_index(PieceType type) {
    switch (type) {
    case PieceType::Pawn: return 0;
    case PieceType::Lance: return 1;
    case PieceType::Knight: return 2;
    case PieceType::Silver: return 3;
    case PieceType::Gold: return 4;
    case PieceType::Bishop: return 5;
    case PieceType::Rook: return 6;
    default: throw std::invalid_argument("piece type cannot be in hand");
    }
}

void place(Board& board, std::uint8_t file, std::uint8_t rank, PieceType type, Color color) {
    board[(static_cast<std::size_t>(rank) - 1U) * 9U + (static_cast<std::size_t>(file) - 1U)] = Piece{type, color};
}

void append_hand(std::string& out, const std::array<std::uint8_t, 7>& hand, bool lowercase) {
    constexpr PieceType order[] = {
        PieceType::Rook, PieceType::Bishop, PieceType::Gold, PieceType::Silver,
        PieceType::Knight, PieceType::Lance, PieceType::Pawn,
    };
    for (PieceType type : order) {
        const auto count = hand[hand_index(type)];
        if (count == 0) continue;
        if (count > 1) out += std::to_string(count);
        char letter = sfen_piece_letter(type);
        if (lowercase) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        out += letter;
    }
}

} // namespace

Position::Position() = default;

Position Position::startpos() {
    Position position;
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

Position Position::from_sfen(std::string_view sfen) {
    std::istringstream input{std::string(sfen)};
    std::string board_field, side_field, hands_field, trailing;
    std::uint32_t ply = 0;
    if (!(input >> board_field >> side_field >> hands_field >> ply) || (input >> trailing)) {
        throw std::invalid_argument("SFEN must contain board, side, hands, and ply");
    }
    if (ply == 0) throw std::invalid_argument("SFEN ply must be at least 1");

    Position position;
    std::uint8_t rank = 1;
    int file = 9;
    bool promoted = false;
    for (char ch : board_field) {
        if (ch == '/') {
            if (file != 0 || promoted || rank >= 9) throw std::invalid_argument("invalid SFEN board rank");
            ++rank; file = 9; continue;
        }
        if (ch >= '1' && ch <= '9') {
            if (promoted) throw std::invalid_argument("promotion marker before empty squares");
            file -= ch - '0';
            if (file < 0) throw std::invalid_argument("too many files in SFEN rank");
            continue;
        }
        if (ch == '+') {
            if (promoted) throw std::invalid_argument("duplicate SFEN promotion marker");
            promoted = true; continue;
        }
        if (file < 1 || rank > 9) throw std::invalid_argument("too many pieces in SFEN board");
        PieceType type = piece_type_from_letter(ch);
        if (promoted) { type = promote_type(type); promoted = false; }
        const Color color = std::isupper(static_cast<unsigned char>(ch)) ? Color::Black : Color::White;
        place(position.board_, static_cast<std::uint8_t>(file), rank, type, color);
        --file;
    }
    if (rank != 9 || file != 0 || promoted) throw std::invalid_argument("SFEN board must contain exactly 9 ranks of 9 files");

    if (side_field == "b") position.side_to_move_ = Color::Black;
    else if (side_field == "w") position.side_to_move_ = Color::White;
    else throw std::invalid_argument("SFEN side must be b or w");

    if (hands_field != "-") {
        unsigned count = 0;
        for (char ch : hands_field) {
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                count = count * 10U + static_cast<unsigned>(ch - '0');
                if (count > std::numeric_limits<std::uint8_t>::max()) throw std::invalid_argument("SFEN hand count is too large");
                continue;
            }
            const PieceType type = piece_type_from_letter(ch);
            if (type == PieceType::King) throw std::invalid_argument("king cannot be in hand");
            const unsigned actual_count = count == 0 ? 1U : count;
            auto& hand = std::isupper(static_cast<unsigned char>(ch)) ? position.hands_.black : position.hands_.white;
            auto& slot = hand[hand_index(type)];
            if (static_cast<unsigned>(slot) + actual_count > std::numeric_limits<std::uint8_t>::max()) throw std::invalid_argument("SFEN hand count is too large");
            slot = static_cast<std::uint8_t>(static_cast<unsigned>(slot) + actual_count);
            count = 0;
        }
        if (count != 0) throw std::invalid_argument("SFEN hand count must precede a piece");
    }
    position.ply_ = ply;
    return position;
}

std::size_t Position::index_of(Square square) {
    if (!square.valid()) throw std::out_of_range("square must be inside the 9x9 board");
    return (static_cast<std::size_t>(square.rank) - 1U) * 9U + (static_cast<std::size_t>(square.file) - 1U);
}

const Piece& Position::at(Square square) const { return board_.at(index_of(square)); }

std::uint8_t Position::hand_count(Color color, PieceType type) const {
    const auto& hand = color == Color::Black ? hands_.black : hands_.white;
    return hand.at(hand_index(type));
}

Position Position::after_move(const Move& move) const {
    if (!move.to.valid()) throw std::invalid_argument("move destination is outside board");
    Position next = *this;
    const Color mover = side_to_move_;
    Piece moving;

    if (move.is_drop()) {
        if (move.promote || move.drop_piece == PieceType::None || move.drop_piece == PieceType::King || is_promoted(move.drop_piece)) {
            throw std::invalid_argument("invalid drop move");
        }
        if (!next.at(move.to).empty()) throw std::invalid_argument("drop destination is occupied");
        auto& hand = mover == Color::Black ? next.hands_.black : next.hands_.white;
        auto& count = hand.at(hand_index(move.drop_piece));
        if (count == 0) throw std::invalid_argument("dropped piece is not in hand");
        --count;
        moving = Piece{move.drop_piece, mover};
    } else {
        if (!move.from->valid()) throw std::invalid_argument("move origin is outside board");
        moving = next.at(*move.from);
        if (moving.empty() || moving.color != mover) throw std::invalid_argument("move origin does not contain mover piece");
        const Piece captured = next.at(move.to);
        if (!captured.empty()) {
            if (captured.color == mover || captured.type == PieceType::King) throw std::invalid_argument("invalid capture");
            const PieceType hand_type = unpromote_type(captured.type);
            auto& hand = mover == Color::Black ? next.hands_.black : next.hands_.white;
            ++hand.at(hand_index(hand_type));
        }
        next.board_.at(index_of(*move.from)) = Piece{};
        if (move.promote) moving.type = promote_type(moving.type);
    }

    next.board_.at(index_of(move.to)) = moving;
    next.side_to_move_ = opposite(mover);
    ++next.ply_;
    return next;
}

std::string Position::to_sfen() const {
    std::string out;
    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        unsigned empty = 0;
        for (int file = 9; file >= 1; --file) {
            const Piece& piece = at(Square{static_cast<std::uint8_t>(file), rank});
            if (piece.empty()) { ++empty; continue; }
            if (empty != 0) { out += static_cast<char>('0' + empty); empty = 0; }
            if (is_promoted(piece.type)) out += '+';
            char letter = sfen_piece_letter(piece.type);
            if (piece.color == Color::White) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            out += letter;
        }
        if (empty != 0) out += static_cast<char>('0' + empty);
        if (rank != 9) out += '/';
    }
    out += side_to_move_ == Color::Black ? " b " : " w ";
    const std::size_t hand_start = out.size();
    append_hand(out, hands_.black, false);
    append_hand(out, hands_.white, true);
    if (out.size() == hand_start) out += '-';
    out += ' ';
    out += std::to_string(ply_);
    return out;
}

} // namespace kadoka::shogi
