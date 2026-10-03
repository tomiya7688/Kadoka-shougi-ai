#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::tree {

inline constexpr std::uint32_t kPureTreeSchemaVersion = 1;

enum class TreeOutcome : std::uint8_t {
    Win,
    Draw,
    Loss,
};

struct PureTreeNode {
    std::string id{};
    std::string position_identity{};
    std::string path_identity{};
    std::uint64_t visits{0};
    std::uint64_t wins{0};
    std::uint64_t losses{0};
    std::uint64_t draws{0};
    double mean_value{0.0};
};

struct PureTreeEdge {
    std::string id{};
    std::string parent_node_id{};
    std::string child_node_id{};
    std::string move_usi{};
    std::uint64_t visits{0};
    std::uint64_t wins{0};
    std::uint64_t losses{0};
    std::uint64_t draws{0};
    double mean_value{0.0};
};

class PureTree {
public:
    [[nodiscard]] static PureTree empty();

    void add_node(PureTreeNode node);
    void set_root(std::string_view node_id);
    void add_edge(PureTreeEdge edge);
    void record_node_visit(
        std::string_view node_id,
        TreeOutcome outcome,
        double value
    );
    void record_edge_visit(
        std::string_view edge_id,
        TreeOutcome outcome,
        double value
    );

    [[nodiscard]] const std::optional<std::string>& root_node_id()
        const noexcept { return root_node_id_; }
    [[nodiscard]] const std::vector<PureTreeNode>& nodes()
        const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<PureTreeEdge>& edges()
        const noexcept { return edges_; }

private:
    std::optional<std::string> root_node_id_{};
    std::vector<PureTreeNode> nodes_{};
    std::vector<PureTreeEdge> edges_{};
};

void validate_pure_tree(const PureTree& tree);

[[nodiscard]] std::string serialize_pure_tree(const PureTree& tree);
[[nodiscard]] PureTree deserialize_pure_tree(std::string_view data);

void write_pure_tree(std::ostream& output, const PureTree& tree);
[[nodiscard]] PureTree read_pure_tree(std::istream& input);

void save_pure_tree(
    const std::filesystem::path& path,
    const PureTree& tree
);
[[nodiscard]] PureTree load_pure_tree(const std::filesystem::path& path);

} // namespace kadoka::shogi::tree
