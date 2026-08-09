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

void add_material(std::vector<MaterialAmount>& materials, ValueKey value, Amount amount)
{
    if (!std::isfinite(amount) || amount < 0.0) {
        throw std::invalid_argument{"material contribution must be finite and non-negative"};
    }
    const auto found = std::ranges::find(materials, value, &MaterialAmount::value);
    if (found == materials.end()) {
        materials.push_back({.value = value, .amount = amount});
    } else {
        found->amount += amount;
        if (!std::isfinite(found->amount)) {
            throw std::overflow_error{"material contribution total overflow"};
        }
    }
}

} // namespace

CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template)
{
    const ObjectTemplate& object = definition.object_template(source_template);
    CompiledPhenotype phenotype;
    phenotype.source_template_ = source_template;
    for (const TemplateMaterialContributionDefinition& contribution : object.material_contributions) {
        add_material(phenotype.material_amounts_, contribution.value, contribution.amount);
    }
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
            if (!std::isfinite(throughput) || throughput <= 0.0) {
                throw std::invalid_argument{"compiled process throughput must be finite and positive"};
            }
            if (type.process->conversion.value == 0) {
                const Amount result_per_input = parameter_value(resolved, type.process->result_per_input);
                if (!std::isfinite(result_per_input) || result_per_input < 0.0) {
                    throw std::invalid_argument{"compiled process result_per_input must be finite and non-negative"};
                }
                function.process_parameters_ = CompiledProcessParameters{
                    .throughput = throughput,
                    .outputs = {{.output = type.process->output, .result_per_input = result_per_input}},
                };
            } else {
            const UnitConversionDefinition& conversion = definition.unit_conversion(type.process->conversion);
            const Amount conversion_ratio = conversion.target_amount / conversion.source_amount;
            if (!std::isfinite(conversion_ratio) || conversion_ratio < 0.0) {
                throw std::invalid_argument{"compiled unit conversion ratio must be finite and non-negative"};
            }
            CompiledProcessParameters parameters{.throughput = throughput};
            Amount allocation_sum{};
            for (const FunctionProcessOutputDefinition& output : type.process->outputs) {
                const Amount allocation = parameter_value(resolved, output.allocation);
                if (!std::isfinite(allocation) || allocation < 0.0) {
                    throw std::invalid_argument{"compiled process allocation must be finite and non-negative"};
                }
                allocation_sum += allocation;
                if (!std::isfinite(allocation_sum)) {
                    throw std::overflow_error{"compiled process allocation total overflow"};
                }
                const Amount result_per_input = conversion_ratio * allocation;
                if (!std::isfinite(result_per_input)) {
                    throw std::overflow_error{"compiled process output overflow"};
                }
                parameters.outputs.push_back({.output = output.output, .result_per_input = result_per_input});
            }
            constexpr Amount kAllocationTolerance = 1e-12;
            if (allocation_sum > 1.0 + kAllocationTolerance) {
                throw std::invalid_argument{"compiled process allocations must not exceed one"};
            }
            function.process_parameters_ = std::move(parameters);
            }
        }
        if (type.buffer_process) {
            const Amount capacity = parameter_value(resolved, type.buffer_process->capacity);
            const Amount throughput = parameter_value(resolved, type.buffer_process->throughput);
            const Amount leakage = parameter_value(resolved, type.buffer_process->leakage);
            if (!std::isfinite(capacity) || capacity < 0.0) {
                throw std::invalid_argument{"compiled buffer capacity must be finite and non-negative"};
            }
            if (!std::isfinite(throughput) || throughput <= 0.0) {
                throw std::invalid_argument{"compiled buffer throughput must be finite and positive"};
            }
            if (!std::isfinite(leakage) || leakage < 0.0) {
                throw std::invalid_argument{"compiled buffer leakage must be finite and non-negative"};
            }
            function.buffer_parameters_ = CompiledBufferParameters{
                .capacity = capacity,
                .throughput = throughput,
                .leakage = leakage,
            };
        }
        for (const MaterialContributionDefinition& contribution : type.material_contributions) {
            add_material(phenotype.material_amounts_, contribution.value, contribution.amount.evaluate(resolved));
        }
        phenotype.functions_.push_back(std::move(function));
    }
    std::ranges::sort(phenotype.material_amounts_, {}, &MaterialAmount::value);
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

const std::optional<CompiledBufferParameters>& CompiledFunctionPhenotype::buffer_parameters() const noexcept
{
    return buffer_parameters_;
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

std::span<const MaterialAmount> CompiledPhenotype::material_amounts() const noexcept { return material_amounts_; }

Amount CompiledPhenotype::material_amount(ValueKey value) const noexcept
{
    const auto found = std::ranges::find(material_amounts_, value, &MaterialAmount::value);
    return found == material_amounts_.end() ? 0.0 : found->amount;
}

} // namespace clife::world
