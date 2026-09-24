#include "kadoka/training/parser.hpp"

#include "kadoka/training/core_history_parser.hpp"
#include "kadoka/training/sfen_lines_parser.hpp"

#include <stdexcept>
#include <utility>

namespace kadoka::shogi::training {

void CollectingParsedRecordSink::on_record(ParsedRecord record) {
    records_.push_back(std::move(record));
}

void CollectingParsedRecordSink::on_error(ParseError error) {
    errors_.push_back(std::move(error));
}

void ParserRegistry::add(std::shared_ptr<const Parser> parser) {
    if (!parser) {
        throw std::invalid_argument("parser must not be null");
    }
    const std::string parser_id{parser->id()};
    if (parser_id.empty()) {
        throw std::invalid_argument("parser id must not be empty");
    }

    const auto [_, inserted] =
        parsers_.emplace(parser_id, std::move(parser));
    if (!inserted) {
        throw std::invalid_argument(
            "duplicate parser id: " + parser_id
        );
    }
}

const Parser* ParserRegistry::find(
    std::string_view parser_id) const noexcept {
    const auto found = parsers_.find(std::string(parser_id));
    return found == parsers_.end() ? nullptr : found->second.get();
}

std::vector<std::string> ParserRegistry::parser_ids() const {
    std::vector<std::string> ids;
    ids.reserve(parsers_.size());
    for (const auto& [id, _] : parsers_) {
        ids.push_back(id);
    }
    return ids;
}

ParserRegistry make_reference_parser_registry() {
    ParserRegistry registry;
    registry.add(std::make_shared<CoreHistoryParser>());
    registry.add(std::make_shared<SfenLinesParser>());
    return registry;
}

} // namespace kadoka::shogi::training
