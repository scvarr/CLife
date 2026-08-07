#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace clife {

using Amount = double;
using ValueIndex = std::uint32_t;

struct ValueId final {
    ValueIndex index;

    friend constexpr bool operator==(ValueId, ValueId) noexcept = default;
};

struct ValueAmount final {
    ValueId value;
    Amount amount;
};

struct Function final {
    ValueId input;
    ValueId output;
    Amount throughput;
    Amount result_per_input{1.0};
};

struct EndRule final {
    ValueId source;
    ValueId target;
    Amount target_per_source;
};

struct Program final {
    std::size_t value_count{};
    std::vector<Function> functions{};
    std::vector<EndRule> end_rules{};
    std::vector<ValueAmount> initial_values{};
};

class Calculator final {
public:
    explicit Calculator(Program program);

    void step(std::span<const ValueAmount> external_values);

    [[nodiscard]] Amount value(ValueId id) const noexcept;
    [[nodiscard]] std::size_t value_count() const noexcept;

private:
    void validate_value(ValueId id, const char* context) const;
    void compile_pipeline();

    Program program_;
    std::vector<Amount> values_;
    std::vector<bool> generated_;
    std::vector<std::vector<std::size_t>> outgoing_;
    std::vector<ValueId> evaluation_order_;
    std::vector<Amount> end_snapshot_;
    std::vector<Amount> end_delta_;
};

} // namespace clife
