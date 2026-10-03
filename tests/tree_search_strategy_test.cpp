#include "kadoka/tree/tree_search_strategy.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

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

} // namespace

int main() {
    const PureTreeNode parent{
        "parent", "position", "path", 16, 8, 4, 4, 0.5
    };
    const std::vector<PureTreeEdge> edges{
        {"edge-b", "parent", "child-b", "2g2f", 3, 1, 1, 1, 0.2},
        {"edge-a", "parent", "child-a", "7g7f", 3, 1, 1, 1, 0.2},
        {"edge-unvisited", "parent", "child-c", "3g3f", 0, 0, 0, 0, 0.0},
    };

    UctStrategy uct{1.0};
    assert(uct.id() == "kadoka.tree_search.uct.v1");
    const std::vector<TreeSearchScore> uct_scores = uct.score_edges(parent, edges);
    assert(uct_scores.size() == edges.size());
    const auto unvisited_score = std::find_if(
        uct_scores.begin(),
        uct_scores.end(),
        [](const auto& score) { return score.edge_id == "edge-unvisited"; }
    );
    assert(unvisited_score != uct_scores.end());
    assert(std::isinf(unvisited_score->score));
    assert(uct.choose_edge(parent, edges) == "edge-unvisited");

    const std::vector<PureTreeEdge> tied_unvisited{
        {"z-edge", "parent", "child-z", "4g4f"},
        {"a-edge", "parent", "child-a2", "5g5f"},
    };
    assert(uct.choose_edge(parent, tied_unvisited) == "a-edge");
    assert(!uct.choose_edge(parent, {}).has_value());

    PuctStrategy puct{
        1.0,
        [](const PureTreeNode&, const PureTreeEdge& edge) {
            return edge.id == "edge-a" ? 0.9 : 0.1;
        },
    };
    assert(puct.id() == "kadoka.tree_search.puct.v1");
    const std::vector<PureTreeEdge> visited_edges{edges[0], edges[1]};
    assert(puct.choose_edge(parent, visited_edges) == "edge-a");

    expect_invalid([] { (void)UctStrategy{-1.0}; });
    expect_invalid([] {
        (void)UctStrategy{std::numeric_limits<double>::quiet_NaN()};
    });
    expect_invalid([] {
        (void)PuctStrategy{1.0, {}};
    });
    expect_invalid([] {
        (void)PuctStrategy{
            1.0,
            [](const PureTreeNode&, const PureTreeEdge&) { return -0.1; },
        }.score_edges(
            PureTreeNode{"p", "position", "path"},
            std::vector<PureTreeEdge>{{"e", "p", "c", "7g7f"}}
        );
    });
    expect_invalid([&] {
        (void)uct.score_edges(
            parent,
            std::vector<PureTreeEdge>{{"wrong-parent", "other", "child", "7g7f"}}
        );
    });

    PureTree tree = PureTree::empty();
    tree.add_node(PureTreeNode{"root", "root-pos", "root-path", 3, 1, 1, 1, 0.25});
    tree.add_node(PureTreeNode{"child", "child-pos", "root-path/7g7f"});
    tree.set_root("root");
    tree.add_edge(PureTreeEdge{"edge", "root", "child", "7g7f", 2, 1, 0, 1, 0.5});
    tree.record_node_visit("root", TreeOutcome::Loss, -1.0);
    tree.record_edge_visit("edge", TreeOutcome::Draw, 0.0);
    assert(tree.nodes()[0].visits == 4);
    assert(tree.nodes()[0].losses == 2);
    assert(tree.nodes()[0].mean_value == -0.0625);
    assert(tree.edges()[0].visits == 3);
    assert(tree.edges()[0].draws == 2);
    assert(std::abs(tree.edges()[0].mean_value - (1.0 / 3.0)) < 1e-12);

    const PureTree reloaded = deserialize_pure_tree(serialize_pure_tree(tree));
    assert(reloaded.nodes()[0].visits == 4);
    assert(reloaded.nodes()[0].losses == 2);
    assert(reloaded.edges()[0].visits == 3);
    assert(reloaded.edges()[0].draws == 2);
}
