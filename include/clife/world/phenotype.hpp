#pragma once

#include <clife/world/definition.hpp>

#include <optional>
#include <span>
#include <vector>

namespace clife::world {

struct CompiledProcessParameters final {
    Amount throughput;
    Amount result_per_input;
};

class CompiledPhenotype;

class CompiledFunctionPhenotype final {
public:
    [[nodiscard]] FunctionTypeId type() const noexcept;
    [[nodiscard]] std::span<const ParameterValue> genome_parameters() const noexcept;
    [[nodiscard]] std::span<const ParameterValue> derived_parameters() const noexcept;
    [[nodiscard]] Amount parameter(ParameterId id) const;
    [[nodiscard]] const std::optional<CompiledProcessParameters>& process_parameters() const noexcept;

private:
    friend class CompiledPhenotype;
    friend CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template);

    FunctionTypeId type_;
    std::vector<ParameterValue> genome_parameters_;
    std::vector<ParameterValue> derived_parameters_;
    std::optional<CompiledProcessParameters> process_parameters_;
};

class CompiledPhenotype final {
public:
    [[nodiscard]] TemplateId source_template() const noexcept;
    [[nodiscard]] std::span<const CompiledFunctionPhenotype> functions() const noexcept;
    [[nodiscard]] const CompiledFunctionPhenotype& function(std::size_t index) const;

private:
    friend CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template);

    TemplateId source_template_;
    std::vector<CompiledFunctionPhenotype> functions_;
};

[[nodiscard]] CompiledPhenotype compile_phenotype(const WorldDefinition& definition, TemplateId source_template);

} // namespace clife::world
