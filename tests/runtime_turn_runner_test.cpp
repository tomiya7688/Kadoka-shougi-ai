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

SearchResult fixed_result(Move move, const char* info) {
    SearchResult result;
    result.best_move = move;
    result.info = info;
    return result;
}

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
        Position position = Position::startpos();
        unsigned calls = 0;
        FunctionAIBackend script_backend{
            AIBackendKind::Script,
            "script-test",
            [&](const Position&, const SearchLimits&) {
                ++calls;
                return fixed_result(
                    Move{Square{7, 7}, Square{7, 6}, PieceType::None, false},
                    "script-adapter"
                );
            },
        };

        assert(script_backend.kind() == AIBackendKind::Script);
        assert(std::string{ai_backend_kind_name(script_backend.kind())} == "script");

        const TurnResult result = run_ai_turn(script_backend, position);
        assert(result.status == TurnStatus::MoveApplied);
        assert(result.next_position.has_value());
        assert(calls == 1);
    }

    {
        Position position = Position::startpos();
        const std::string before = position.to_sfen();
        FunctionAIBackend process_backend{
            AIBackendKind::ExternalProcess,
            "process-test",
            [&](const Position&, const SearchLimits&) {
                return fixed_result(
                    Move{Square{7, 7}, Square{7, 5}, PieceType::None, false},
                    "process-adapter"
                );
            },
        };

        const TurnResult result = run_ai_turn(process_backend, position);
        assert(result.status == TurnStatus::IllegalMove);
        assert(!result.next_position.has_value());
        assert(position.to_sfen() == before);
    }

    {
        const Position empty = Position::from_sfen("9/9/9/9/9/9/9/9/9 b - 1");
        unsigned calls = 0;
        FunctionAIBackend backend{
            AIBackendKind::Network,
            "must-not-be-called",
            [&](const Position&, const SearchLimits&) {
                ++calls;
                return SearchResult{};
            },
        };

        const TurnResult result = run_ai_turn(backend, empty);
        assert(result.status == TurnStatus::NoLegalMoves);
        assert(!result.search_result.has_value());
        assert(!result.next_position.has_value());
        assert(calls == 0);
    }

    return 0;
}
