#include <clife/world/calculation.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace clife::world {

std::vector<CalculationPortAmount> evaluate_calculation(const CalculationDefinition& calculation,
                                                        std::span<const CalculationPortAmount> inputs)
{
    for (const CalculationPortAmount& input : inputs) {
        if (std::ranges::none_of(calculation.inputs,
                                 [input](const CalculationInputDefinition& definition) {
                                     return definition.id == input.port;
                                 })) {
            throw std::invalid_argument{"calculation input port is unknown"};
        }
        if (std::ranges::count(inputs, input.port, &CalculationPortAmount::port) != 1) {
            throw std::invalid_argument{"calculation input is supplied more than once"};
        }
        if (!std::isfinite(input.amount)) {
            throw std::invalid_argument{"calculation input must be finite"};
        }
    }
    if (inputs.size() != calculation.inputs.size()) {
        throw std::invalid_argument{"calculation input is missing"};
    }

    std::vector<ParameterValue> resolved;
    resolved.reserve(calculation.inputs.size() + calculation.outputs.size());
    for (const CalculationInputDefinition& input : calculation.inputs) {
        const auto found = std::ranges::find(inputs, input.id, &CalculationPortAmount::port);
        if (found == inputs.end()) {
            throw std::invalid_argument{"calculation input is missing"};
        }
        resolved.push_back({.parameter = ParameterId{input.id.value}, .value = found->amount});
    }

    std::vector<CalculationPortAmount> results;
    results.reserve(calculation.outputs.size());
    for (const CalculationOutputDefinition& output : calculation.outputs) {
        const Amount amount = output.expression.evaluate(resolved);
        resolved.push_back({.parameter = ParameterId{output.id.value}, .value = amount});
        results.push_back({.port = output.id, .amount = amount});
    }
    return results;
}

} // namespace clife::world
