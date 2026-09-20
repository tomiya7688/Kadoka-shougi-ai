#include "kadoka/ai_package/board_recognition.hpp"
#include "kadoka/ai_package/json_player_observation_parser.hpp"

#include "kadoka/runtime/player_api.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;
using namespace kadoka::shogi::ai_package;

namespace {

ObservationFrame sample_frame() {
    ObservationFrame frame;
    frame.width = 2;
    frame.height = 2;
    frame.row_stride_bytes = 8;
    frame.pixel_format = ObservationPixelFormat::Rgba8;
    frame.pixels.assign(16, 0x7f);
    return frame;
}

ObservedGameState sample_state() {
    PlayerClock clock;
    clock.black.main_ms = 60000;
    clock.black.byoyomi_ms = 30000;
    clock.black.increment_ms = 0;
    clock.white.main_ms = 59000;
    clock.white.byoyomi_ms = 30000;
    clock.white.increment_ms = 0;
    clock.per_move_limit_ms = 5000;

    const Position position = Position::from_sfen(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/"
        "1B5R1/LNSGKGSNL b Pp 1"
    );

    return ObservedGameState{
        position.to_sfen(),
        Color::Black,
        clock,
    };
}

RecognitionConfidence high_confidence() {
    return RecognitionConfidence{
        0.98,
        0.99,
        0.97,
        0.96,
        0.92,
    };
}

class FixedSource final : public ObservationSource {
public:
    explicit FixedSource(ObservationCaptureResult result)
        : result_(std::move(result)) {}

    ObservationCaptureResult capture() override {
        ++calls;
        return result_;
    }

    int calls{0};

private:
    ObservationCaptureResult result_;
};

class ThrowingSource final : public ObservationSource {
public:
    ObservationCaptureResult capture() override {
        throw std::runtime_error("capture failed hard");
    }
};

class FixedRecognition final : public BoardRecognitionAdapter {
public:
    explicit FixedRecognition(BoardRecognitionResult result)
        : result_(std::move(result)) {}

    BoardRecognitionResult recognize(
        const ObservationFrame& frame
    ) override {
        ++calls;
        last_width = frame.width;
        return result_;
    }

    int calls{0};
    std::uint32_t last_width{0};

private:
    BoardRecognitionResult result_;
};

class ThrowingRecognition final : public BoardRecognitionAdapter {
public:
    BoardRecognitionResult recognize(
        const ObservationFrame&
    ) override {
        throw std::runtime_error("model crashed");
    }
};

class CapturingConverter final : public InternalBoardConverter {
public:
    void assign_observed_state(
        const ObservedGameState& state
    ) override {
        ++calls;
        captured = state;
    }

    int calls{0};
    ObservedGameState captured{};
};

class ThrowingConverter final : public InternalBoardConverter {
public:
    void assign_observed_state(
        const ObservedGameState&
    ) override {
        ++calls;
        throw std::runtime_error("conversion failed");
    }

    int calls{0};
};

ObservationCaptureResult successful_capture() {
    return ObservationCaptureResult{
        sample_frame(),
        ObservationCaptureFailure::None,
        {},
    };
}

BoardRecognitionResult successful_recognition() {
    return BoardRecognitionResult{
        sample_state(),
        high_confidence(),
        BoardRecognitionFailure::None,
        {},
    };
}

template <class Function>
void assert_invalid_argument(Function&& function) {
    bool failed = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);
}

} // namespace

int main() {
    {
        assert(bytes_per_pixel(ObservationPixelFormat::Gray8) == 1);
        assert(bytes_per_pixel(ObservationPixelFormat::Rgb8) == 3);
        assert(bytes_per_pixel(ObservationPixelFormat::Rgba8) == 4);
        assert(bytes_per_pixel(ObservationPixelFormat::Bgra8) == 4);

        ObservationFrame frame = sample_frame();
        assert(is_valid_observation_frame(frame));

        frame.row_stride_bytes = 7;
        assert(!is_valid_observation_frame(frame));

        frame = sample_frame();
        frame.pixels.resize(15);
        assert(!is_valid_observation_frame(frame));

        frame = sample_frame();
        frame.width = 0;
        assert(!is_valid_observation_frame(frame));

        frame = sample_frame();
        frame.height = 0;
        assert(!is_valid_observation_frame(frame));

        ObservationFrame rgb;
        rgb.width = 3;
        rgb.height = 2;
        rgb.row_stride_bytes = 12; // padded row
        rgb.pixel_format = ObservationPixelFormat::Rgb8;
        rgb.pixels.assign(24, 0);
        assert(is_valid_observation_frame(rgb));
    }

    {
        const RecognitionConfidence confidence = high_confidence();
        assert(is_valid_recognition_confidence(confidence));

        RecognitionConfidence invalid = confidence;
        invalid.overall = 1.1;
        assert(!is_valid_recognition_confidence(invalid));

        invalid = confidence;
        invalid.board = -0.1;
        assert(!is_valid_recognition_confidence(invalid));
    }

    {
        FixedSource source{successful_capture()};
        FixedRecognition recognition{successful_recognition()};
        ReferenceInternalBoard internal;

        RecognitionAcceptancePolicy policy;
        policy.minimum_overall_confidence = 0.90;
        policy.minimum_board_confidence = 0.95;
        policy.minimum_hands_confidence = 0.90;
        policy.minimum_side_to_move_confidence = 0.90;
        policy.minimum_clock_confidence = 0.90;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            internal,
            policy
        );

        assert(result.converted());
        assert(
            result.status
            == ObservationPipelineStatus::Converted
        );
        assert(result.state.has_value());
        assert(result.confidence.overall == 0.98);
        assert(source.calls == 1);
        assert(recognition.calls == 1);
        assert(recognition.last_width == 2);
        assert(internal.initialized());
        assert(
            internal.position().to_sfen()
            == sample_state().sfen
        );
        assert(
            internal.position().hand_count(
                Color::Black,
                PieceType::Pawn
            ) == 1
        );
        assert(
            internal.position().hand_count(
                Color::White,
                PieceType::Pawn
            ) == 1
        );
        assert(internal.clock().black.main_ms == 60000);
        assert(internal.clock().per_move_limit_ms == 5000);
    }

    {
        // API and screen-recognition paths must converge on the same
        // normalized state and therefore the same internal board.
        const ObservedGameState state = sample_state();
        const Position position = Position::from_sfen(state.sfen);
        const PlayerObservation observation =
            make_player_observation(position, state.clock);

        const JsonPlayerObservationParser json_parser;
        const ReferenceInternalBoard api_board =
            json_parser.parse_reference(
                serialize_player_observation(observation)
            );

        FixedSource source{successful_capture()};
        FixedRecognition recognition{successful_recognition()};
        ReferenceInternalBoard screen_board;
        const ObservationPipelineResult screen_result =
            observe_into(source, recognition, screen_board);

        assert(screen_result.converted());
        assert(
            screen_board.position().to_sfen()
            == api_board.position().to_sfen()
        );
        assert(
            screen_board.position().side_to_move()
            == api_board.position().side_to_move()
        );
        assert(
            screen_board.clock().black.main_ms
            == api_board.clock().black.main_ms
        );
    }

    {
        FixedSource source{ObservationCaptureResult{
            std::nullopt,
            ObservationCaptureFailure::PermissionDenied,
            "screen capture permission denied",
        }};
        FixedRecognition recognition{successful_recognition()};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::CaptureFailed
        );
        assert(!result.converted());
        assert(
            result.capture_failure
            == ObservationCaptureFailure::PermissionDenied
        );
        assert(recognition.calls == 0);
        assert(converter.calls == 0);
        assert(
            result.message == "screen capture permission denied"
        );
    }

    {
        ObservationFrame invalid = sample_frame();
        invalid.pixels.clear();

        FixedSource source{ObservationCaptureResult{
            invalid,
            ObservationCaptureFailure::None,
            {},
        }};
        FixedRecognition recognition{successful_recognition()};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::CaptureFailed
        );
        assert(
            result.capture_failure
            == ObservationCaptureFailure::InvalidFrame
        );
        assert(recognition.calls == 0);
        assert(converter.calls == 0);
    }

    {
        ThrowingSource source;
        FixedRecognition recognition{successful_recognition()};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::CaptureFailed
        );
        assert(
            result.capture_failure
            == ObservationCaptureFailure::Other
        );
        assert(recognition.calls == 0);
        assert(converter.calls == 0);
    }

    {
        FixedSource source{successful_capture()};
        FixedRecognition recognition{BoardRecognitionResult{
            std::nullopt,
            RecognitionConfidence{},
            BoardRecognitionFailure::BoardNotFound,
            "board not found",
        }};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::RecognitionFailed
        );
        assert(
            result.recognition_failure
            == BoardRecognitionFailure::BoardNotFound
        );
        assert(result.message == "board not found");
        assert(converter.calls == 0);
    }

    {
        FixedSource source{successful_capture()};
        ThrowingRecognition recognition;
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::RecognitionFailed
        );
        assert(
            result.recognition_failure
            == BoardRecognitionFailure::Other
        );
        assert(converter.calls == 0);
    }

    {
        BoardRecognitionResult invalid = successful_recognition();
        invalid.confidence.overall = 1.2;

        FixedSource source{successful_capture()};
        FixedRecognition recognition{invalid};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::RecognitionFailed
        );
        assert(
            result.recognition_failure
            == BoardRecognitionFailure::InvalidState
        );
        assert(converter.calls == 0);
    }

    {
        BoardRecognitionResult invalid = successful_recognition();
        invalid.state->side_to_move = Color::White;

        FixedSource source{successful_capture()};
        FixedRecognition recognition{invalid};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::RecognitionFailed
        );
        assert(converter.calls == 0);
    }

    {
        BoardRecognitionResult invalid = successful_recognition();
        invalid.state->clock.black.main_ms = -1;

        FixedSource source{successful_capture()};
        FixedRecognition recognition{invalid};
        CapturingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::RecognitionFailed
        );
        assert(converter.calls == 0);
    }

    {
        BoardRecognitionResult low = successful_recognition();
        low.confidence.overall = 0.70;

        FixedSource source{successful_capture()};
        FixedRecognition recognition{low};
        CapturingConverter converter;
        RecognitionAcceptancePolicy policy;
        policy.minimum_overall_confidence = 0.80;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter,
            policy
        );
        assert(
            result.status
            == ObservationPipelineStatus::LowConfidence
        );
        assert(result.state.has_value());
        assert(converter.calls == 0);
    }

    {
        BoardRecognitionResult missing = successful_recognition();
        missing.confidence.clock = std::nullopt;

        FixedSource source{successful_capture()};
        FixedRecognition recognition{missing};
        CapturingConverter converter;
        RecognitionAcceptancePolicy policy;
        policy.minimum_clock_confidence = 0.50;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter,
            policy
        );
        assert(
            result.status
            == ObservationPipelineStatus::LowConfidence
        );
        assert(converter.calls == 0);
    }

    {
        BoardRecognitionResult low_hands = successful_recognition();
        low_hands.confidence.hands = 0.40;

        FixedSource source{successful_capture()};
        FixedRecognition recognition{low_hands};
        CapturingConverter converter;
        RecognitionAcceptancePolicy policy;
        policy.minimum_hands_confidence = 0.80;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter,
            policy
        );
        assert(
            result.status
            == ObservationPipelineStatus::LowConfidence
        );
        assert(converter.calls == 0);
    }

    {
        FixedSource source{successful_capture()};
        FixedRecognition recognition{successful_recognition()};
        ThrowingConverter converter;

        const ObservationPipelineResult result = observe_into(
            source,
            recognition,
            converter
        );
        assert(
            result.status
            == ObservationPipelineStatus::ConversionFailed
        );
        assert(result.state.has_value());
        assert(converter.calls == 1);
    }

    {
        FixedSource source{successful_capture()};
        FixedRecognition recognition{successful_recognition()};
        CapturingConverter converter;
        RecognitionAcceptancePolicy policy;
        policy.minimum_overall_confidence = 1.1;

        assert_invalid_argument([&] {
            (void)observe_into(
                source,
                recognition,
                converter,
                policy
            );
        });
        assert(source.calls == 0);
        assert(recognition.calls == 0);
        assert(converter.calls == 0);
    }

    {
        const ObservedGameState state = sample_state();
        validate_observed_game_state(state);

        ObservedGameState invalid = state;
        invalid.clock.white.byoyomi_ms = -1;
        assert_invalid_argument([&] {
            validate_observed_game_state(invalid);
        });
    }

    return 0;
}
