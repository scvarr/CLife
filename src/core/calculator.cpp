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
      outgoing_(program_.value_count), end_snapshot_(program_.value_count, 0.0), end_delta_(program_.value_count, 0.0)
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
        validate_value(function.output, "function output");
        if (!std::isfinite(function.throughput) || function.throughput <= 0.0) {
            throw std::invalid_argument{"function throughput must be finite and positive"};
        }
        if (!std::isfinite(function.result_per_input) || function.result_per_input < 0.0) {
            throw std::invalid_argument{"function result_per_input must be finite and non-negative"};
        }
        generated_[function.output.index] = true;
    }

    std::sort(program_.functions.begin(), program_.functions.end(), [](const Function& left, const Function& right) {
        return std::tie(left.input.index, left.output.index, left.throughput, left.result_per_input) <
               std::tie(right.input.index, right.output.index, right.throughput, right.result_per_input);
    });

    for (const ValueAmount& initial : program_.initial_values) {
        if (generated_[initial.value.index]) {
            throw std::invalid_argument{"genome-produced value cannot also have a persistent initial value"};
        }
    }

    for (std::size_t i = 0; i < program_.end_rules.size(); ++i) {
        const EndRule& rule = program_.end_rules[i];
        validate_value(rule.source, "end rule source");
        validate_value(rule.target, "end rule target");
        if (!std::isfinite(rule.target_per_source)) {
            throw std::invalid_argument{"end rule target_per_source must be finite"};
        }
        for (std::size_t previous = 0; previous < i; ++previous) {
            if (program_.end_rules[previous].source == rule.source) {
                throw std::invalid_argument{"value has more than one consuming end rule"};
            }
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

    for (const ValueId input : evaluation_order_) {
        const std::vector<std::size_t>& outgoing = outgoing_[input.index];
        if (outgoing.empty()) {
            continue;
        }

        const Amount available = values_[input.index];
        if (!std::isfinite(available) || available < 0.0) {
            throw std::domain_error{"genome function input must be finite and non-negative"};
        }

        Amount total_demand{0.0};
        for (const std::size_t function_index : outgoing) {
            total_demand += program_.functions[function_index].throughput;
        }
        if (!std::isfinite(total_demand)) {
            throw std::overflow_error{"function demand overflow"};
        }

        const Amount load_fraction = std::min(1.0, available / total_demand);
        Amount total_taken{0.0};
        for (const std::size_t function_index : outgoing) {
            const Function& function = program_.functions[function_index];
            const Amount taken = function.throughput * load_fraction;
            const Amount produced = taken * function.result_per_input;
            values_[function.output.index] += produced;
            total_taken += taken;
        }

        values_[input.index] = std::max(0.0, available - total_taken);
    }

    end_snapshot_ = values_;
    std::fill(end_delta_.begin(), end_delta_.end(), 0.0);

    for (const EndRule& rule : program_.end_rules) {
        const Amount source = end_snapshot_[rule.source.index];
        if (!std::isfinite(source) || source < 0.0) {
            throw std::domain_error{"end rule source must be finite and non-negative"};
        }
        end_delta_[rule.target.index] += source * rule.target_per_source;
    }

    for (const EndRule& rule : program_.end_rules) {
        values_[rule.source.index] = 0.0;
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
        ++indegree[function.output.index];
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
            const ValueId output = program_.functions[function_index].output;
            --indegree[output.index];
            if (indegree[output.index] == 0) {
                ready.push_back(output);
            }
        }
    }

    if (evaluation_order_.size() != program_.value_count) {
        throw std::invalid_argument{"genome pipeline contains a same-tick cycle"};
    }
}

} // namespace clife
