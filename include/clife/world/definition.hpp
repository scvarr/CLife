#pragma once

#include <clife/core/calculator.hpp>
#include <clife/world/expression.hpp>
#include <clife/world/identities.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace clife::world {

struct ValueDefinition final {
    ValueKey key;
    std::string name;
};

struct GenomeParameterDefinition final {
    ParameterId id;
    std::string name;
    Amount default_value;
};

struct DerivedParameterDefinition final {
    ParameterId id;
    std::string name;
    std::string expression_source;
    Expression expression;
};

struct FunctionProcessDefinition final {
    ValueKey input;
    ValueKey output;
    ParameterId throughput;
    ParameterId result_per_input;
};

struct BufferProcessDefinition final {
    ValueKey value;
    ParameterId capacity;
    ParameterId throughput;
    ParameterId leakage;
};

struct MaterialContributionDefinition final {
    ValueKey value;
    std::string expression_source;
    Expression amount;
};

struct FunctionTypeDefinition final {
    FunctionTypeId id;
    std::string name;
    std::vector<GenomeParameterDefinition> genome_parameters;
    std::vector<DerivedParameterDefinition> derived_parameters;
    std::optional<FunctionProcessDefinition> process;
    std::optional<BufferProcessDefinition> buffer_process;
    std::vector<MaterialContributionDefinition> material_contributions;
};

struct GenomeFunctionInstance final {
    FunctionTypeId type;
    std::vector<ParameterValue> parameters;
};

struct InitialValueDefinition final {
    ValueKey value;
    Amount amount;
};

struct TemplateMaterialContributionDefinition final {
    ValueKey value;
    Amount amount;
};

enum class HostChannelDirection {
    input,
    output,
};

struct HostBinding final {
    std::string channel;
    HostChannelDirection direction;
    ValueKey value;
};

struct ObjectTemplate final {
    TemplateId id;
    std::string name;
    std::vector<InitialValueDefinition> initial_values;
    std::vector<TemplateMaterialContributionDefinition> material_contributions;
    std::vector<GenomeFunctionInstance> genome;
    std::vector<HostBinding> host_bindings;
};

struct WorldRuleDefinition final {
    ValueKey source;
    ValueKey end_buffer;
    ValueKey target;
    Amount target_per_source;
};

class WorldDefinition final {
public:
    [[nodiscard]] ValueKey add_value(std::string name);
    void rename_value(ValueKey key, std::string name);
    void remove_value(ValueKey key);
    void reorder_values(std::span<const ValueKey> order);

    [[nodiscard]] TemplateId add_template(std::string name);
    void rename_template(TemplateId id, std::string name);
    void remove_template(TemplateId id);
    void set_initial_value(TemplateId id, ValueKey value, Amount amount);
    void remove_initial_value(TemplateId id, ValueKey value);
    void set_template_material_contribution(TemplateId id, ValueKey value, Amount amount);

    [[nodiscard]] FunctionTypeId add_function_type(std::string name);
    void rename_function_type(FunctionTypeId id, std::string name);
    [[nodiscard]] ParameterId add_genome_parameter(FunctionTypeId type, std::string name, Amount default_value);
    [[nodiscard]] ParameterId add_derived_parameter(FunctionTypeId type, std::string name, std::string_view expression);
    void set_derived_parameter_expression(FunctionTypeId type, ParameterId parameter, std::string_view expression);
    void rename_parameter(FunctionTypeId type, ParameterId parameter, std::string name);
    void set_function_process(FunctionTypeId type, FunctionProcessDefinition process);
    void set_buffer_process(FunctionTypeId type, BufferProcessDefinition process);
    void add_function_material_contribution(FunctionTypeId type, ValueKey value, std::string_view expression);
    void set_function_material_contribution(FunctionTypeId type, ValueKey value, std::string_view expression);
    void remove_function_material_contribution(FunctionTypeId type, ValueKey value);

    [[nodiscard]] std::size_t add_genome_function(TemplateId id, FunctionTypeId type);
    void set_genome_parameter(TemplateId id, std::size_t index, ParameterId parameter, Amount value);
    void remove_genome_function(TemplateId id, std::size_t index);

    [[nodiscard]] std::size_t add_world_rule(WorldRuleDefinition rule);
    void change_world_rule(std::size_t index, WorldRuleDefinition rule);
    void remove_world_rule(std::size_t index);

    [[nodiscard]] std::size_t add_host_binding(TemplateId id, HostBinding binding);
    void change_host_binding(TemplateId id, std::size_t index, HostBinding binding);
    void remove_host_binding(TemplateId id, std::size_t index);

    [[nodiscard]] const std::vector<ValueDefinition>& values() const noexcept;
    [[nodiscard]] const std::vector<ObjectTemplate>& templates() const noexcept;
    [[nodiscard]] const std::vector<FunctionTypeDefinition>& function_types() const noexcept;
    [[nodiscard]] const std::vector<WorldRuleDefinition>& world_rules() const noexcept;
    [[nodiscard]] const ValueDefinition& value(ValueKey key) const;
    [[nodiscard]] const ObjectTemplate& object_template(TemplateId id) const;
    [[nodiscard]] const FunctionTypeDefinition& function_type(FunctionTypeId id) const;

private:
    [[nodiscard]] ObjectTemplate& mutable_template(TemplateId id);
    [[nodiscard]] FunctionTypeDefinition& mutable_function_type(FunctionTypeId id);
    [[nodiscard]] bool parameter_belongs_to(const FunctionTypeDefinition& type, ParameterId parameter) const noexcept;
    void validate_rule(const WorldRuleDefinition& rule, std::size_t ignored_index) const;
    void validate_binding(const ObjectTemplate& object, const HostBinding& binding, std::size_t ignored_index) const;

    std::vector<ValueDefinition> values_;
    std::vector<ObjectTemplate> templates_;
    std::vector<FunctionTypeDefinition> function_types_;
    std::vector<WorldRuleDefinition> world_rules_;
    std::uint32_t next_value_key_{1};
    std::uint32_t next_template_id_{1};
    std::uint32_t next_function_type_id_{1};
    std::uint32_t next_parameter_id_{1};
};

} // namespace clife::world
