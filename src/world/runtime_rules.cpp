#include <clife/world/runtime_rules.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace clife::world {
namespace {

bool has_input(const CalculationDefinition& calculation, CalculationPortId id)
{
    return std::ranges::any_of(calculation.inputs, [id](const CalculationInputDefinition& input) { return input.id == id; });
}

bool has_output(const CalculationDefinition& calculation, CalculationPortId id)
{
    return std::ranges::any_of(calculation.outputs, [id](const CalculationOutputDefinition& output) { return output.id == id; });
}

} // namespace

RuntimeRuleExecutor::RuntimeRuleExecutor(std::vector<RuntimeWorldRule> rules) : rules_(std::move(rules))
{
    std::vector<ValueId> sources;
    for (const RuntimeWorldRule& rule : rules_) {
        if (std::ranges::find(sources, rule.source) != sources.end()) {
            throw std::invalid_argument{"runtime value has more than one consuming world rule"};
        }
        sources.push_back(rule.source);
        if (std::ranges::none_of(rule.inputs, [&](const RuntimeRuleInputBinding& input) {
                return input.kind == RuntimeRuleInputKind::end_residual && input.value == rule.source;
            })) {
            throw std::invalid_argument{"runtime rule must bind its consumed source as an end residual"};
        }
        for (const RuntimeRuleInputBinding& input : rule.inputs) {
            if (!has_input(rule.calculation, input.input)) throw std::invalid_argument{"runtime rule input is missing"};
        }
        for (const RuntimeRuleOutputBinding& output : rule.outputs) {
            if (!has_output(rule.calculation, output.output)) throw std::invalid_argument{"runtime rule output is missing"};
        }
    }
}

void RuntimeRuleExecutor::apply(Calculator& calculator, const CompiledPhenotype& phenotype) const
{
    for (const RuntimeWorldRule& rule : rules_) {
        if (rule.source.index >= calculator.value_count()) {
            throw std::invalid_argument{"runtime rule source value is outside calculator"};
        }
        (void)calculator.finalize_residual(rule.source);
    }

    std::vector<std::pair<ValueId, Amount>> deltas;
    for (const RuntimeWorldRule& rule : rules_) {
        std::vector<CalculationPortAmount> inputs;
        inputs.reserve(rule.inputs.size());
        for (const RuntimeRuleInputBinding& binding : rule.inputs) {
            if (binding.kind != RuntimeRuleInputKind::object_characteristic &&
                binding.value.index >= calculator.value_count()) {
                throw std::invalid_argument{"runtime rule input value is outside calculator"};
            }
            Amount amount{};
            if (binding.kind == RuntimeRuleInputKind::end_residual) amount = calculator.end_value(binding.value);
            else if (binding.kind == RuntimeRuleInputKind::runtime_value) amount = calculator.value(binding.value);
            else amount = phenotype.characteristic(binding.characteristic);
            inputs.push_back({.port = binding.input, .amount = amount});
        }
        const std::vector<CalculationPortAmount> outputs = evaluate_calculation(rule.calculation, inputs);
        for (const RuntimeRuleOutputBinding& binding : rule.outputs) {
            const auto found = std::ranges::find(outputs, binding.output, &CalculationPortAmount::port);
            if (found == outputs.end()) throw std::invalid_argument{"runtime rule calculation output is missing"};
            deltas.push_back({binding.target, found->amount});
        }
    }
    for (const auto& [target, delta] : deltas) calculator.apply_delta(target, delta);
}

} // namespace clife::world
