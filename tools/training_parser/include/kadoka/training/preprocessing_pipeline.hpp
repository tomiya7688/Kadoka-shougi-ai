#pragma once

#include "kadoka/training/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace kadoka::shogi::training {

inline constexpr std::string_view kPreprocessingConfigSchema =
    "kadoka.training_preprocessing_pipeline";
inline constexpr std::uint32_t kPreprocessingConfigVersion = 1;

inline constexpr std::string_view kExcludeInvalidPreprocessorId =
    "exclude_invalid";
inline constexpr std::string_view kNormalizeSidePreprocessorId =
    "normalize_side_to_move";
inline constexpr std::string_view kExpandFileMirrorPreprocessorId =
    "expand_file_mirror";
inline constexpr std::string_view kDeduplicatePreprocessorId =
    "deduplicate";

struct PreprocessingPipelineConfig {
    std::vector<std::string> steps{};
};

struct PreprocessingSummary {
    std::size_t input_records{0};
    std::size_t emitted_records{0};
    std::size_t invalid_records{0};
    std::size_t duplicate_records{0};
    std::size_t expanded_records{0};
    std::size_t forwarded_errors{0};
};

struct PreprocessorContext {
    std::unordered_set<std::string> seen_records{};
    PreprocessingSummary* summary{nullptr};
};

class Preprocessor {
public:
    virtual ~Preprocessor() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    virtual void apply(
        const ParsedRecord& input,
        std::vector<ParsedRecord>& output,
        PreprocessorContext& context
    ) const = 0;
};

class PreprocessorRegistry {
public:
    using Factory = std::function<std::shared_ptr<const Preprocessor>()>;

    void add(std::string id, Factory factory);
    [[nodiscard]] std::shared_ptr<const Preprocessor> create(
        std::string_view id
    ) const;

private:
    std::map<std::string, Factory> factories_{};
};

[[nodiscard]] PreprocessorRegistry make_reference_preprocessor_registry();
[[nodiscard]] PreprocessingPipelineConfig
make_reference_preprocessing_pipeline_config();

[[nodiscard]] std::string serialize_preprocessing_pipeline_config(
    const PreprocessingPipelineConfig& config
);
[[nodiscard]] PreprocessingPipelineConfig
deserialize_preprocessing_pipeline_config(std::string_view json);

class PreprocessingPipeline final : public ParsedRecordSink {
public:
    PreprocessingPipeline(
        PreprocessingPipelineConfig config,
        const PreprocessorRegistry& registry,
        ParsedRecordSink& downstream
    );

    void on_record(ParsedRecord record) override;
    void on_error(ParseError error) override;

    [[nodiscard]] const PreprocessingSummary& summary()
        const noexcept {
        return summary_;
    }

    void reset();

private:
    std::vector<std::shared_ptr<const Preprocessor>> steps_{};
    ParsedRecordSink* downstream_{nullptr};
    PreprocessorContext context_{};
    PreprocessingSummary summary_{};
};

} // namespace kadoka::shogi::training
