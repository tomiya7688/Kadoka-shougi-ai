#include "kadoka/ai_package/board_recognition.hpp"

#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace kadoka::shogi::ai_package {
namespace {

bool valid_confidence(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool valid_optional_confidence(
    const std::optional<double>& value) noexcept {
    return !value.has_value() || valid_confidence(*value);
}

void validate_threshold(
    const std::optional<double>& value,
    const char* name) {
    if (value.has_value() && !valid_confidence(*value)) {
        throw std::invalid_argument(
            std::string(name) + " must be within [0, 1]"
        );
    }
}

void validate_policy(const RecognitionAcceptancePolicy& policy) {
    if (!valid_confidence(policy.minimum_overall_confidence)) {
        throw std::invalid_argument(
            "minimum_overall_confidence must be within [0, 1]"
        );
    }
    validate_threshold(
        policy.minimum_board_confidence,
        "minimum_board_confidence"
    );
    validate_threshold(
        policy.minimum_hands_confidence,
        "minimum_hands_confidence"
    );
    validate_threshold(
        policy.minimum_side_to_move_confidence,
        "minimum_side_to_move_confidence"
    );
    validate_threshold(
        policy.minimum_clock_confidence,
        "minimum_clock_confidence"
    );
}

bool meets_component_threshold(
    const std::optional<double>& confidence,
    const std::optional<double>& threshold) noexcept {
    if (!threshold.has_value()) return true;
    return confidence.has_value() && *confidence >= *threshold;
}

std::string low_confidence_message(
    const RecognitionConfidence& confidence,
    const RecognitionAcceptancePolicy& policy) {
    if (confidence.overall < policy.minimum_overall_confidence) {
        return "overall recognition confidence below threshold";
    }
    if (!meets_component_threshold(
            confidence.board,
            policy.minimum_board_confidence)) {
        return "board recognition confidence below threshold or missing";
    }
    if (!meets_component_threshold(
            confidence.hands,
            policy.minimum_hands_confidence)) {
        return "hands recognition confidence below threshold or missing";
    }
    if (!meets_component_threshold(
            confidence.side_to_move,
            policy.minimum_side_to_move_confidence)) {
        return "side-to-move recognition confidence below threshold or missing";
    }
    if (!meets_component_threshold(
            confidence.clock,
            policy.minimum_clock_confidence)) {
        return "clock recognition confidence below threshold or missing";
    }
    return {};
}

ObservationPipelineResult pipeline_failure(
    ObservationPipelineStatus status,
    std::string message,
    ObservationCaptureFailure capture_failure =
        ObservationCaptureFailure::None,
    BoardRecognitionFailure recognition_failure =
        BoardRecognitionFailure::None,
    RecognitionConfidence confidence = {},
    std::optional<ObservedGameState> state = std::nullopt) {
    return ObservationPipelineResult{
        status,
        capture_failure,
        recognition_failure,
        std::move(state),
        std::move(confidence),
        std::move(message),
    };
}

} // namespace

std::size_t bytes_per_pixel(
    ObservationPixelFormat format) noexcept {
    switch (format) {
    case ObservationPixelFormat::Gray8:
        return 1;
    case ObservationPixelFormat::Rgb8:
        return 3;
    case ObservationPixelFormat::Rgba8:
    case ObservationPixelFormat::Bgra8:
        return 4;
    }
    return 0;
}

bool is_valid_observation_frame(
    const ObservationFrame& frame) noexcept {
    if (frame.width == 0 || frame.height == 0) {
        return false;
    }

    const std::size_t bpp = bytes_per_pixel(frame.pixel_format);
    if (bpp == 0) return false;

    const std::size_t width =
        static_cast<std::size_t>(frame.width);
    const std::size_t height =
        static_cast<std::size_t>(frame.height);

    if (width > std::numeric_limits<std::size_t>::max() / bpp) {
        return false;
    }
    const std::size_t minimum_stride = width * bpp;
    if (frame.row_stride_bytes < minimum_stride) {
        return false;
    }
    if (frame.row_stride_bytes == 0
        || height
            > std::numeric_limits<std::size_t>::max()
                / frame.row_stride_bytes) {
        return false;
    }

    const std::size_t required_bytes =
        frame.row_stride_bytes * height;
    return frame.pixels.size() >= required_bytes;
}

bool is_valid_recognition_confidence(
    const RecognitionConfidence& confidence) noexcept {
    return valid_confidence(confidence.overall)
        && valid_optional_confidence(confidence.board)
        && valid_optional_confidence(confidence.hands)
        && valid_optional_confidence(confidence.side_to_move)
        && valid_optional_confidence(confidence.clock);
}

ObservationPipelineResult observe_into(
    ObservationSource& source,
    BoardRecognitionAdapter& recognition,
    InternalBoardConverter& converter,
    const RecognitionAcceptancePolicy& policy) {
    validate_policy(policy);

    ObservationCaptureResult capture;
    try {
        capture = source.capture();
    } catch (const std::exception& error) {
        return pipeline_failure(
            ObservationPipelineStatus::CaptureFailed,
            std::string("observation capture threw: ") + error.what(),
            ObservationCaptureFailure::Other
        );
    } catch (...) {
        return pipeline_failure(
            ObservationPipelineStatus::CaptureFailed,
            "observation capture threw unknown exception",
            ObservationCaptureFailure::Other
        );
    }

    if (!capture.succeeded()) {
        return pipeline_failure(
            ObservationPipelineStatus::CaptureFailed,
            capture.message.empty()
                ? "observation capture failed"
                : capture.message,
            capture.failure
        );
    }
    if (!is_valid_observation_frame(*capture.frame)) {
        return pipeline_failure(
            ObservationPipelineStatus::CaptureFailed,
            "observation source returned invalid frame",
            ObservationCaptureFailure::InvalidFrame
        );
    }

    BoardRecognitionResult recognized;
    try {
        recognized = recognition.recognize(*capture.frame);
    } catch (const std::exception& error) {
        return pipeline_failure(
            ObservationPipelineStatus::RecognitionFailed,
            std::string("board recognition threw: ") + error.what(),
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::Other
        );
    } catch (...) {
        return pipeline_failure(
            ObservationPipelineStatus::RecognitionFailed,
            "board recognition threw unknown exception",
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::Other
        );
    }

    if (!recognized.succeeded()) {
        return pipeline_failure(
            ObservationPipelineStatus::RecognitionFailed,
            recognized.message.empty()
                ? "board recognition failed"
                : recognized.message,
            ObservationCaptureFailure::None,
            recognized.failure,
            recognized.confidence,
            recognized.state
        );
    }
    if (!is_valid_recognition_confidence(recognized.confidence)) {
        return pipeline_failure(
            ObservationPipelineStatus::RecognitionFailed,
            "board recognition returned invalid confidence",
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::InvalidState,
            recognized.confidence,
            recognized.state
        );
    }

    try {
        validate_observed_game_state(*recognized.state);
    } catch (const std::exception& error) {
        return pipeline_failure(
            ObservationPipelineStatus::RecognitionFailed,
            std::string("recognized game state is invalid: ")
                + error.what(),
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::InvalidState,
            recognized.confidence,
            recognized.state
        );
    }

    const std::string low_message =
        low_confidence_message(recognized.confidence, policy);
    if (!low_message.empty()) {
        return pipeline_failure(
            ObservationPipelineStatus::LowConfidence,
            low_message,
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::None,
            recognized.confidence,
            recognized.state
        );
    }

    try {
        converter.assign_observed_state(*recognized.state);
    } catch (const std::exception& error) {
        return pipeline_failure(
            ObservationPipelineStatus::ConversionFailed,
            std::string("internal board conversion failed: ")
                + error.what(),
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::None,
            recognized.confidence,
            recognized.state
        );
    } catch (...) {
        return pipeline_failure(
            ObservationPipelineStatus::ConversionFailed,
            "internal board conversion failed with unknown exception",
            ObservationCaptureFailure::None,
            BoardRecognitionFailure::None,
            recognized.confidence,
            recognized.state
        );
    }

    return ObservationPipelineResult{
        ObservationPipelineStatus::Converted,
        ObservationCaptureFailure::None,
        BoardRecognitionFailure::None,
        std::move(recognized.state),
        recognized.confidence,
        {},
    };
}

} // namespace kadoka::shogi::ai_package
