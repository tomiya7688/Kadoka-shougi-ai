#pragma once

#include "kadoka/tree/pure_tree.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::tree {

struct TreeSearchScore {
    std::string edge_id{};
    double score{0.0};
};

class TreeSearchStrategy {
public:
    virtual ~TreeSearchStrategy() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::vector<TreeSearchScore> score_edges(
        const PureTreeNode& parent,
        std::span<const PureTreeEdge> candidates
    ) const = 0;

    [[nodiscard]] std::optional<std::string> choose_edge(
        const PureTreeNode& parent,
        std::span<const PureTreeEdge> candidates
    ) const;
};

class UctStrategy final : public TreeSearchStrategy {
public:
    explicit UctStrategy(double exploration = 1.4142135623730951);

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] std::vector<TreeSearchScore> score_edges(
        const PureTreeNode& parent,
        std::span<const PureTreeEdge> candidates
    ) const override;

private:
    double exploration_;
};

using TreePriorFunction = std::function<double(
    const PureTreeNode& parent,
    const PureTreeEdge& edge
)>;

class PuctStrategy final : public TreeSearchStrategy {
public:
    PuctStrategy(double exploration, TreePriorFunction prior_function);

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] std::vector<TreeSearchScore> score_edges(
        const PureTreeNode& parent,
        std::span<const PureTreeEdge> candidates
    ) const override;

private:
    double exploration_;
    TreePriorFunction prior_function_;
};

} // namespace kadoka::shogi::tree
