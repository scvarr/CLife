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

struct FunctionOutput final {
    ValueId value;
    Amount result_per_input{1.0};
};

struct Function final {
    ValueId input;
    // Kept to read programs authored before multi-output functions.
    ValueId output{};
    Amount throughput;
    Amount result_per_input{1.0};
    std::vector<FunctionOutput> outputs;
};

struct BufferProcess final {
    ValueId value;
    Amount capacity;
    Amount throughput;
    Amount leakage{};
    Amount initial_amount{};
};

struct BufferState final {
    Amount stored_amount{};
    Amount received_last_tick{};
    Amount supplied_last_tick{};
};

struct EndBufferTransfer final {
    ValueId source;
    ValueId target;
    Amount target_per_source{1.0};
};

struct EndRule final {
    ValueId source;
    ValueId target;
    Amount target_per_source;

    friend constexpr bool operator==(const EndRule&, const EndRule&) noexcept = default;
};

struct Program final {
    std::size_t value_count{};
    std::vector<Function> functions{};
    std::vector<BufferProcess> buffers{};
    std::vector<EndBufferTransfer> end_buffer_transfers{};
    std::vector<EndRule> end_rules{};
    std::vector<ValueAmount> initial_values{};
};

class Calculator final {
public:
    explicit Calculator(Program program);

    void step(std::span<const ValueAmount> external_values);
    void apply_delta(ValueId id, Amount delta);
    Amount finalize_residual(ValueId id);

    [[nodiscard]] Amount value(ValueId id) const noexcept;
    [[nodiscard]] const BufferState& buffer_state(std::size_t index) const;
    [[nodiscard]] std::size_t buffer_count() const noexcept;
    [[nodiscard]] Amount end_value(ValueId id) const noexcept;
    [[nodiscard]] std::size_t value_count() const noexcept;

private:
    void validate_value(ValueId id, const char* context) const;
    void compile_pipeline();

    Program program_;
    std::vector<Amount> values_;
    std::vector<bool> generated_;
    std::vector<std::vector<std::size_t>> outgoing_;
    std::vector<std::vector<std::size_t>> buffers_by_value_;
    std::vector<ValueId> evaluation_order_;
    std::vector<BufferState> buffer_states_;
    std::vector<Amount> end_buffer_;
    std::vector<Amount> end_delta_;
};

} // namespace clife
