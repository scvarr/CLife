#pragma once

#include <clife/world/definition.hpp>

#include <optional>
#include <span>
#include <vector>

namespace clife::world {

struct CompiledProcessOutput final {
    ValueKey output;
    Amount result_per_input;
};

struct CompiledProcessParameters final {
    Amount throughput;
    std::vector<CompiledProcessOutput> outputs;
};

struct CompiledBufferParameters final {
    Amount capacity;
    Amount throughput;
    Amount leakage;
};

struct MaterialAmount final {
    ValueKey value;
    Amount amount;
};

struct CompiledCalculationOutputValue final {
    CalculationId calculation;
    CalculationPortId output;
    Amount value;
};

struct ObjectCharacteristicAmount final {
    ObjectCharacteristicId characteristic;
    Amount amount;
};

class CompiledPhenotype;

class CompiledFunctionPhenotype final {
public:
    [[nodiscard]] FunctionTypeId type() const noexcept;
    [[nodiscard]] std::span<const ParameterValue> genome_parameters() const noexcept;
    [[nodiscard]] Amount parameter(ParameterId id) const;
    [[nodiscard]] std::span<const CompiledCalculationOutputValue> calculation_outputs() const noexcept;
    [[nodiscard]] Amount calculation_output(CalculationId calculation, CalculationPortId output) const;
    [[nodiscard]] const std::optional<CompiledProcessParameters>& process_parameters() const noexcept;
    [[nodiscard]] const std::optional<CompiledBufferParameters>& buffer_parameters() const noexcept;

private:
    friend class CompiledPhenotype;
    friend CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template);

    FunctionTypeId type_;
    std::vector<ParameterValue> genome_parameters_;
    std::vector<CompiledCalculationOutputValue> calculation_outputs_;
    std::optional<CompiledProcessParameters> process_parameters_;
    std::optional<CompiledBufferParameters> buffer_parameters_;
};

class CompiledPhenotype final {
public:
    [[nodiscard]] TemplateId source_template() const noexcept;
    [[nodiscard]] std::span<const CompiledFunctionPhenotype> functions() const noexcept;
    [[nodiscard]] const CompiledFunctionPhenotype& function(std::size_t index) const;
    [[nodiscard]] std::span<const MaterialAmount> material_amounts() const noexcept;
    [[nodiscard]] Amount material_amount(ValueKey value) const noexcept;
    [[nodiscard]] std::span<const ObjectCharacteristicAmount> characteristics() const noexcept;
    [[nodiscard]] Amount characteristic(ObjectCharacteristicId characteristic) const noexcept;
    [[nodiscard]] Amount function_contribution_sum(ObjectCharacteristicId characteristic) const noexcept;

private:
    friend CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template);

    TemplateId source_template_;
    std::vector<CompiledFunctionPhenotype> functions_;
    std::vector<MaterialAmount> material_amounts_;
    std::vector<ObjectCharacteristicAmount> characteristics_;
    std::vector<ObjectCharacteristicAmount> function_contribution_sums_;
};

[[nodiscard]] CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template);

} // namespace clife::world
