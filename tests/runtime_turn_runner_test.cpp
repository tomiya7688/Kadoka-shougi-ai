#include "kadoka/runtime/turn_runner.hpp"

#include <cassert>
#include <string>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

class FixedMoveEngine final : public Engine {
public:
    explicit FixedMoveEngine(Move move) : move_(move) {}

    [[nodiscard]] std::string name() const override {
        return "fixed-move-test-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits&
    ) override {
        ++calls_;
        SearchResult result;
        result.best_move = move_;
        result.info = "test";
        return result;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

private:
    Move move_{};
    unsigned calls_{0};
};

} // namespace

int main() {
    {
        Position position = Position::startpos();
        FixedMoveEngine engine{Move{Square{7, 7}, Square{7, 6}, PieceType::None, false}};

        const TurnResult result = run_engine_turn(engine, position);
        assert(result.status == TurnStatus::MoveApplied);
        assert(result.search_result.has_value());
        assert(result.next_position.has_value());
        assert(engine.calls() == 1);
        assert(position.side_to_move() == Color::Black);
        assert(result.next_position->side_to_move() == Color::White);
        assert(result.next_position->at(Square{7, 6}).type == PieceType::Pawn);
        assert(result.next_position->at(Square{7, 7}).empty());
    }

    {
        Position position = Position::startpos();
        const std::string before = position.to_sfen();
        FixedMoveEngine engine{Move{Square{7, 7}, Square{7, 5}, PieceType::None, false}};

        const TurnResult result = run_engine_turn(engine, position);
        assert(result.status == TurnStatus::IllegalMove);
        assert(result.search_result.has_value());
        assert(!result.next_position.has_value());
        assert(engine.calls() == 1);
        assert(position.to_sfen() == before);
    }

    {
        const Position empty = Position::from_sfen("9/9/9/9/9/9/9/9/9 b - 1");
        FixedMoveEngine engine{Move{Square{1, 1}, Square{1, 2}, PieceType::None, false}};

        const TurnResult result = run_engine_turn(engine, empty);
        assert(result.status == TurnStatus::NoLegalMoves);
        assert(!result.search_result.has_value());
        assert(!result.next_position.has_value());
        assert(engine.calls() == 0);
    }

    return 0;
}
