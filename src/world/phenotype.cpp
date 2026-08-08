#include <clife/world/phenotype.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace clife::world {
namespace {

[[nodiscard]] Amount parameter_value(std::span<const ParameterValue> values, ParameterId id)
{
    const auto found = std::ranges::find(values, id, &ParameterValue::parameter);
    if (found == values.end()) {
        throw std::invalid_argument{"phenotype parameter value is missing"};
    }
    return found->value;
}

} // namespace

CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template)
{
    const ObjectTemplate& object = definition.object_template(source_template);
    CompiledPhenotype phenotype;
    phenotype.source_template_ = source_template;
    phenotype.functions_.reserve(object.genome.size());
    for (const GenomeFunctionInstance& instance : object.genome) {
        const FunctionTypeDefinition& type = definition.function_type(instance.type);
        if (instance.parameters.size() != type.genome_parameters.size()) {
            throw std::invalid_argument{"genome function has an incomplete parameter set"};
        }
        CompiledFunctionPhenotype function;
        function.type_ = instance.type;
        function.genome_parameters_ = instance.parameters;
        for (const GenomeParameterDefinition& parameter : type.genome_parameters) {
            const auto count = std::ranges::count(instance.parameters, parameter.id, &ParameterValue::parameter);
            if (count != 1) {
                throw std::invalid_argument{"genome function parameter identity is missing or duplicated"};
            }
            const Amount value = parameter_value(instance.parameters, parameter.id);
            if (!std::isfinite(value)) {
                throw std::invalid_argument{"genome parameter must be finite"};
            }
        }

        std::vector<ParameterValue> resolved = function.genome_parameters_;
        resolved.reserve(resolved.size() + type.derived_parameters.size());
        for (const DerivedParameterDefinition& parameter : type.derived_parameters) {
            const Amount value = parameter.expression.evaluate(resolved);
            function.derived_parameters_.push_back({.parameter = parameter.id, .value = value});
            resolved.push_back({.parameter = parameter.id, .value = value});
        }
        if (type.process) {
            const Amount throughput = parameter_value(resolved, type.process->throughput);
            const Amount result_per_input = parameter_value(resolved, type.process->result_per_input);
            if (!std::isfinite(throughput) || throughput <= 0.0) {
                throw std::invalid_argument{"compiled process throughput must be finite and positive"};
            }
            if (!std::isfinite(result_per_input) || result_per_input < 0.0) {
                throw std::invalid_argument{"compiled process result_per_input must be finite and non-negative"};
            }
            function.process_parameters_ = CompiledProcessParameters{
                .throughput = throughput,
                .result_per_input = result_per_input,
            };
        }
        phenotype.functions_.push_back(std::move(function));
    }
    return phenotype;
}

FunctionTypeId CompiledFunctionPhenotype::type() const noexcept { return type_; }

std::span<const ParameterValue> CompiledFunctionPhenotype::genome_parameters() const noexcept
{
    return genome_parameters_;
}

std::span<const ParameterValue> CompiledFunctionPhenotype::derived_parameters() const noexcept
{
    return derived_parameters_;
}

Amount CompiledFunctionPhenotype::parameter(ParameterId id) const
{
    const auto genome = std::ranges::find(genome_parameters_, id, &ParameterValue::parameter);
    if (genome != genome_parameters_.end()) {
        return genome->value;
    }
    return parameter_value(derived_parameters_, id);
}

const std::optional<CompiledProcessParameters>& CompiledFunctionPhenotype::process_parameters() const noexcept
{
    return process_parameters_;
}

TemplateId CompiledPhenotype::source_template() const noexcept { return source_template_; }

std::span<const CompiledFunctionPhenotype> CompiledPhenotype::functions() const noexcept { return functions_; }

const CompiledFunctionPhenotype& CompiledPhenotype::function(std::size_t index) const
{
    if (index >= functions_.size()) {
        throw std::out_of_range{"compiled phenotype function index is out of range"};
    }
    return functions_[index];
}

} // namespace clife::world
