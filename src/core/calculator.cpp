#include <clife/core/calculator.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace clife {

Calculator::Calculator(Program program)
    : program_(std::move(program)), values_(program_.value_count, 0.0), generated_(program_.value_count, false),
      outgoing_(program_.value_count), buffers_by_value_(program_.value_count),
      buffer_states_(program_.buffers.size()), end_buffer_(program_.value_count, 0.0),
      end_delta_(program_.value_count, 0.0)
{
    if (program_.value_count == 0) {
        throw std::invalid_argument{"calculator must contain at least one value"};
    }
    if (program_.value_count > static_cast<std::size_t>(std::numeric_limits<ValueIndex>::max())) {
        throw std::invalid_argument{"calculator contains too many values for ValueId"};
    }

    for (std::size_t i = 0; i < program_.initial_values.size(); ++i) {
        const ValueAmount& initial = program_.initial_values[i];
        validate_value(initial.value, "initial value");
        if (!std::isfinite(initial.amount)) {
            throw std::invalid_argument{"initial value must be finite"};
        }
        for (std::size_t previous = 0; previous < i; ++previous) {
            if (program_.initial_values[previous].value == initial.value) {
                throw std::invalid_argument{"value has more than one initial value"};
            }
        }
        values_[initial.value.index] = initial.amount;
    }

    for (const Function& function : program_.functions) {
        validate_value(function.input, "function input");
        if (!std::isfinite(function.throughput) || function.throughput <= 0.0) {
            throw std::invalid_argument{"function throughput must be finite and positive"};
        }
        const bool legacy_output = function.outputs.empty();
        const std::span<const FunctionOutput> outputs = legacy_output
                                                           ? std::span<const FunctionOutput>{}
                                                           : std::span<const FunctionOutput>{function.outputs};
        if (legacy_output) {
            validate_value(function.output, "function output");
            if (!std::isfinite(function.result_per_input) || function.result_per_input < 0.0) {
                throw std::invalid_argument{"function result_per_input must be finite and non-negative"};
            }
            generated_[function.output.index] = true;
            continue;
        }
        for (const FunctionOutput& output : outputs) {
            validate_value(output.value, "function output");
            if (!std::isfinite(output.result_per_input) || output.result_per_input < 0.0) {
                throw std::invalid_argument{"function result_per_input must be finite and non-negative"};
            }
            generated_[output.value.index] = true;
        }
    }

    for (Function& function : program_.functions) {
        if (function.outputs.empty()) {
            function.outputs.push_back({.value = function.output, .result_per_input = function.result_per_input});
        }
    }

    std::sort(program_.functions.begin(), program_.functions.end(), [](const Function& left, const Function& right) {
        return std::tuple{left.input.index, left.throughput, left.outputs.size()} <
               std::tuple{right.input.index, right.throughput, right.outputs.size()};
    });

    for (std::size_t index = 0; index < program_.buffers.size(); ++index) {
        const BufferProcess& buffer = program_.buffers[index];
        validate_value(buffer.value, "buffer value");
        if (!std::isfinite(buffer.capacity) || buffer.capacity < 0.0) {
            throw std::invalid_argument{"buffer capacity must be finite and non-negative"};
        }
        if (!std::isfinite(buffer.throughput) || buffer.throughput <= 0.0) {
            throw std::invalid_argument{"buffer throughput must be finite and positive"};
        }
        if (!std::isfinite(buffer.leakage) || buffer.leakage < 0.0) {
            throw std::invalid_argument{"buffer leakage must be finite and non-negative"};
        }
        if (!std::isfinite(buffer.initial_amount) || buffer.initial_amount < 0.0 ||
            buffer.initial_amount > buffer.capacity) {
            throw std::invalid_argument{"buffer initial amount must be finite and within capacity"};
        }
        buffer_states_[index].stored_amount = buffer.initial_amount;
    }

    for (const ValueAmount& initial : program_.initial_values) {
        if (generated_[initial.value.index]) {
            throw std::invalid_argument{"genome-produced value cannot also have a persistent initial value"};
        }
    }

    for (std::size_t i = 0; i < program_.end_buffer_transfers.size(); ++i) {
        const EndBufferTransfer& transfer = program_.end_buffer_transfers[i];
        validate_value(transfer.source, "end-buffer transfer source");
        validate_value(transfer.target, "end-buffer transfer target");
        if (!std::isfinite(transfer.target_per_source) || transfer.target_per_source < 0.0) {
            throw std::invalid_argument{"end-buffer transfer factor must be finite and non-negative"};
        }
        for (std::size_t previous = 0; previous < i; ++previous) {
            if (program_.end_buffer_transfers[previous].source == transfer.source) {
                throw std::invalid_argument{"value has more than one consuming end-buffer transfer"};
            }
        }
    }
    std::sort(program_.end_buffer_transfers.begin(), program_.end_buffer_transfers.end(),
              [](const EndBufferTransfer& left, const EndBufferTransfer& right) {
                  return std::tie(left.source.index, left.target.index, left.target_per_source) <
                         std::tie(right.source.index, right.target.index, right.target_per_source);
              });

    for (const EndRule& rule : program_.end_rules) {
        validate_value(rule.source, "end rule source");
        validate_value(rule.target, "end rule target");
        if (!std::isfinite(rule.target_per_source)) {
            throw std::invalid_argument{"end rule target_per_source must be finite"};
        }
        if (generated_[rule.target.index]) {
            throw std::invalid_argument{"end rule target must be persistent, not a genome-produced value"};
        }
    }

    std::sort(program_.end_rules.begin(), program_.end_rules.end(), [](const EndRule& left, const EndRule& right) {
        return std::tie(left.source.index, left.target.index, left.target_per_source) <
               std::tie(right.source.index, right.target.index, right.target_per_source);
    });

    compile_pipeline();
}

void Calculator::step(std::span<const ValueAmount> external_values)
{
    std::fill(end_buffer_.begin(), end_buffer_.end(), 0.0);
    for (std::size_t index = 0; index < generated_.size(); ++index) {
        if (generated_[index]) {
            values_[index] = 0.0;
        }
    }

    for (std::size_t i = 0; i < external_values.size(); ++i) {
        const ValueAmount& external = external_values[i];
        validate_value(external.value, "external value");
        if (generated_[external.value.index]) {
            throw std::invalid_argument{"external value cannot also be produced by the genome"};
        }
        if (!std::isfinite(external.amount)) {
            throw std::invalid_argument{"external value must be finite"};
        }
        for (std::size_t previous = 0; previous < i; ++previous) {
            if (external_values[previous].value == external.value) {
                throw std::invalid_argument{"external value supplied more than once in one tick"};
            }
        }
        values_[external.value.index] = external.amount;
    }

    for (BufferState& state : buffer_states_) {
        state.received_last_tick = 0.0;
        state.supplied_last_tick = 0.0;
    }

    for (const ValueId input : evaluation_order_) {
        const std::vector<std::size_t>& outgoing = outgoing_[input.index];
        const std::vector<std::size_t>& buffers = buffers_by_value_[input.index];
        if (outgoing.empty() && buffers.empty()) {
            continue;
        }

        const Amount available = values_[input.index];
        if (!std::isfinite(available) || available < 0.0) {
            throw std::domain_error{"genome function input must be finite and non-negative"};
        }

        Amount normal_demand{0.0};
        for (const std::size_t function_index : outgoing) {
            normal_demand += program_.functions[function_index].throughput;
        }
        if (!std::isfinite(normal_demand)) {
            throw std::overflow_error{"normal flow demand overflow"};
        }

        if (available >= normal_demand) {
            for (const std::size_t function_index : outgoing) {
                const Function& function = program_.functions[function_index];
                for (const FunctionOutput& output : function.outputs) {
                    values_[output.value.index] += function.throughput * output.result_per_input;
                }
            }

            const Amount surplus = available - normal_demand;
            Amount total_charge_demand{0.0};
            for (const std::size_t buffer_index : buffers) {
                const BufferProcess& buffer = program_.buffers[buffer_index];
                const Amount free_capacity = std::max(0.0, buffer.capacity - buffer_states_[buffer_index].stored_amount);
                total_charge_demand += std::min(free_capacity, buffer.throughput);
            }
            if (!std::isfinite(total_charge_demand)) {
                throw std::overflow_error{"buffer charge demand overflow"};
            }

            const Amount total_charge = std::min(surplus, total_charge_demand);
            const Amount charge_fraction = total_charge_demand > 0.0 ? total_charge / total_charge_demand : 0.0;
            for (const std::size_t buffer_index : buffers) {
                const BufferProcess& buffer = program_.buffers[buffer_index];
                BufferState& state = buffer_states_[buffer_index];
                const Amount free_capacity = std::max(0.0, buffer.capacity - state.stored_amount);
                const Amount charge_demand = std::min(free_capacity, buffer.throughput);
                state.received_last_tick = charge_demand * charge_fraction;
                state.stored_amount += state.received_last_tick;
            }
            values_[input.index] = surplus - total_charge;
        } else {
            const Amount deficit = normal_demand - available;
            Amount total_offer{0.0};
            for (const std::size_t buffer_index : buffers) {
                const BufferProcess& buffer = program_.buffers[buffer_index];
                total_offer += std::min(buffer_states_[buffer_index].stored_amount, buffer.throughput);
            }
            if (!std::isfinite(total_offer)) {
                throw std::overflow_error{"buffer discharge offer overflow"};
            }

            const Amount total_discharge = std::min(deficit, total_offer);
            const Amount discharge_fraction = total_offer > 0.0 ? total_discharge / total_offer : 0.0;
            const Amount demand_fraction = (available + total_discharge) / normal_demand;
            for (const std::size_t function_index : outgoing) {
                const Function& function = program_.functions[function_index];
                const Amount taken = function.throughput * demand_fraction;
                for (const FunctionOutput& output : function.outputs) {
                    values_[output.value.index] += taken * output.result_per_input;
                }
            }
            for (const std::size_t buffer_index : buffers) {
                const BufferProcess& buffer = program_.buffers[buffer_index];
                BufferState& state = buffer_states_[buffer_index];
                const Amount offer = std::min(state.stored_amount, buffer.throughput);
                state.supplied_last_tick = offer * discharge_fraction;
                state.stored_amount -= state.supplied_last_tick;
            }
            values_[input.index] = 0.0;
        }
    }

    for (std::size_t index = 0; index < program_.buffers.size(); ++index) {
        const BufferProcess& buffer = program_.buffers[index];
        BufferState& state = buffer_states_[index];
        const Amount leaked = std::min(state.stored_amount, buffer.leakage);
        state.stored_amount -= leaked;
        state.stored_amount = std::clamp(state.stored_amount, 0.0, buffer.capacity);
        end_buffer_[buffer.value.index] += leaked;
    }

    std::fill(end_delta_.begin(), end_delta_.end(), 0.0);

    for (const EndBufferTransfer& transfer : program_.end_buffer_transfers) {
        const Amount source = values_[transfer.source.index];
        if (!std::isfinite(source) || source < 0.0) {
            throw std::domain_error{"end-buffer transfer source must be finite and non-negative"};
        }
        end_buffer_[transfer.target.index] += source * transfer.target_per_source;
        if (!std::isfinite(end_buffer_[transfer.target.index])) {
            throw std::overflow_error{"end buffer produced a non-finite value"};
        }
        values_[transfer.source.index] = 0.0;
    }

    for (const EndRule& rule : program_.end_rules) {
        const Amount source = end_buffer_[rule.source.index];
        if (!std::isfinite(source) || source < 0.0) {
            throw std::domain_error{"end rule source must be finite and non-negative"};
        }
        end_delta_[rule.target.index] += source * rule.target_per_source;
    }

    for (std::size_t index = 0; index < values_.size(); ++index) {
        values_[index] += end_delta_[index];
        if (!std::isfinite(values_[index])) {
            throw std::overflow_error{"calculator produced a non-finite value"};
        }
    }
}

Amount Calculator::value(ValueId id) const noexcept
{
    const auto index = static_cast<std::size_t>(id.index);
    return index < values_.size() ? values_[index] : 0.0;
}

void Calculator::apply_delta(ValueId id, Amount delta)
{
    validate_value(id, "runtime value delta");
    if (!std::isfinite(delta)) {
        throw std::invalid_argument{"runtime value delta must be finite"};
    }
    values_[id.index] += delta;
    if (!std::isfinite(values_[id.index])) {
        throw std::overflow_error{"runtime value delta produced a non-finite value"};
    }
}

const BufferState& Calculator::buffer_state(std::size_t index) const
{
    if (index >= buffer_states_.size()) {
        throw std::out_of_range{"buffer index is out of range"};
    }
    return buffer_states_[index];
}

std::size_t Calculator::buffer_count() const noexcept { return buffer_states_.size(); }

Amount Calculator::end_value(ValueId id) const noexcept
{
    const auto index = static_cast<std::size_t>(id.index);
    return index < end_buffer_.size() ? end_buffer_[index] : 0.0;
}

std::size_t Calculator::value_count() const noexcept
{
    return values_.size();
}

void Calculator::validate_value(ValueId id, const char* context) const
{
    if (static_cast<std::size_t>(id.index) >= program_.value_count) {
        throw std::invalid_argument{std::string{context} + " references value outside calculator"};
    }
}

void Calculator::compile_pipeline()
{
    std::vector<std::size_t> indegree(program_.value_count, 0);
    for (std::size_t function_index = 0; function_index < program_.functions.size(); ++function_index) {
        const Function& function = program_.functions[function_index];
        outgoing_[function.input.index].push_back(function_index);
        for (const FunctionOutput& output : function.outputs) {
            ++indegree[output.value.index];
        }
    }
    for (std::size_t buffer_index = 0; buffer_index < program_.buffers.size(); ++buffer_index) {
        buffers_by_value_[program_.buffers[buffer_index].value.index].push_back(buffer_index);
    }
    for (std::vector<std::size_t>& buffers : buffers_by_value_) {
        std::ranges::sort(buffers, [&](std::size_t left_index, std::size_t right_index) {
            const BufferProcess& left = program_.buffers[left_index];
            const BufferProcess& right = program_.buffers[right_index];
            return std::tie(left.capacity, left.throughput, left.leakage, left.initial_amount, left_index) <
                   std::tie(right.capacity, right.throughput, right.leakage, right.initial_amount, right_index);
        });
    }

    std::deque<ValueId> ready;
    for (std::size_t index = 0; index < indegree.size(); ++index) {
        if (indegree[index] == 0) {
            ready.push_back(ValueId{static_cast<ValueIndex>(index)});
        }
    }

    evaluation_order_.reserve(program_.value_count);
    while (!ready.empty()) {
        const ValueId value = ready.front();
        ready.pop_front();
        evaluation_order_.push_back(value);

        for (const std::size_t function_index : outgoing_[value.index]) {
            for (const FunctionOutput& output : program_.functions[function_index].outputs) {
                --indegree[output.value.index];
                if (indegree[output.value.index] == 0) {
                    ready.push_back(output.value);
                }
            }
        }
    }

    if (evaluation_order_.size() != program_.value_count) {
        throw std::invalid_argument{"genome pipeline contains a same-tick cycle"};
    }
}

} // namespace clife
