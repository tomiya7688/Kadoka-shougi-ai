#pragma once

#include "kadoka/training/parser.hpp"

namespace kadoka::shogi::training {

inline constexpr std::string_view kSfenLinesParserId =
    "reference.sfen_lines.v1";

class SfenLinesParser final : public Parser {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return kSfenLinesParserId;
    }

    [[nodiscard]] std::string_view display_name() const noexcept override {
        return "Reference one-SFEN-per-line v1";
    }

    [[nodiscard]] ParseSummary parse(
        std::span<const ParserSource> sources,
        ParsedRecordSink& sink
    ) const override;
};

} // namespace kadoka::shogi::training
