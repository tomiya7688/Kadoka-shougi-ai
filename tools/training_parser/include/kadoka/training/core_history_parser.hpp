#pragma once

#include "kadoka/training/parser.hpp"

namespace kadoka::shogi::training {

inline constexpr std::string_view kCoreHistoryParserId =
    "kadoka.core_history.v1";

class CoreHistoryParser final : public Parser {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return kCoreHistoryParserId;
    }

    [[nodiscard]] std::string_view display_name() const noexcept override {
        return "Kadoka Core BoardState/GameAux JSONL v1";
    }

    [[nodiscard]] ParseSummary parse(
        std::span<const ParserSource> sources,
        ParsedRecordSink& sink
    ) const override;
};

} // namespace kadoka::shogi::training
