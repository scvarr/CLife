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

struct UnitDefinition final {
    UnitId id;
    std::string symbol;
    std::string description;
};

struct UnitComponent final {
    UnitId unit;
    std::int32_t exponent;
};

struct UnitExpression final {
    std::vector<UnitComponent> components;
};

struct UnitConversionDefinition final {
    UnitConversionId id;
    UnitExpression source_unit;
    Amount source_amount;
    UnitExpression target_unit;
    Amount target_amount;
};

struct ValueDefinition final {
    ValueKey key;
    std::string name;
    std::optional<UnitExpression> unit;
};

struct ObjectCharacteristicDefinition final {
    ObjectCharacteristicId id;
    std::string name;
};

struct GenomeParameterDefinition final {
    ParameterId id;
    std::string name;
    Amount default_value;
};

enum class FunctionValueSourceKind {
    genome_parameter,
    calculation_output,
};

struct FunctionValueSource final {
    FunctionValueSourceKind kind{FunctionValueSourceKind::genome_parameter};
    ParameterId genome_parameter{};
    CalculationId calculation{};
    CalculationPortId calculation_output{};
};

struct FunctionCalculationInputBinding final {
    CalculationPortId input;
    ParameterId genome_parameter;
};

struct FunctionCalculationBinding final {
    CalculationId calculation;
    std::vector<FunctionCalculationInputBinding> inputs;
};

struct FunctionProcessOutputDefinition final {
    ValueKey output;
    FunctionValueSource allocation;
};

struct FunctionProcessDefinition final {
    ValueKey input;
    FunctionValueSource throughput;
    UnitConversionId conversion;
    std::vector<FunctionProcessOutputDefinition> outputs;
};

struct BufferProcessDefinition final {
    ValueKey value;
    FunctionValueSource capacity;
    FunctionValueSource throughput;
    FunctionValueSource leakage;
};

struct MaterialContributionDefinition final {
    ValueKey value;
    FunctionValueSource amount;
};

struct FunctionCharacteristicContributionDefinition final {
    ObjectCharacteristicId characteristic;
    FunctionValueSource amount;
};

struct CalculationInputDefinition final {
    CalculationPortId id;
    std::string name;
};

struct CalculationOutputDefinition final {
    CalculationPortId id;
    std::string name;
    std::string expression_source;
    Expression expression;
};

struct CalculationDefinition final {
    CalculationId id;
    std::string name;
    std::vector<CalculationInputDefinition> inputs;
    std::vector<CalculationOutputDefinition> outputs;
};

struct FunctionTypeDefinition final {
    FunctionTypeId id;
    std::string name;
    std::vector<GenomeParameterDefinition> genome_parameters;
    std::vector<FunctionCalculationBinding> calculations;
    std::optional<FunctionProcessDefinition> process;
    std::optional<BufferProcessDefinition> buffer_process;
    std::vector<MaterialContributionDefinition> material_contributions;
    std::vector<FunctionCharacteristicContributionDefinition> characteristic_contributions;
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

struct BaseObjectCharacteristicDefinition final {
    ObjectCharacteristicId characteristic;
    Amount amount;
};

enum class ObjectConstructionSourceKind {
    base_characteristic,
    function_contribution_sum,
};

struct ObjectConstructionSource final {
    ObjectConstructionSourceKind kind;
    ObjectCharacteristicId characteristic;
};

struct ObjectConstructionInputBinding final {
    CalculationPortId input;
    ObjectConstructionSource source;
};

struct ObjectConstructionOutputBinding final {
    CalculationPortId output;
    ObjectCharacteristicId characteristic;
};

struct ObjectConstructionDefinition final {
    CalculationId calculation;
    std::vector<ObjectConstructionInputBinding> inputs;
    std::vector<ObjectConstructionOutputBinding> outputs;
};

enum class HostChannelDirection {
    input,
    output,
};

struct HostBinding final {
    std::string channel;
    HostChannelDirection direction;
    enum class SourceKind { value, object_characteristic } source_kind{SourceKind::value};
    ValueKey value{};
    ObjectCharacteristicId characteristic{};
};

struct ObjectTemplate final {
    TemplateId id;
    std::string name;
    std::vector<InitialValueDefinition> initial_values;
    std::vector<TemplateMaterialContributionDefinition> material_contributions;
    std::vector<GenomeFunctionInstance> genome;
    std::vector<BaseObjectCharacteristicDefinition> base_characteristics;
    std::vector<HostBinding> host_bindings;
};

struct WorldRuleDefinition final {
    ValueKey source;
    ValueKey end_buffer;
    ValueKey target;
    Amount target_per_source;
};

struct FunctionTypeSnapshot final {
    FunctionTypeId id;
    std::string name;
    std::vector<GenomeParameterDefinition> genome_parameters;
    std::vector<FunctionCalculationBinding> calculations;
    std::optional<FunctionProcessDefinition> process;
    std::optional<BufferProcessDefinition> buffer_process;
    std::vector<MaterialContributionDefinition> material_contributions;
    std::vector<FunctionCharacteristicContributionDefinition> characteristic_contributions;
};

struct CalculationOutputSnapshot final {
    CalculationPortId id;
    std::string name;
    std::string expression_source;
};

struct CalculationSnapshot final {
    CalculationId id;
    std::string name;
    std::vector<CalculationInputDefinition> inputs;
    std::vector<CalculationOutputSnapshot> outputs;
};

struct WorldDefinitionSnapshot final {
    std::uint32_t schema_version{7};
    std::vector<ValueDefinition> values;
    std::vector<UnitDefinition> units;
    std::vector<UnitConversionDefinition> unit_conversions;
    std::vector<ObjectCharacteristicDefinition> object_characteristics;
    std::vector<CalculationSnapshot> calculations;
    std::vector<FunctionTypeSnapshot> function_types;
    std::vector<ObjectTemplate> templates;
    std::vector<WorldRuleDefinition> world_rules;
    std::optional<ObjectConstructionDefinition> object_construction;
    std::uint32_t next_value_key{1};
    std::uint32_t next_template_id{1};
    std::uint32_t next_function_type_id{1};
    std::uint32_t next_parameter_id{1};
    std::uint32_t next_calculation_id{1};
    std::uint32_t next_calculation_port_id{1};
    std::uint32_t next_unit_id{1};
    std::uint32_t next_unit_conversion_id{1};
    std::uint32_t next_object_characteristic_id{1};
};

class WorldDefinition final {
public:
    [[nodiscard]] ValueKey add_value(std::string name);
    [[nodiscard]] UnitId add_unit(std::string symbol, std::string description = {});
    void update_unit(UnitId id, std::string symbol, std::string description);
    void remove_unit(UnitId id);
    [[nodiscard]] UnitConversionId add_unit_conversion(UnitExpression source_unit, Amount source_amount,
                                                        UnitExpression target_unit, Amount target_amount);
    void remove_unit_conversion(UnitConversionId id);
    void set_value_unit(ValueKey value, UnitExpression unit);
    void clear_value_unit(ValueKey value);
    [[nodiscard]] ObjectCharacteristicId add_object_characteristic(std::string name);
    void rename_object_characteristic(ObjectCharacteristicId id, std::string name);
    void remove_object_characteristic(ObjectCharacteristicId id);
    void rename_value(ValueKey key, std::string name);
    void remove_value(ValueKey key);
    void reorder_values(std::span<const ValueKey> order);

    [[nodiscard]] TemplateId add_template(std::string name);
    void rename_template(TemplateId id, std::string name);
    void remove_template(TemplateId id);
    void set_initial_value(TemplateId id, ValueKey value, Amount amount);
    void remove_initial_value(TemplateId id, ValueKey value);
    void set_template_base_characteristic(TemplateId id, ObjectCharacteristicId characteristic, Amount amount);
    void remove_template_base_characteristic(TemplateId id, ObjectCharacteristicId characteristic);
    void set_template_material_contribution(TemplateId id, ValueKey value, Amount amount);

    [[nodiscard]] FunctionTypeId add_function_type(std::string name);
    void rename_function_type(FunctionTypeId id, std::string name);
    void remove_function_type(FunctionTypeId id);
    [[nodiscard]] ParameterId add_genome_parameter(FunctionTypeId type, std::string name, Amount default_value);
    void rename_parameter(FunctionTypeId type, ParameterId parameter, std::string name);
    void set_function_calculation_binding(FunctionTypeId type, FunctionCalculationBinding binding);
    void remove_function_calculation_binding(FunctionTypeId type, CalculationId calculation);
    void set_function_process(FunctionTypeId type, FunctionProcessDefinition process);
    void change_function_process_settings(FunctionTypeId type, ValueKey input, FunctionValueSource throughput,
                                          UnitConversionId conversion);
    void change_function_process_output(FunctionTypeId type, ValueKey existing_output,
                                        FunctionProcessOutputDefinition replacement);
    void add_function_process_output(FunctionTypeId type, FunctionProcessOutputDefinition output);
    void remove_function_process_output(FunctionTypeId type, ValueKey output);
    void remove_function_process(FunctionTypeId type);
    void set_buffer_process(FunctionTypeId type, BufferProcessDefinition process);
    void remove_buffer_process(FunctionTypeId type);
    void set_function_material_contribution(FunctionTypeId type, ValueKey value, FunctionValueSource amount);
    void remove_function_material_contribution(FunctionTypeId type, ValueKey value);
    void set_function_characteristic_contribution(FunctionTypeId type, ObjectCharacteristicId characteristic,
                                                  FunctionValueSource amount);
    void remove_function_characteristic_contribution(FunctionTypeId type, ObjectCharacteristicId characteristic);

    [[nodiscard]] CalculationId add_calculation(std::string name);
    [[nodiscard]] CalculationPortId add_calculation_input(CalculationId calculation, std::string name);
    [[nodiscard]] CalculationPortId add_calculation_output(CalculationId calculation, std::string name,
                                                           std::string_view expression);
    void remove_calculation(CalculationId id);
    void remove_calculation_input(CalculationId calculation, CalculationPortId input);
    void remove_calculation_output(CalculationId calculation, CalculationPortId output);
    void set_calculation_output_expression(CalculationId calculation, CalculationPortId output,
                                           std::string_view expression);

    [[nodiscard]] std::size_t add_genome_function(TemplateId id, FunctionTypeId type);
    void set_genome_parameter(TemplateId id, std::size_t index, ParameterId parameter, Amount value);
    void remove_genome_function(TemplateId id, std::size_t index);

    [[nodiscard]] std::size_t add_world_rule(WorldRuleDefinition rule);
    void change_world_rule(std::size_t index, WorldRuleDefinition rule);
    void remove_world_rule(std::size_t index);

    [[nodiscard]] std::size_t add_host_binding(TemplateId id, HostBinding binding);
    void change_host_binding(TemplateId id, std::size_t index, HostBinding binding);
    void remove_host_binding(TemplateId id, std::size_t index);
    void set_object_construction(ObjectConstructionDefinition construction);
    void remove_object_construction();

    [[nodiscard]] const std::vector<ValueDefinition>& values() const noexcept;
    [[nodiscard]] const std::vector<UnitDefinition>& units() const noexcept;
    [[nodiscard]] const std::vector<UnitConversionDefinition>& unit_conversions() const noexcept;
    [[nodiscard]] const std::vector<ObjectCharacteristicDefinition>& object_characteristics() const noexcept;
    [[nodiscard]] const std::optional<ObjectConstructionDefinition>& object_construction() const noexcept;
    [[nodiscard]] const UnitConversionDefinition& unit_conversion(UnitConversionId id) const;
    [[nodiscard]] const std::vector<ObjectTemplate>& templates() const noexcept;
    [[nodiscard]] const std::vector<FunctionTypeDefinition>& function_types() const noexcept;
    [[nodiscard]] const std::vector<CalculationDefinition>& calculations() const noexcept;
    [[nodiscard]] const std::vector<WorldRuleDefinition>& world_rules() const noexcept;
    [[nodiscard]] const ValueDefinition& value(ValueKey key) const;
    [[nodiscard]] const UnitDefinition& unit(UnitId id) const;
    [[nodiscard]] const ObjectCharacteristicDefinition& object_characteristic(ObjectCharacteristicId id) const;
    [[nodiscard]] const ObjectTemplate& object_template(TemplateId id) const;
    [[nodiscard]] const FunctionTypeDefinition& function_type(FunctionTypeId id) const;
    [[nodiscard]] const CalculationDefinition& calculation(CalculationId id) const;
    [[nodiscard]] WorldDefinitionSnapshot snapshot() const;
    [[nodiscard]] static WorldDefinition from_snapshot(const WorldDefinitionSnapshot& snapshot);

private:
    [[nodiscard]] ObjectTemplate& mutable_template(TemplateId id);
    [[nodiscard]] FunctionTypeDefinition& mutable_function_type(FunctionTypeId id);
    [[nodiscard]] CalculationDefinition& mutable_calculation(CalculationId id);
    [[nodiscard]] bool parameter_belongs_to(const FunctionTypeDefinition& type, ParameterId parameter) const noexcept;
    void validate_function_value_source(const FunctionTypeDefinition& type, const FunctionValueSource& source) const;
    void validate_function_calculation_binding(const FunctionTypeDefinition& type,
                                                const FunctionCalculationBinding& binding) const;
    void validate_unit_expression(const UnitExpression& expression) const;
    void validate_object_construction(const ObjectConstructionDefinition& construction) const;
    void validate_rule(const WorldRuleDefinition& rule, std::size_t ignored_index) const;
    void validate_binding(const ObjectTemplate& object, const HostBinding& binding, std::size_t ignored_index) const;

    std::vector<ValueDefinition> values_;
    std::vector<UnitDefinition> units_;
    std::vector<UnitConversionDefinition> unit_conversions_;
    std::vector<ObjectCharacteristicDefinition> object_characteristics_;
    std::vector<ObjectTemplate> templates_;
    std::vector<FunctionTypeDefinition> function_types_;
    std::vector<CalculationDefinition> calculations_;
    std::vector<WorldRuleDefinition> world_rules_;
    std::optional<ObjectConstructionDefinition> object_construction_;
    std::uint32_t next_value_key_{1};
    std::uint32_t next_template_id_{1};
    std::uint32_t next_function_type_id_{1};
    std::uint32_t next_parameter_id_{1};
    std::uint32_t next_calculation_id_{1};
    std::uint32_t next_calculation_port_id_{1};
    std::uint32_t next_unit_id_{1};
    std::uint32_t next_unit_conversion_id_{1};
    std::uint32_t next_object_characteristic_id_{1};
};

} // namespace clife::world
