#include "kadoka/tree/pure_tree.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace kadoka::shogi::tree;

namespace {

template <typename Action>
void expect_invalid(Action&& action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

PureTree sample_tree() {
    PureTree tree = PureTree::empty();
    tree.add_node(PureTreeNode{
        "root\tid",
        "start-position\nseed",
        "root-path",
        3,
        1,
        1,
        1,
        0.25,
    });
    tree.add_node(PureTreeNode{
        "child-a",
        "same-position",
        "root-path/7g7f",
        2,
        1,
        0,
        1,
        0.5,
    });
    tree.add_node(PureTreeNode{
        "child-b",
        "same-position",
        "root-path/2g2f",
        1,
        0,
        1,
        0,
        -0.5,
    });
    tree.set_root("root\tid");
    tree.add_edge(PureTreeEdge{
        "edge-a",
        "root\tid",
        "child-a",
        "7g7f",
        2,
        1,
        0,
        1,
        0.5,
    });
    tree.add_edge(PureTreeEdge{
        "edge-b",
        "root\tid",
        "child-b",
        "2g2f",
        1,
        0,
        1,
        0,
        -0.5,
    });
    return tree;
}

void assert_same_tree(const PureTree& expected, const PureTree& actual) {
    assert(expected.root_node_id() == actual.root_node_id());
    assert(expected.nodes().size() == actual.nodes().size());
    assert(expected.edges().size() == actual.edges().size());
    for (std::size_t i = 0; i < expected.nodes().size(); ++i) {
        const PureTreeNode& lhs = expected.nodes()[i];
        const PureTreeNode& rhs = actual.nodes()[i];
        assert(lhs.id == rhs.id);
        assert(lhs.position_identity == rhs.position_identity);
        assert(lhs.path_identity == rhs.path_identity);
        assert(lhs.visits == rhs.visits);
        assert(lhs.wins == rhs.wins);
        assert(lhs.losses == rhs.losses);
        assert(lhs.draws == rhs.draws);
        assert(lhs.mean_value == rhs.mean_value);
    }
    for (std::size_t i = 0; i < expected.edges().size(); ++i) {
        const PureTreeEdge& lhs = expected.edges()[i];
        const PureTreeEdge& rhs = actual.edges()[i];
        assert(lhs.id == rhs.id);
        assert(lhs.parent_node_id == rhs.parent_node_id);
        assert(lhs.child_node_id == rhs.child_node_id);
        assert(lhs.move_usi == rhs.move_usi);
        assert(lhs.visits == rhs.visits);
        assert(lhs.wins == rhs.wins);
        assert(lhs.losses == rhs.losses);
        assert(lhs.draws == rhs.draws);
        assert(lhs.mean_value == rhs.mean_value);
    }
}

} // namespace

int main() {
    const PureTree empty = PureTree::empty();
    validate_pure_tree(empty);
    const PureTree empty_round_trip = deserialize_pure_tree(
        serialize_pure_tree(empty)
    );
    assert(empty_round_trip.nodes().empty());
    assert(empty_round_trip.edges().empty());
    assert(!empty_round_trip.root_node_id().has_value());

    const PureTree expected = sample_tree();
    validate_pure_tree(expected);
    const std::string serialized = serialize_pure_tree(expected);
    assert(serialized.find("KADOKA_PURE_TREE\t1") == 0);
    assert(serialized.find("\\t") != std::string::npos);
    const PureTree decoded = deserialize_pure_tree(serialized);
    assert_same_tree(expected, decoded);

    const auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / ("kadoka_pure_tree_" + std::to_string(stamp) + ".tree");
    save_pure_tree(path, expected);
    const PureTree from_file = load_pure_tree(path);
    assert_same_tree(expected, from_file);
    std::filesystem::remove(path);

    PureTree duplicate_identity = PureTree::empty();
    duplicate_identity.add_node(PureTreeNode{"first", "position", "path"});
    expect_invalid([&] {
        duplicate_identity.add_node(
            PureTreeNode{"second", "position", "path"}
        );
    });

    PureTree invalid_mean = PureTree::empty();
    expect_invalid([&] {
        invalid_mean.add_node(PureTreeNode{
            "bad", "position", "path", 0, 0, 0, 0, 1.1
        });
    });

    PureTree missing_root = PureTree::empty();
    missing_root.add_node(PureTreeNode{"node", "position", "path"});
    expect_invalid([&] { validate_pure_tree(missing_root); });

    PureTree disconnected = PureTree::empty();
    disconnected.add_node(PureTreeNode{"root", "p0", "root"});
    disconnected.add_node(PureTreeNode{"orphan", "p1", "orphan"});
    disconnected.set_root("root");
    expect_invalid([&] { validate_pure_tree(disconnected); });

    PureTree cycle = PureTree::empty();
    cycle.add_node(PureTreeNode{"a", "p0", "a"});
    cycle.add_node(PureTreeNode{"b", "p1", "a/b"});
    cycle.set_root("a");
    cycle.add_edge(PureTreeEdge{"ab", "a", "b", "7g7f"});
    cycle.add_edge(PureTreeEdge{"ba", "b", "a", "3c3d"});
    expect_invalid([&] { validate_pure_tree(cycle); });

    expect_invalid([] {
        (void)deserialize_pure_tree("KADOKA_PURE_TREE\t2\nROOT\t\n");
    });
    expect_invalid([] {
        (void)deserialize_pure_tree(
            "KADOKA_PURE_TREE\t1\nROOT\t\nNODE\tn\tp\tpath\t0\t0\t0\t0\t0\n"
        );
    });
}
