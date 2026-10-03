#include "kadoka/tree/pure_tree.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace kadoka::shogi::tree {
namespace {

constexpr std::string_view kHeader = "KADOKA_PURE_TREE";

void validate_identity(std::string_view value, std::string_view field) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(field) + " must not be empty");
    }
}

void validate_stats(
    std::uint64_t visits,
    std::uint64_t wins,
    std::uint64_t losses,
    std::uint64_t draws,
    double mean_value) {
    if (wins > visits || losses > visits - wins
        || draws > visits - wins - losses) {
        throw std::invalid_argument(
            "outcome counts must not exceed visit count"
        );
    }
    if (!std::isfinite(mean_value) || mean_value < -1.0 || mean_value > 1.0) {
        throw std::invalid_argument("mean_value must be finite and in [-1, 1]");
    }
}

void record_stats(
    std::uint64_t& visits,
    std::uint64_t& wins,
    std::uint64_t& losses,
    std::uint64_t& draws,
    double& mean_value,
    TreeOutcome outcome,
    double value) {
    validate_stats(visits, wins, losses, draws, mean_value);
    if (!std::isfinite(value) || value < -1.0 || value > 1.0) {
        throw std::invalid_argument("visit value must be finite and in [-1, 1]");
    }
    if (visits == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("visit count overflow");
    }
    std::uint64_t* outcome_count = nullptr;
    switch (outcome) {
    case TreeOutcome::Win: outcome_count = &wins; break;
    case TreeOutcome::Draw: outcome_count = &draws; break;
    case TreeOutcome::Loss: outcome_count = &losses; break;
    }
    if (outcome_count == nullptr
        || *outcome_count == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("outcome count overflow or invalid outcome");
    }
    const long double updated_mean = (
        static_cast<long double>(mean_value) * static_cast<long double>(visits)
        + static_cast<long double>(value)
    ) / static_cast<long double>(visits + 1);
    const double next_mean = static_cast<double>(updated_mean);
    if (!std::isfinite(next_mean)) {
        throw std::overflow_error("mean value update overflow");
    }
    ++visits;
    ++*outcome_count;
    mean_value = next_mean;
}

std::string escape_field(std::string_view value) {
    std::string result;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '\t': result += "\\t"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        default:
            if (ch < 0x20U || ch == 0x7fU) {
                constexpr char digits[] = "0123456789abcdef";
                result += "\\x";
                result += digits[ch >> 4];
                result += digits[ch & 0x0fU];
            } else {
                result += static_cast<char>(ch);
            }
        }
    }
    return result;
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

std::string unescape_field(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\') {
            result += value[index];
            continue;
        }
        if (++index >= value.size()) {
            throw std::invalid_argument("truncated field escape");
        }
        switch (value[index]) {
        case '\\': result += '\\'; break;
        case 't': result += '\t'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 'x': {
            if (index + 2 >= value.size()) {
                throw std::invalid_argument("truncated hexadecimal escape");
            }
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if (high < 0 || low < 0) {
                throw std::invalid_argument("invalid hexadecimal escape");
            }
            result += static_cast<char>((high << 4) | low);
            index += 2;
            break;
        }
        default:
            throw std::invalid_argument("unknown field escape");
        }
    }
    return result;
}

std::vector<std::string_view> split_fields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t tab = line.find('\t', start);
        const std::size_t end = tab == std::string_view::npos
            ? line.size()
            : tab;
        fields.push_back(line.substr(start, end - start));
        if (tab == std::string_view::npos) break;
        start = tab + 1;
    }
    return fields;
}

template <typename Integer>
Integer parse_integer(std::string_view value, std::string_view field) {
    Integer parsed{};
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("invalid integer field: " + std::string(field));
    }
    return parsed;
}

double parse_double(std::string_view value) {
    double parsed = 0.0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed,
        std::chars_format::general
    );
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("invalid mean_value field");
    }
    return parsed;
}

std::string format_double(double value) {
    std::array<char, 64> buffer{};
    const auto [end, error] = std::to_chars(
        buffer.data(),
        buffer.data() + buffer.size(),
        value,
        std::chars_format::general,
        std::numeric_limits<double>::max_digits10
    );
    if (error != std::errc{}) {
        throw std::runtime_error("failed to format mean_value");
    }
    return std::string(buffer.data(), end);
}

std::string identity_key(
    std::string_view position,
    std::string_view path) {
    std::string key;
    key.reserve(position.size() + path.size() + 1);
    key.append(position);
    key += '\0';
    key.append(path);
    return key;
}

} // namespace

PureTree PureTree::empty() {
    return {};
}

void PureTree::add_node(PureTreeNode node) {
    validate_identity(node.id, "node id");
    validate_identity(node.position_identity, "position identity");
    validate_identity(node.path_identity, "path identity");
    validate_stats(
        node.visits,
        node.wins,
        node.losses,
        node.draws,
        node.mean_value
    );
    if (std::any_of(nodes_.begin(), nodes_.end(), [&node](const auto& existing) {
            return existing.id == node.id;
        })) {
        throw std::invalid_argument("duplicate node id");
    }
    const std::string key = identity_key(
        node.position_identity,
        node.path_identity
    );
    if (std::any_of(nodes_.begin(), nodes_.end(), [&key](const auto& existing) {
            return identity_key(existing.position_identity, existing.path_identity)
                == key;
        })) {
        throw std::invalid_argument("duplicate position/path identity");
    }
    nodes_.push_back(std::move(node));
}

void PureTree::set_root(std::string_view node_id) {
    const auto node = std::find_if(nodes_.begin(), nodes_.end(), [node_id](const auto& item) {
        return item.id == node_id;
    });
    if (node == nodes_.end()) {
        throw std::invalid_argument("root node id does not exist");
    }
    root_node_id_ = node->id;
}

void PureTree::add_edge(PureTreeEdge edge) {
    validate_identity(edge.id, "edge id");
    validate_identity(edge.parent_node_id, "edge parent node id");
    validate_identity(edge.child_node_id, "edge child node id");
    validate_identity(edge.move_usi, "edge move");
    validate_stats(
        edge.visits,
        edge.wins,
        edge.losses,
        edge.draws,
        edge.mean_value
    );
    if (edge.parent_node_id == edge.child_node_id) {
        throw std::invalid_argument("edge must connect different nodes");
    }
    const auto contains_node = [this](std::string_view id) {
        return std::any_of(nodes_.begin(), nodes_.end(), [id](const auto& node) {
            return node.id == id;
        });
    };
    if (!contains_node(edge.parent_node_id)
        || !contains_node(edge.child_node_id)) {
        throw std::invalid_argument("edge references an unknown node");
    }
    if (std::any_of(edges_.begin(), edges_.end(), [&edge](const auto& existing) {
            return existing.id == edge.id;
        })) {
        throw std::invalid_argument("duplicate edge id");
    }
    if (std::any_of(edges_.begin(), edges_.end(), [&edge](const auto& existing) {
            return existing.parent_node_id == edge.parent_node_id
                && existing.move_usi == edge.move_usi;
        })) {
        throw std::invalid_argument("duplicate move from parent node");
    }
    edges_.push_back(std::move(edge));
}

void PureTree::record_node_visit(
    std::string_view node_id,
    TreeOutcome outcome,
    double value) {
    const auto found = std::find_if(nodes_.begin(), nodes_.end(), [node_id](const auto& node) {
        return node.id == node_id;
    });
    if (found == nodes_.end()) throw std::out_of_range("unknown node id");
    record_stats(
        found->visits,
        found->wins,
        found->losses,
        found->draws,
        found->mean_value,
        outcome,
        value
    );
}

void PureTree::record_edge_visit(
    std::string_view edge_id,
    TreeOutcome outcome,
    double value) {
    const auto found = std::find_if(edges_.begin(), edges_.end(), [edge_id](const auto& edge) {
        return edge.id == edge_id;
    });
    if (found == edges_.end()) throw std::out_of_range("unknown edge id");
    record_stats(
        found->visits,
        found->wins,
        found->losses,
        found->draws,
        found->mean_value,
        outcome,
        value
    );
}

void validate_pure_tree(const PureTree& tree) {
    const auto& nodes = tree.nodes();
    const auto& edges = tree.edges();
    const auto& root = tree.root_node_id();

    if (nodes.empty()) {
        if (root.has_value() || !edges.empty()) {
            throw std::invalid_argument("empty tree cannot have a root or edges");
        }
        return;
    }
    if (!root.has_value()) {
        throw std::invalid_argument("non-empty tree must have a root node");
    }

    std::unordered_map<std::string, const PureTreeNode*> by_id;
    std::unordered_set<std::string> position_paths;
    by_id.reserve(nodes.size());
    position_paths.reserve(nodes.size());
    for (const PureTreeNode& node : nodes) {
        validate_identity(node.id, "node id");
        validate_identity(node.position_identity, "position identity");
        validate_identity(node.path_identity, "path identity");
        validate_stats(node.visits, node.wins, node.losses, node.draws, node.mean_value);
        if (!by_id.emplace(node.id, &node).second) {
            throw std::invalid_argument("duplicate node id");
        }
        if (!position_paths.insert(identity_key(
                node.position_identity,
                node.path_identity
            )).second) {
            throw std::invalid_argument("duplicate position/path identity");
        }
    }
    if (!by_id.contains(*root)) {
        throw std::invalid_argument("root node id does not exist");
    }

    std::unordered_map<std::string, std::size_t> parent_count;
    std::unordered_map<std::string, std::vector<std::string>> children;
    std::unordered_set<std::string> edge_ids;
    std::unordered_set<std::string> parent_moves;
    parent_count.reserve(nodes.size());
    edge_ids.reserve(edges.size());
    for (const PureTreeEdge& edge : edges) {
        validate_identity(edge.id, "edge id");
        validate_identity(edge.move_usi, "edge move");
        validate_stats(edge.visits, edge.wins, edge.losses, edge.draws, edge.mean_value);
        if (!edge_ids.insert(edge.id).second) {
            throw std::invalid_argument("duplicate edge id");
        }
        if (!by_id.contains(edge.parent_node_id)
            || !by_id.contains(edge.child_node_id)) {
            throw std::invalid_argument("edge references an unknown node");
        }
        if (edge.parent_node_id == edge.child_node_id) {
            throw std::invalid_argument("edge must connect different nodes");
        }
        std::string move_key = edge.parent_node_id;
        move_key += '\0';
        move_key += edge.move_usi;
        if (!parent_moves.insert(std::move(move_key)).second) {
            throw std::invalid_argument("duplicate move from parent node");
        }
        const std::size_t count = ++parent_count[edge.child_node_id];
        if (count > 1) {
            throw std::invalid_argument("node has more than one parent");
        }
        children[edge.parent_node_id].push_back(edge.child_node_id);
    }

    if (parent_count.contains(*root)) {
        throw std::invalid_argument("root node cannot have a parent");
    }
    for (const PureTreeNode& node : nodes) {
        if (node.id != *root && parent_count[node.id] != 1) {
            throw std::invalid_argument("every non-root node must have one parent");
        }
    }

    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{*root};
    visited.reserve(nodes.size());
    while (!pending.empty()) {
        std::string current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current).second) {
            throw std::invalid_argument("cycle detected in tree edges");
        }
        const auto children_it = children.find(current);
        if (children_it != children.end()) {
            for (const std::string& child : children_it->second) {
                pending.push_back(child);
            }
        }
    }
    if (visited.size() != nodes.size()) {
        throw std::invalid_argument("tree contains nodes unreachable from root");
    }
}

std::string serialize_pure_tree(const PureTree& tree) {
    validate_pure_tree(tree);
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << kHeader << '\t' << kPureTreeSchemaVersion << '\n';
    output << "ROOT\t";
    if (tree.root_node_id().has_value()) {
        output << escape_field(*tree.root_node_id());
    }
    output << '\n';

    for (const PureTreeNode& node : tree.nodes()) {
        output << "NODE\t"
               << escape_field(node.id) << '\t'
               << escape_field(node.position_identity) << '\t'
               << escape_field(node.path_identity) << '\t'
               << node.visits << '\t' << node.wins << '\t'
               << node.losses << '\t' << node.draws << '\t'
               << format_double(node.mean_value) << '\n';
    }
    for (const PureTreeEdge& edge : tree.edges()) {
        output << "EDGE\t"
               << escape_field(edge.id) << '\t'
               << escape_field(edge.parent_node_id) << '\t'
               << escape_field(edge.child_node_id) << '\t'
               << escape_field(edge.move_usi) << '\t'
               << edge.visits << '\t' << edge.wins << '\t'
               << edge.losses << '\t' << edge.draws << '\t'
               << format_double(edge.mean_value) << '\n';
    }
    return output.str();
}

PureTree deserialize_pure_tree(std::string_view data) {
    PureTree tree = PureTree::empty();
    std::size_t line_number = 0;
    std::size_t start = 0;
    bool saw_header = false;
    bool saw_root = false;
    std::optional<std::string> root_id{};
    while (start <= data.size()) {
        const std::size_t newline = data.find('\n', start);
        const std::size_t end = newline == std::string_view::npos
            ? data.size()
            : newline;
        std::string_view line = data.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        ++line_number;
        start = newline == std::string_view::npos ? data.size() + 1 : newline + 1;
        if (line.empty()) continue;

        const std::vector<std::string_view> fields = split_fields(line);
        try {
            if (!saw_header) {
                if (fields.size() != 2 || fields[0] != kHeader
                    || parse_integer<std::uint32_t>(fields[1], "version")
                        != kPureTreeSchemaVersion) {
                    throw std::invalid_argument("unsupported Pure Tree header/version");
                }
                saw_header = true;
                continue;
            }
            if (!saw_root) {
                if (fields.size() != 2 || fields[0] != "ROOT") {
                    throw std::invalid_argument("expected ROOT record");
                }
                const std::string id = unescape_field(fields[1]);
                if (!id.empty()) root_id = id;
                saw_root = true;
                continue;
            }
            if (fields[0] == "NODE") {
                if (fields.size() != 9) {
                    throw std::invalid_argument("NODE record has wrong field count");
                }
                tree.add_node(PureTreeNode{
                    unescape_field(fields[1]),
                    unescape_field(fields[2]),
                    unescape_field(fields[3]),
                    parse_integer<std::uint64_t>(fields[4], "visits"),
                    parse_integer<std::uint64_t>(fields[5], "wins"),
                    parse_integer<std::uint64_t>(fields[6], "losses"),
                    parse_integer<std::uint64_t>(fields[7], "draws"),
                    parse_double(fields[8]),
                });
            } else if (fields[0] == "EDGE") {
                if (fields.size() != 10) {
                    throw std::invalid_argument("EDGE record has wrong field count");
                }
                tree.add_edge(PureTreeEdge{
                    unescape_field(fields[1]),
                    unescape_field(fields[2]),
                    unescape_field(fields[3]),
                    unescape_field(fields[4]),
                    parse_integer<std::uint64_t>(fields[5], "visits"),
                    parse_integer<std::uint64_t>(fields[6], "wins"),
                    parse_integer<std::uint64_t>(fields[7], "losses"),
                    parse_integer<std::uint64_t>(fields[8], "draws"),
                    parse_double(fields[9]),
                });
            } else {
                throw std::invalid_argument("unknown Pure Tree record type");
            }
        } catch (const std::exception& error) {
            throw std::invalid_argument(
                "invalid Pure Tree line " + std::to_string(line_number)
                + ": " + error.what()
            );
        }
    }
    if (!saw_header || !saw_root) {
        throw std::invalid_argument("Pure Tree data is missing header or root record");
    }
    if (root_id.has_value()) tree.set_root(*root_id);
    validate_pure_tree(tree);
    return tree;
}

void write_pure_tree(std::ostream& output, const PureTree& tree) {
    const std::string data = serialize_pure_tree(tree);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output) throw std::runtime_error("failed to write Pure Tree data");
}

PureTree read_pure_tree(std::istream& input) {
    const std::string data{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
    if (input.bad()) throw std::runtime_error("failed to read Pure Tree data");
    return deserialize_pure_tree(data);
}

void save_pure_tree(const std::filesystem::path& path, const PureTree& tree) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("failed to open Pure Tree output file");
    write_pure_tree(output, tree);
}

PureTree load_pure_tree(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("failed to open Pure Tree input file");
    return read_pure_tree(input);
}

} // namespace kadoka::shogi::tree
