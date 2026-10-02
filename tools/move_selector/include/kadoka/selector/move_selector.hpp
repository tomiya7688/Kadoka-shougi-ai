#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace kadoka::shogi::selector {

struct ScoredMove {
    std::string usi{};
    double score{0.0};
};

struct SelectionRequest {
    std::span<const ScoredMove> scored_moves{};
    std::span<const std::string> legal_moves{};
    std::optional<std::string_view> fallback_move{};
};

struct MoveSelection {
    std::optional<std::string> move{};
    bool used_fallback{false};
};

class MoveSelector {
public:
    virtual ~MoveSelector() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual MoveSelection select(
        const SelectionRequest& request
    ) = 0;
};

class ArgmaxSelector final : public MoveSelector {
public:
    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] MoveSelection select(
        const SelectionRequest& request
    ) override;
};

class TopKSelector final : public MoveSelector {
public:
    explicit TopKSelector(std::size_t k, std::uint64_t seed = 0);

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] MoveSelection select(
        const SelectionRequest& request
    ) override;

private:
    std::size_t k_;
    std::uint64_t state_;
};

class ThresholdSelector final : public MoveSelector {
public:
    explicit ThresholdSelector(double threshold, std::uint64_t seed = 0);

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] MoveSelection select(
        const SelectionRequest& request
    ) override;

private:
    double threshold_;
    std::uint64_t state_;
};

} // namespace kadoka::shogi::selector
