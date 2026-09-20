#pragma once

#include "kadoka/ai_package/observation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kadoka::shogi::ai_package {

enum class ObservationPixelFormat : std::uint8_t {
    Gray8,
    Rgb8,
    Rgba8,
    Bgra8,
};

struct ObservationFrame {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::size_t row_stride_bytes{0};
    ObservationPixelFormat pixel_format{ObservationPixelFormat::Rgba8};
    std::vector<std::uint8_t> pixels{};
};

enum class ObservationCaptureFailure : std::uint8_t {
    None,
    Unavailable,
    PermissionDenied,
    EndOfStream,
    InvalidFrame,
    Other,
};

struct ObservationCaptureResult {
    std::optional<ObservationFrame> frame{};
    ObservationCaptureFailure failure{ObservationCaptureFailure::None};
    std::string message{};

    [[nodiscard]] bool succeeded() const noexcept {
        return frame.has_value()
            && failure == ObservationCaptureFailure::None;
    }
};

class ObservationSource {
public:
    virtual ~ObservationSource() = default;

    [[nodiscard]] virtual ObservationCaptureResult capture() = 0;
};

struct RecognitionConfidence {
    double overall{0.0};
    std::optional<double> board{};
    std::optional<double> hands{};
    std::optional<double> side_to_move{};
    std::optional<double> clock{};
};

enum class BoardRecognitionFailure : std::uint8_t {
    None,
    BoardNotFound,
    AmbiguousBoard,
    MissingSideToMove,
    InvalidState,
    UnsupportedFrame,
    Other,
};

struct BoardRecognitionResult {
    std::optional<ObservedGameState> state{};
    RecognitionConfidence confidence{};
    BoardRecognitionFailure failure{BoardRecognitionFailure::None};
    std::string message{};

    [[nodiscard]] bool succeeded() const noexcept {
        return state.has_value()
            && failure == BoardRecognitionFailure::None;
    }
};

class BoardRecognitionAdapter {
public:
    virtual ~BoardRecognitionAdapter() = default;

    [[nodiscard]] virtual BoardRecognitionResult recognize(
        const ObservationFrame& frame
    ) = 0;
};

struct RecognitionAcceptancePolicy {
    // Kept caller-configurable because a UI helper and an unattended engine
    // may choose different confidence requirements.
    double minimum_overall_confidence{0.0};

    // A component threshold means that component confidence is required and
    // must meet the configured value. nullopt means the caller does not
    // require a confidence score for that component.
    std::optional<double> minimum_board_confidence{};
    std::optional<double> minimum_hands_confidence{};
    std::optional<double> minimum_side_to_move_confidence{};
    std::optional<double> minimum_clock_confidence{};
};

enum class ObservationPipelineStatus : std::uint8_t {
    Converted,
    CaptureFailed,
    RecognitionFailed,
    LowConfidence,
    ConversionFailed,
};

struct ObservationPipelineResult {
    ObservationPipelineStatus status{
        ObservationPipelineStatus::CaptureFailed
    };
    std::optional<ObservedGameState> state{};
    RecognitionConfidence confidence{};
    std::string message{};

    [[nodiscard]] bool converted() const noexcept {
        return status == ObservationPipelineStatus::Converted;
    }
};

[[nodiscard]] std::size_t bytes_per_pixel(
    ObservationPixelFormat format
) noexcept;

[[nodiscard]] bool is_valid_observation_frame(
    const ObservationFrame& frame
) noexcept;

[[nodiscard]] bool is_valid_recognition_confidence(
    const RecognitionConfidence& confidence
) noexcept;

[[nodiscard]] ObservationPipelineResult observe_into(
    ObservationSource& source,
    BoardRecognitionAdapter& recognition,
    InternalBoardConverter& converter,
    const RecognitionAcceptancePolicy& policy = {}
);

} // namespace kadoka::shogi::ai_package
