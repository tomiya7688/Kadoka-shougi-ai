#pragma once

#include "kadoka/runtime/game_outcome.hpp"
#include "kadoka/runtime/player_api.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::runtime {

inline constexpr std::uint32_t kGameHistorySchemaVersion = 1;

struct HistoryClock {
    std::optional<std::int64_t> black_main_ms{};
    std::optional<std::int64_t> white_main_ms{};
    std::optional<std::int64_t> black_byoyomi_ms{};
    std::optional<std::int64_t> white_byoyomi_ms{};
    std::optional<std::int64_t> black_increment_ms{};
    std::optional<std::int64_t> white_increment_ms{};
    std::optional<std::int64_t> per_move_limit_ms{};
};

struct BoardStateRecord {
    std::string game_id{};
    std::uint64_t ply{0};
    std::string sfen{};
    Color side_to_move{Color::Black};
    HistoryClock clock{};
};

enum class GameAuxEventType : std::uint8_t {
    Action,
    Terminal,
};

struct GameAuxRecord {
    std::string game_id{};
    std::uint64_t ply{0};
    std::uint64_t event_index{0};
    GameAuxEventType event_type{GameAuxEventType::Action};
    std::optional<Color> actor{};
    std::optional<PlayerAction> action{};
    std::optional<ActionResult> result{};
    std::optional<GameOutcome> terminal{};
};

[[nodiscard]] std::string make_ulid(
    std::uint64_t timestamp_ms,
    const std::array<std::uint8_t, 10>& entropy
);
[[nodiscard]] std::string generate_ulid();
[[nodiscard]] bool is_valid_ulid(std::string_view value) noexcept;

[[nodiscard]] BoardStateRecord make_board_state_record(
    std::string game_id,
    std::uint64_t ply,
    const Position& position,
    HistoryClock clock = {}
);

[[nodiscard]] std::string serialize_board_state_record(
    const BoardStateRecord& record
);
[[nodiscard]] BoardStateRecord deserialize_board_state_record(
    std::string_view json
);

[[nodiscard]] std::string serialize_game_aux_record(
    const GameAuxRecord& record
);
[[nodiscard]] GameAuxRecord deserialize_game_aux_record(
    std::string_view json
);

[[nodiscard]] std::string serialize_board_state_jsonl(
    std::span<const BoardStateRecord> records
);
[[nodiscard]] std::vector<BoardStateRecord> deserialize_board_state_jsonl(
    std::string_view jsonl
);

[[nodiscard]] std::string serialize_game_aux_jsonl(
    std::span<const GameAuxRecord> records
);
[[nodiscard]] std::vector<GameAuxRecord> deserialize_game_aux_jsonl(
    std::string_view jsonl
);

class GameHistoryRecorder {
public:
    explicit GameHistoryRecorder(
        Position initial_position,
        HistoryClock initial_clock = {},
        std::string game_id = {}
    );

    [[nodiscard]] const std::string& game_id() const noexcept {
        return game_id_;
    }
    [[nodiscard]] std::uint64_t ply() const noexcept { return ply_; }
    [[nodiscard]] const Position& position() const noexcept {
        return position_;
    }
    [[nodiscard]] const std::vector<BoardStateRecord>& board_states()
        const noexcept {
        return board_states_;
    }
    [[nodiscard]] const std::vector<GameAuxRecord>& events()
        const noexcept {
        return events_;
    }

    // Applies the normal public Player API action through the authoritative
    // native bridge and records the resulting action event. Accepted moves
    // append exactly one new BoardState; illegal actions leave ply unchanged.
    [[nodiscard]] ActionResult apply_and_record(
        Color actor,
        const PlayerAction& action,
        HistoryClock clock_after = {}
    );

    void record_terminal(const GameOutcome& outcome);

private:
    std::string game_id_{};
    Position position_{};
    std::uint64_t ply_{0};
    std::uint64_t next_event_index_{0};
    bool terminal_recorded_{false};
    bool terminal_pending_{false};
    std::vector<BoardStateRecord> board_states_{};
    std::vector<GameAuxRecord> events_{};
};

} // namespace kadoka::shogi::runtime
