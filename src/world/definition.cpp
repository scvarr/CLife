#include <clife/world/definition.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

namespace clife::world {
namespace {

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

void require_name(std::string_view name, const char* context)
{
    if (name.empty()) {
        throw std::invalid_argument{std::string{context} + " name must not be empty"};
    }
}

[[nodiscard]] std::vector<ParameterName> expression_parameter_names(const FunctionTypeDefinition& type,
                                                                     std::size_t derived_count)
{
    std::vector<ParameterName> names;
    names.reserve(type.genome_parameters.size() + derived_count);
    for (const GenomeParameterDefinition& parameter : type.genome_parameters) {
        names.push_back({.parameter = parameter.id, .name = parameter.name});
    }
    for (std::size_t index = 0; index < derived_count; ++index) {
        const DerivedParameterDefinition& parameter = type.derived_parameters[index];
        names.push_back({.parameter = parameter.id, .name = parameter.name});
    }
    return names;
}

[[nodiscard]] std::vector<ParameterName> calculation_expression_names(const CalculationDefinition& calculation)
{
    std::vector<ParameterName> names;
    names.reserve(calculation.inputs.size() + calculation.outputs.size());
    for (const CalculationInputDefinition& input : calculation.inputs) {
        names.push_back({.parameter = ParameterId{input.id.value}, .name = input.name});
    }
    for (const CalculationOutputDefinition& output : calculation.outputs) {
        names.push_back({.parameter = ParameterId{output.id.value}, .name = output.name});
    }
    return names;
}

[[nodiscard]] bool calculation_port_name_exists(const CalculationDefinition& calculation, std::string_view name)
{
    return std::ranges::any_of(calculation.inputs,
                               [name](const CalculationInputDefinition& input) { return input.name == name; }) ||
           std::ranges::any_of(calculation.outputs,
                               [name](const CalculationOutputDefinition& output) { return output.name == name; });
}

template <typename Entries, typename Project>
void require_unique_snapshot_ids(const Entries& entries, Project project, const char* context)
{
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto id = std::invoke(project, entries[index]);
        if (id.value == 0) {
            throw std::invalid_argument{std::string{context} + " ID must not be zero"};
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (std::invoke(project, entries[other]) == id) {
                throw std::invalid_argument{std::string{context} + " IDs must be unique"};
            }
        }
    }
}

template <typename Entries>
void require_unique_snapshot_names(const Entries& entries, const char* context)
{
    for (std::size_t index = 0; index < entries.size(); ++index) {
        require_name(entries[index].name, context);
        for (std::size_t other = 0; other < index; ++other) {
            if (entries[other].name == entries[index].name) {
                throw std::invalid_argument{std::string{context} + " names must be unique"};
            }
        }
    }
}

template <typename Entries, typename Project>
void require_next_id_is_unused(std::uint32_t next, const Entries& entries, Project project, const char* context)
{
    if (next == 0) {
        return;
    }
    for (const auto& entry : entries) {
        if (std::invoke(project, entry).value >= next) {
            throw std::invalid_argument{std::string{"next "} + context + " ID would reuse an existing ID"};
        }
    }
}

} // namespace

ValueKey WorldDefinition::add_value(std::string name)
{
    require_name(name, "value");
    if (std::ranges::any_of(values_, [&name](const ValueDefinition& entry) { return entry.name == name; })) {
        throw std::invalid_argument{"value name must be unique"};
    }
    if (next_value_key_ == 0) {
        throw std::overflow_error{"ValueKey space exhausted"};
    }
    const ValueKey key{next_value_key_++};
    values_.push_back({.key = key, .name = std::move(name)});
    return key;
}

UnitId WorldDefinition::add_unit(std::string symbol)
{
    require_name(symbol, "unit");
    if (std::ranges::any_of(units_, [&symbol](const UnitDefinition& entry) { return entry.symbol == symbol; })) {
        throw std::invalid_argument{"unit symbol must be unique"};
    }
    if (next_unit_id_ == 0) {
        throw std::overflow_error{"UnitId space exhausted"};
    }
    const UnitId id{next_unit_id_++};
    units_.push_back({.id = id, .symbol = std::move(symbol)});
    return id;
}

UnitConversionId WorldDefinition::add_unit_conversion(UnitExpression source_unit, Amount source_amount,
                                                       UnitExpression target_unit, Amount target_amount)
{
    if (!std::isfinite(source_amount) || source_amount <= 0.0) {
        throw std::invalid_argument{"unit conversion source amount must be finite and positive"};
    }
    if (!std::isfinite(target_amount) || target_amount < 0.0) {
        throw std::invalid_argument{"unit conversion target amount must be finite and non-negative"};
    }
    validate_unit_expression(source_unit);
    validate_unit_expression(target_unit);
    if (next_unit_conversion_id_ == 0) {
        throw std::overflow_error{"UnitConversionId space exhausted"};
    }
    const UnitConversionId id{next_unit_conversion_id_++};
    unit_conversions_.push_back({
        .id = id,
        .source_unit = std::move(source_unit),
        .source_amount = source_amount,
        .target_unit = std::move(target_unit),
        .target_amount = target_amount,
    });
    return id;
}

void WorldDefinition::set_value_unit(ValueKey key, UnitExpression expression)
{
    validate_unit_expression(expression);
    auto& entry = const_cast<ValueDefinition&>(value(key));
    entry.unit = std::move(expression);
}

void WorldDefinition::rename_value(ValueKey key, std::string name)
{
    require_name(name, "value");
    if (std::ranges::any_of(
            values_, [key, &name](const ValueDefinition& entry) { return entry.key != key && entry.name == name; })) {
        throw std::invalid_argument{"value name must be unique"};
    }
    auto& entry = const_cast<ValueDefinition&>(value(key));
    entry.name = std::move(name);
}

void WorldDefinition::remove_value(ValueKey key)
{
    (void)value(key);
    const auto referenced = [key](ValueKey candidate) { return candidate == key; };
    for (const ObjectTemplate& object : templates_) {
        if (std::ranges::any_of(object.initial_values,
                                [&](const InitialValueDefinition& item) { return referenced(item.value); }) ||
            std::ranges::any_of(object.material_contributions,
                                [&](const TemplateMaterialContributionDefinition& item) {
                                    return referenced(item.value);
                                }) ||
            std::ranges::any_of(object.host_bindings,
                                [&](const HostBinding& item) { return referenced(item.value); })) {
            throw std::invalid_argument{"cannot remove a referenced value"};
        }
    }
    if (std::ranges::any_of(world_rules_, [&](const WorldRuleDefinition& rule) {
            return referenced(rule.source) || referenced(rule.end_buffer) || referenced(rule.target);
        })) {
        throw std::invalid_argument{"cannot remove a referenced value"};
    }
    if (std::ranges::any_of(function_types_, [&](const FunctionTypeDefinition& type) {
            return (type.process && (referenced(type.process->input) || referenced(type.process->output))) ||
                   (type.buffer_process && referenced(type.buffer_process->value)) ||
                   std::ranges::any_of(type.material_contributions,
                                       [&](const MaterialContributionDefinition& item) {
                                           return referenced(item.value);
                                       });
        })) {
        throw std::invalid_argument{"cannot remove a referenced value"};
    }
    std::erase_if(values_, [key](const ValueDefinition& entry) { return entry.key == key; });
}

void WorldDefinition::reorder_values(std::span<const ValueKey> order)
{
    if (order.size() != values_.size()) {
        throw std::invalid_argument{"value reorder must contain every value exactly once"};
    }
    std::vector<ValueDefinition> reordered;
    reordered.reserve(values_.size());
    for (const ValueKey key : order) {
        if (std::ranges::any_of(reordered, [key](const ValueDefinition& entry) { return entry.key == key; })) {
            throw std::invalid_argument{"value reorder contains a duplicate key"};
        }
        reordered.push_back(value(key));
    }
    values_ = std::move(reordered);
}

TemplateId WorldDefinition::add_template(std::string name)
{
    require_name(name, "template");
    if (std::ranges::any_of(templates_, [&name](const ObjectTemplate& entry) { return entry.name == name; })) {
        throw std::invalid_argument{"template name must be unique"};
    }
    if (next_template_id_ == 0) {
        throw std::overflow_error{"TemplateId space exhausted"};
    }
    const TemplateId id{next_template_id_++};
    templates_.push_back({.id = id, .name = std::move(name)});
    return id;
}

void WorldDefinition::rename_template(TemplateId id, std::string name)
{
    require_name(name, "template");
    if (std::ranges::any_of(
            templates_, [id, &name](const ObjectTemplate& entry) { return entry.id != id && entry.name == name; })) {
        throw std::invalid_argument{"template name must be unique"};
    }
    mutable_template(id).name = std::move(name);
}

void WorldDefinition::remove_template(TemplateId id)
{
    (void)object_template(id);
    std::erase_if(templates_, [id](const ObjectTemplate& entry) { return entry.id == id; });
}

void WorldDefinition::set_initial_value(TemplateId id, ValueKey key, Amount amount)
{
    (void)value(key);
    if (!std::isfinite(amount)) {
        throw std::invalid_argument{"initial value must be finite"};
    }
    ObjectTemplate& object = mutable_template(id);
    const auto found = std::ranges::find(object.initial_values, key, &InitialValueDefinition::value);
    if (found == object.initial_values.end()) {
        object.initial_values.push_back({.value = key, .amount = amount});
    } else {
        found->amount = amount;
    }
}

void WorldDefinition::remove_initial_value(TemplateId id, ValueKey key)
{
    ObjectTemplate& object = mutable_template(id);
    if (std::erase_if(object.initial_values, [key](const InitialValueDefinition& item) { return item.value == key; }) ==
        0) {
        throw std::invalid_argument{"initial value does not exist"};
    }
}

void WorldDefinition::set_template_material_contribution(TemplateId id, ValueKey key, Amount amount)
{
    (void)value(key);
    if (!std::isfinite(amount) || amount < 0.0) {
        throw std::invalid_argument{"template material contribution must be finite and non-negative"};
    }
    ObjectTemplate& object = mutable_template(id);
    const auto found = std::ranges::find(object.material_contributions, key,
                                         &TemplateMaterialContributionDefinition::value);
    if (found == object.material_contributions.end()) {
        object.material_contributions.push_back({.value = key, .amount = amount});
    } else {
        found->amount = amount;
    }
}

FunctionTypeId WorldDefinition::add_function_type(std::string name)
{
    require_name(name, "function type");
    if (std::ranges::any_of(function_types_,
                            [&name](const FunctionTypeDefinition& entry) { return entry.name == name; })) {
        throw std::invalid_argument{"function type name must be unique"};
    }
    if (next_function_type_id_ == 0) {
        throw std::overflow_error{"FunctionTypeId space exhausted"};
    }
    const FunctionTypeId id{next_function_type_id_++};
    function_types_.push_back({.id = id, .name = std::move(name)});
    return id;
}

void WorldDefinition::rename_function_type(FunctionTypeId id, std::string name)
{
    require_name(name, "function type");
    if (std::ranges::any_of(function_types_, [id, &name](const FunctionTypeDefinition& entry) {
            return entry.id != id && entry.name == name;
        })) {
        throw std::invalid_argument{"function type name must be unique"};
    }
    mutable_function_type(id).name = std::move(name);
}

ParameterId WorldDefinition::add_genome_parameter(FunctionTypeId type_id, std::string name, Amount default_value)
{
    require_name(name, "parameter");
    if (!std::isfinite(default_value)) {
        throw std::invalid_argument{"genome parameter default must be finite"};
    }
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const auto duplicate_name = [&name](const auto& parameter) { return parameter.name == name; };
    if (std::ranges::any_of(type.genome_parameters, duplicate_name) ||
        std::ranges::any_of(type.derived_parameters, duplicate_name)) {
        throw std::invalid_argument{"parameter name must be unique within a function type"};
    }
    if (next_parameter_id_ == 0) {
        throw std::overflow_error{"ParameterId space exhausted"};
    }
    const ParameterId id{next_parameter_id_++};
    type.genome_parameters.push_back({.id = id, .name = std::move(name), .default_value = default_value});
    for (ObjectTemplate& object : templates_) {
        for (GenomeFunctionInstance& function : object.genome) {
            if (function.type == type_id) {
                function.parameters.push_back({.parameter = id, .value = default_value});
            }
        }
    }
    return id;
}

ParameterId WorldDefinition::add_derived_parameter(FunctionTypeId type_id, std::string name,
                                                   std::string_view expression_source)
{
    require_name(name, "parameter");
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const auto duplicate_name = [&name](const auto& parameter) { return parameter.name == name; };
    if (std::ranges::any_of(type.genome_parameters, duplicate_name) ||
        std::ranges::any_of(type.derived_parameters, duplicate_name)) {
        throw std::invalid_argument{"parameter name must be unique within a function type"};
    }
    const std::vector<ParameterName> names = expression_parameter_names(type, type.derived_parameters.size());
    Expression expression = compile_expression(expression_source, names);
    if (next_parameter_id_ == 0) {
        throw std::overflow_error{"ParameterId space exhausted"};
    }
    const ParameterId id{next_parameter_id_++};
    type.derived_parameters.push_back({
        .id = id,
        .name = std::move(name),
        .expression_source = std::string{expression_source},
        .expression = std::move(expression),
    });
    return id;
}

void WorldDefinition::set_derived_parameter_expression(FunctionTypeId type_id, ParameterId parameter_id,
                                                        std::string_view expression_source)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const auto parameter = std::ranges::find(type.derived_parameters, parameter_id, &DerivedParameterDefinition::id);
    if (parameter == type.derived_parameters.end()) {
        throw std::invalid_argument{"ParameterId is not a derived parameter for this function type"};
    }
    const std::size_t index = static_cast<std::size_t>(parameter - type.derived_parameters.begin());
    const std::vector<ParameterName> names = expression_parameter_names(type, index);
    Expression expression = compile_expression(expression_source, names);
    parameter->expression_source = std::string{expression_source};
    parameter->expression = std::move(expression);
}

void WorldDefinition::rename_parameter(FunctionTypeId type_id, ParameterId parameter_id, std::string name)
{
    require_name(name, "parameter");
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const auto duplicate_name = [parameter_id, &name](const auto& parameter) {
        return parameter.id != parameter_id && parameter.name == name;
    };
    if (std::ranges::any_of(type.genome_parameters, duplicate_name) ||
        std::ranges::any_of(type.derived_parameters, duplicate_name)) {
        throw std::invalid_argument{"parameter name must be unique within a function type"};
    }
    const auto genome = std::ranges::find(type.genome_parameters, parameter_id, &GenomeParameterDefinition::id);
    if (genome != type.genome_parameters.end()) {
        genome->name = std::move(name);
        return;
    }
    const auto derived = std::ranges::find(type.derived_parameters, parameter_id, &DerivedParameterDefinition::id);
    if (derived == type.derived_parameters.end()) {
        throw std::invalid_argument{"unknown ParameterId for function type"};
    }
    derived->name = std::move(name);
}

void WorldDefinition::set_function_process(FunctionTypeId type_id, FunctionProcessDefinition process)
{
    (void)value(process.input);
    (void)value(process.output);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (type.buffer_process) {
        throw std::invalid_argument{"function type cannot have both conversion and buffer processes"};
    }
    if (!parameter_belongs_to(type, process.throughput) || !parameter_belongs_to(type, process.result_per_input)) {
        throw std::invalid_argument{"process parameter does not belong to function type"};
    }
    type.process = process;
}

void WorldDefinition::set_buffer_process(FunctionTypeId type_id, BufferProcessDefinition process)
{
    (void)value(process.value);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (type.process) {
        throw std::invalid_argument{"function type cannot have both conversion and buffer processes"};
    }
    if (!parameter_belongs_to(type, process.capacity) || !parameter_belongs_to(type, process.throughput) ||
        !parameter_belongs_to(type, process.leakage)) {
        throw std::invalid_argument{"buffer parameter does not belong to function type"};
    }
    type.buffer_process = process;
}

void WorldDefinition::add_function_material_contribution(FunctionTypeId type_id, ValueKey key,
                                                         std::string_view expression_source)
{
    (void)value(key);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (std::ranges::any_of(type.material_contributions,
                            [key](const MaterialContributionDefinition& item) { return item.value == key; })) {
        throw std::invalid_argument{"function type has more than one contribution for a material value"};
    }
    const std::vector<ParameterName> names = expression_parameter_names(type, type.derived_parameters.size());
    Expression expression = compile_expression(expression_source, names);
    type.material_contributions.push_back({
        .value = key,
        .expression_source = std::string{expression_source},
        .amount = std::move(expression),
    });
}

void WorldDefinition::set_function_material_contribution(FunctionTypeId type_id, ValueKey key,
                                                          std::string_view expression_source)
{
    (void)value(key);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const std::vector<ParameterName> names = expression_parameter_names(type, type.derived_parameters.size());
    Expression expression = compile_expression(expression_source, names);
    const auto contribution =
        std::ranges::find(type.material_contributions, key, &MaterialContributionDefinition::value);
    if (contribution == type.material_contributions.end()) {
        type.material_contributions.push_back({
            .value = key,
            .expression_source = std::string{expression_source},
            .amount = std::move(expression),
        });
        return;
    }
    contribution->expression_source = std::string{expression_source};
    contribution->amount = std::move(expression);
}

void WorldDefinition::remove_function_material_contribution(FunctionTypeId type_id, ValueKey key)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (std::erase_if(type.material_contributions,
                      [key](const MaterialContributionDefinition& item) { return item.value == key; }) == 0) {
        throw std::invalid_argument{"function material contribution does not exist"};
    }
}

CalculationId WorldDefinition::add_calculation(std::string name)
{
    require_name(name, "calculation");
    if (std::ranges::any_of(calculations_, [&name](const CalculationDefinition& calculation) {
            return calculation.name == name;
        })) {
        throw std::invalid_argument{"calculation name must be unique"};
    }
    if (next_calculation_id_ == 0) {
        throw std::overflow_error{"CalculationId space exhausted"};
    }
    const CalculationId id{next_calculation_id_++};
    calculations_.push_back({.id = id, .name = std::move(name)});
    return id;
}

CalculationPortId WorldDefinition::add_calculation_input(CalculationId calculation_id, std::string name)
{
    require_name(name, "calculation port");
    CalculationDefinition& calculation = mutable_calculation(calculation_id);
    if (calculation_port_name_exists(calculation, name)) {
        throw std::invalid_argument{"calculation port name must be unique within a calculation"};
    }
    if (next_calculation_port_id_ == 0) {
        throw std::overflow_error{"CalculationPortId space exhausted"};
    }
    const CalculationPortId id{next_calculation_port_id_++};
    calculation.inputs.push_back({.id = id, .name = std::move(name)});
    return id;
}

CalculationPortId WorldDefinition::add_calculation_output(CalculationId calculation_id, std::string name,
                                                           std::string_view expression_source)
{
    require_name(name, "calculation port");
    CalculationDefinition& calculation = mutable_calculation(calculation_id);
    if (calculation_port_name_exists(calculation, name)) {
        throw std::invalid_argument{"calculation port name must be unique within a calculation"};
    }
    const std::vector<ParameterName> names = calculation_expression_names(calculation);
    Expression expression = compile_expression(expression_source, names);
    if (next_calculation_port_id_ == 0) {
        throw std::overflow_error{"CalculationPortId space exhausted"};
    }
    const CalculationPortId id{next_calculation_port_id_++};
    calculation.outputs.push_back({
        .id = id,
        .name = std::move(name),
        .expression_source = std::string{expression_source},
        .expression = std::move(expression),
    });
    return id;
}

std::size_t WorldDefinition::add_genome_function(TemplateId id, FunctionTypeId type_id)
{
    const FunctionTypeDefinition& type = function_type(type_id);
    GenomeFunctionInstance function{.type = type_id};
    function.parameters.reserve(type.genome_parameters.size());
    for (const GenomeParameterDefinition& parameter : type.genome_parameters) {
        function.parameters.push_back({.parameter = parameter.id, .value = parameter.default_value});
    }
    ObjectTemplate& object = mutable_template(id);
    object.genome.push_back(std::move(function));
    return object.genome.size() - 1;
}

void WorldDefinition::set_genome_parameter(TemplateId id, std::size_t index, ParameterId parameter, Amount value)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{"genome parameter value must be finite"};
    }
    ObjectTemplate& object = mutable_template(id);
    if (index >= object.genome.size()) {
        throw std::out_of_range{"genome function index is out of range"};
    }
    GenomeFunctionInstance& function = object.genome[index];
    const FunctionTypeDefinition& type = function_type(function.type);
    if (std::ranges::none_of(type.genome_parameters, [parameter](const GenomeParameterDefinition& definition) {
            return definition.id == parameter;
        })) {
        throw std::invalid_argument{"ParameterId is not an independent genome parameter for this function"};
    }
    const auto found = std::ranges::find(function.parameters, parameter, &ParameterValue::parameter);
    if (found == function.parameters.end()) {
        throw std::invalid_argument{"genome parameter value is missing"};
    }
    found->value = value;
}

void WorldDefinition::remove_genome_function(TemplateId id, std::size_t index)
{
    ObjectTemplate& object = mutable_template(id);
    if (index >= object.genome.size()) {
        throw std::out_of_range{"genome function index is out of range"};
    }
    object.genome.erase(object.genome.begin() + static_cast<std::ptrdiff_t>(index));
}

std::size_t WorldDefinition::add_world_rule(WorldRuleDefinition rule)
{
    validate_rule(rule, kNoIndex);
    world_rules_.push_back(rule);
    return world_rules_.size() - 1;
}

void WorldDefinition::change_world_rule(std::size_t index, WorldRuleDefinition rule)
{
    if (index >= world_rules_.size()) {
        throw std::out_of_range{"world rule index is out of range"};
    }
    validate_rule(rule, index);
    world_rules_[index] = rule;
}

void WorldDefinition::remove_world_rule(std::size_t index)
{
    if (index >= world_rules_.size()) {
        throw std::out_of_range{"world rule index is out of range"};
    }
    world_rules_.erase(world_rules_.begin() + static_cast<std::ptrdiff_t>(index));
}

std::size_t WorldDefinition::add_host_binding(TemplateId id, HostBinding binding)
{
    ObjectTemplate& object = mutable_template(id);
    validate_binding(object, binding, kNoIndex);
    object.host_bindings.push_back(std::move(binding));
    return object.host_bindings.size() - 1;
}

void WorldDefinition::change_host_binding(TemplateId id, std::size_t index, HostBinding binding)
{
    ObjectTemplate& object = mutable_template(id);
    if (index >= object.host_bindings.size()) {
        throw std::out_of_range{"host binding index is out of range"};
    }
    validate_binding(object, binding, index);
    object.host_bindings[index] = std::move(binding);
}

void WorldDefinition::remove_host_binding(TemplateId id, std::size_t index)
{
    ObjectTemplate& object = mutable_template(id);
    if (index >= object.host_bindings.size()) {
        throw std::out_of_range{"host binding index is out of range"};
    }
    object.host_bindings.erase(object.host_bindings.begin() + static_cast<std::ptrdiff_t>(index));
}

const std::vector<ValueDefinition>& WorldDefinition::values() const noexcept { return values_; }
const std::vector<UnitDefinition>& WorldDefinition::units() const noexcept { return units_; }
const std::vector<UnitConversionDefinition>& WorldDefinition::unit_conversions() const noexcept
{
    return unit_conversions_;
}
const std::vector<ObjectTemplate>& WorldDefinition::templates() const noexcept { return templates_; }
const std::vector<FunctionTypeDefinition>& WorldDefinition::function_types() const noexcept { return function_types_; }

const std::vector<CalculationDefinition>& WorldDefinition::calculations() const noexcept { return calculations_; }
const std::vector<WorldRuleDefinition>& WorldDefinition::world_rules() const noexcept { return world_rules_; }

const ValueDefinition& WorldDefinition::value(ValueKey key) const
{
    const auto found = std::ranges::find(values_, key, &ValueDefinition::key);
    if (found == values_.end()) {
        throw std::invalid_argument{"unknown ValueKey"};
    }
    return *found;
}

const UnitDefinition& WorldDefinition::unit(UnitId id) const
{
    const auto found = std::ranges::find(units_, id, &UnitDefinition::id);
    if (found == units_.end()) {
        throw std::invalid_argument{"unknown UnitId"};
    }
    return *found;
}

const ObjectTemplate& WorldDefinition::object_template(TemplateId id) const
{
    const auto found = std::ranges::find(templates_, id, &ObjectTemplate::id);
    if (found == templates_.end()) {
        throw std::invalid_argument{"unknown TemplateId"};
    }
    return *found;
}

const FunctionTypeDefinition& WorldDefinition::function_type(FunctionTypeId id) const
{
    const auto found = std::ranges::find(function_types_, id, &FunctionTypeDefinition::id);
    if (found == function_types_.end()) {
        throw std::invalid_argument{"unknown FunctionTypeId"};
    }
    return *found;
}

const CalculationDefinition& WorldDefinition::calculation(CalculationId id) const
{
    const auto found = std::ranges::find(calculations_, id, &CalculationDefinition::id);
    if (found == calculations_.end()) {
        throw std::out_of_range{"CalculationId is out of range"};
    }
    return *found;
}

WorldDefinitionSnapshot WorldDefinition::snapshot() const
{
    WorldDefinitionSnapshot result{
        .values = values_,
        .units = units_,
        .unit_conversions = unit_conversions_,
        .templates = templates_,
        .world_rules = world_rules_,
        .next_value_key = next_value_key_,
        .next_template_id = next_template_id_,
        .next_function_type_id = next_function_type_id_,
        .next_parameter_id = next_parameter_id_,
        .next_calculation_id = next_calculation_id_,
        .next_calculation_port_id = next_calculation_port_id_,
        .next_unit_id = next_unit_id_,
        .next_unit_conversion_id = next_unit_conversion_id_,
    };
    result.calculations.reserve(calculations_.size());
    for (const CalculationDefinition& calculation : calculations_) {
        CalculationSnapshot stored{.id = calculation.id, .name = calculation.name, .inputs = calculation.inputs};
        for (const CalculationOutputDefinition& output : calculation.outputs) {
            stored.outputs.push_back({.id = output.id, .name = output.name, .expression_source = output.expression_source});
        }
        result.calculations.push_back(std::move(stored));
    }
    result.function_types.reserve(function_types_.size());
    for (const FunctionTypeDefinition& type : function_types_) {
        FunctionTypeSnapshot stored{
            .id = type.id,
            .name = type.name,
            .genome_parameters = type.genome_parameters,
            .process = type.process,
            .buffer_process = type.buffer_process,
        };
        for (const DerivedParameterDefinition& parameter : type.derived_parameters) {
            stored.derived_parameters.push_back(
                {.id = parameter.id, .name = parameter.name, .expression_source = parameter.expression_source});
        }
        for (const MaterialContributionDefinition& contribution : type.material_contributions) {
            stored.material_contributions.push_back(
                {.value = contribution.value, .expression_source = contribution.expression_source});
        }
        result.function_types.push_back(std::move(stored));
    }
    return result;
}

WorldDefinition WorldDefinition::from_snapshot(const WorldDefinitionSnapshot& source)
{
    if (source.schema_version != 1 && source.schema_version != 2 && source.schema_version != 3) {
        throw std::invalid_argument{"unsupported WorldDefinition snapshot schema version"};
    }
    require_unique_snapshot_ids(source.values, &ValueDefinition::key, "ValueKey");
    require_unique_snapshot_names(source.values, "value");
    if (source.schema_version >= 2) {
        require_unique_snapshot_ids(source.units, &UnitDefinition::id, "UnitId");
        for (std::size_t index = 0; index < source.units.size(); ++index) {
            require_name(source.units[index].symbol, "unit");
            for (std::size_t other = 0; other < index; ++other) {
                if (source.units[other].symbol == source.units[index].symbol) {
                    throw std::invalid_argument{"unit symbols must be unique"};
                }
            }
        }
    }
    require_unique_snapshot_ids(source.templates, &ObjectTemplate::id, "TemplateId");
    require_unique_snapshot_names(source.templates, "template");
    require_unique_snapshot_ids(source.function_types, &FunctionTypeSnapshot::id, "FunctionTypeId");
    require_unique_snapshot_names(source.function_types, "function type");
    require_unique_snapshot_ids(source.calculations, &CalculationSnapshot::id, "CalculationId");
    require_unique_snapshot_names(source.calculations, "calculation");

    WorldDefinition restored;
    restored.values_ = source.values;
    if (source.schema_version >= 2) {
        restored.units_ = source.units;
        for (const ValueDefinition& value : restored.values_) {
            if (value.unit) {
                restored.validate_unit_expression(*value.unit);
            }
        }
    } else {
        for (ValueDefinition& value : restored.values_) {
            value.unit.reset();
        }
    }
    if (source.schema_version == 3) {
        require_unique_snapshot_ids(source.unit_conversions, &UnitConversionDefinition::id, "UnitConversionId");
        for (const UnitConversionDefinition& conversion : source.unit_conversions) {
            if (!std::isfinite(conversion.source_amount) || conversion.source_amount <= 0.0) {
                throw std::invalid_argument{"unit conversion source amount must be finite and positive"};
            }
            if (!std::isfinite(conversion.target_amount) || conversion.target_amount < 0.0) {
                throw std::invalid_argument{"unit conversion target amount must be finite and non-negative"};
            }
            restored.validate_unit_expression(conversion.source_unit);
            restored.validate_unit_expression(conversion.target_unit);
            restored.unit_conversions_.push_back(conversion);
        }
    }

    std::vector<ParameterId> parameter_ids;
    for (const FunctionTypeSnapshot& stored : source.function_types) {
        FunctionTypeDefinition type{.id = stored.id, .name = stored.name};
        require_unique_snapshot_ids(stored.genome_parameters, &GenomeParameterDefinition::id, "ParameterId");
        require_unique_snapshot_names(stored.genome_parameters, "parameter");
        for (const GenomeParameterDefinition& parameter : stored.genome_parameters) {
            if (!std::isfinite(parameter.default_value)) {
                throw std::invalid_argument{"genome parameter default must be finite"};
            }
            if (std::ranges::find(parameter_ids, parameter.id) != parameter_ids.end()) {
                throw std::invalid_argument{"ParameterId IDs must be globally unique"};
            }
            parameter_ids.push_back(parameter.id);
            type.genome_parameters.push_back(parameter);
        }
        for (const DerivedParameterSnapshot& stored_parameter : stored.derived_parameters) {
            require_name(stored_parameter.name, "parameter");
            if (stored_parameter.id.value == 0 ||
                std::ranges::find(parameter_ids, stored_parameter.id) != parameter_ids.end()) {
                throw std::invalid_argument{"ParameterId IDs must be globally unique"};
            }
            if (std::ranges::any_of(type.genome_parameters, [&stored_parameter](const auto& parameter) {
                    return parameter.name == stored_parameter.name;
                }) || std::ranges::any_of(type.derived_parameters, [&stored_parameter](const auto& parameter) {
                    return parameter.name == stored_parameter.name;
                })) {
                throw std::invalid_argument{"parameter name must be unique within a function type"};
            }
            const Expression expression = compile_expression(
                stored_parameter.expression_source, expression_parameter_names(type, type.derived_parameters.size()));
            parameter_ids.push_back(stored_parameter.id);
            type.derived_parameters.push_back({
                .id = stored_parameter.id,
                .name = stored_parameter.name,
                .expression_source = stored_parameter.expression_source,
                .expression = expression,
            });
        }
        if (stored.process && stored.buffer_process) {
            throw std::invalid_argument{"function type cannot have both conversion and buffer processes"};
        }
        if (stored.process) {
            (void)restored.value(stored.process->input);
            (void)restored.value(stored.process->output);
            if (!restored.parameter_belongs_to(type, stored.process->throughput) ||
                !restored.parameter_belongs_to(type, stored.process->result_per_input)) {
                throw std::invalid_argument{"process parameter does not belong to function type"};
            }
            type.process = stored.process;
        }
        if (stored.buffer_process) {
            (void)restored.value(stored.buffer_process->value);
            if (!restored.parameter_belongs_to(type, stored.buffer_process->capacity) ||
                !restored.parameter_belongs_to(type, stored.buffer_process->throughput) ||
                !restored.parameter_belongs_to(type, stored.buffer_process->leakage)) {
                throw std::invalid_argument{"buffer parameter does not belong to function type"};
            }
            type.buffer_process = stored.buffer_process;
        }
        for (const MaterialContributionSnapshot& contribution : stored.material_contributions) {
            (void)restored.value(contribution.value);
            if (std::ranges::any_of(type.material_contributions, [&contribution](const auto& existing) {
                    return existing.value == contribution.value;
                })) {
                throw std::invalid_argument{"function type has more than one contribution for a material value"};
            }
            type.material_contributions.push_back({
                .value = contribution.value,
                .expression_source = contribution.expression_source,
                .amount = compile_expression(contribution.expression_source,
                                             expression_parameter_names(type, type.derived_parameters.size())),
            });
        }
        restored.function_types_.push_back(std::move(type));
    }

    std::vector<CalculationPortId> port_ids;
    for (const CalculationSnapshot& stored : source.calculations) {
        CalculationDefinition calculation{.id = stored.id, .name = stored.name};
        for (const CalculationInputDefinition& input : stored.inputs) {
            require_name(input.name, "calculation port");
            if (input.id.value == 0 || std::ranges::find(port_ids, input.id) != port_ids.end() ||
                calculation_port_name_exists(calculation, input.name)) {
                throw std::invalid_argument{"CalculationPortId or name is not unique"};
            }
            port_ids.push_back(input.id);
            calculation.inputs.push_back(input);
        }
        for (const CalculationOutputSnapshot& output : stored.outputs) {
            require_name(output.name, "calculation port");
            if (output.id.value == 0 || std::ranges::find(port_ids, output.id) != port_ids.end() ||
                calculation_port_name_exists(calculation, output.name)) {
                throw std::invalid_argument{"CalculationPortId or name is not unique"};
            }
            const Expression expression = compile_expression(output.expression_source, calculation_expression_names(calculation));
            port_ids.push_back(output.id);
            calculation.outputs.push_back({
                .id = output.id,
                .name = output.name,
                .expression_source = output.expression_source,
                .expression = expression,
            });
        }
        restored.calculations_.push_back(std::move(calculation));
    }

    for (const ObjectTemplate& stored : source.templates) {
        ObjectTemplate object{.id = stored.id, .name = stored.name};
        for (const InitialValueDefinition& initial : stored.initial_values) {
            (void)restored.value(initial.value);
            if (!std::isfinite(initial.amount) ||
                std::ranges::any_of(object.initial_values, [&initial](const auto& existing) {
                    return existing.value == initial.value;
                })) {
                throw std::invalid_argument{"initial value must be finite and unique"};
            }
            object.initial_values.push_back(initial);
        }
        for (const TemplateMaterialContributionDefinition& contribution : stored.material_contributions) {
            (void)restored.value(contribution.value);
            if (!std::isfinite(contribution.amount) || contribution.amount < 0.0 ||
                std::ranges::any_of(object.material_contributions, [&contribution](const auto& existing) {
                    return existing.value == contribution.value;
                })) {
                throw std::invalid_argument{"template material contribution must be finite, non-negative, and unique"};
            }
            object.material_contributions.push_back(contribution);
        }
        for (const GenomeFunctionInstance& function : stored.genome) {
            const FunctionTypeDefinition& type = restored.function_type(function.type);
            if (function.parameters.size() != type.genome_parameters.size()) {
                throw std::invalid_argument{"genome function parameters do not match its function type"};
            }
            for (const GenomeParameterDefinition& parameter : type.genome_parameters) {
                const auto found = std::ranges::find(function.parameters, parameter.id, &ParameterValue::parameter);
                if (found == function.parameters.end() || !std::isfinite(found->value) ||
                    std::ranges::count(function.parameters, parameter.id, &ParameterValue::parameter) != 1) {
                    throw std::invalid_argument{"genome function parameter is invalid"};
                }
            }
            object.genome.push_back(function);
        }
        for (const HostBinding& binding : stored.host_bindings) {
            if (binding.direction != HostChannelDirection::input && binding.direction != HostChannelDirection::output) {
                throw std::invalid_argument{"host binding direction is invalid"};
            }
            restored.validate_binding(object, binding, kNoIndex);
            object.host_bindings.push_back(binding);
        }
        restored.templates_.push_back(std::move(object));
    }
    for (const WorldRuleDefinition& rule : source.world_rules) {
        restored.validate_rule(rule, kNoIndex);
        restored.world_rules_.push_back(rule);
    }

    require_next_id_is_unused(source.next_value_key, source.values, &ValueDefinition::key, "ValueKey");
    require_next_id_is_unused(source.next_template_id, source.templates, &ObjectTemplate::id, "TemplateId");
    require_next_id_is_unused(source.next_function_type_id, source.function_types, &FunctionTypeSnapshot::id,
                              "FunctionTypeId");
    require_next_id_is_unused(source.next_parameter_id, parameter_ids, [](ParameterId id) { return id; },
                              "ParameterId");
    require_next_id_is_unused(source.next_calculation_id, source.calculations, &CalculationSnapshot::id,
                              "CalculationId");
    require_next_id_is_unused(source.next_calculation_port_id, port_ids, [](CalculationPortId id) { return id; },
                              "CalculationPortId");
    if (source.schema_version >= 2) {
        require_next_id_is_unused(source.next_unit_id, source.units, &UnitDefinition::id, "UnitId");
    }
    if (source.schema_version == 3) {
        require_next_id_is_unused(source.next_unit_conversion_id, source.unit_conversions,
                                  &UnitConversionDefinition::id, "UnitConversionId");
    }
    restored.next_value_key_ = source.next_value_key;
    restored.next_template_id_ = source.next_template_id;
    restored.next_function_type_id_ = source.next_function_type_id;
    restored.next_parameter_id_ = source.next_parameter_id;
    restored.next_calculation_id_ = source.next_calculation_id;
    restored.next_calculation_port_id_ = source.next_calculation_port_id;
    restored.next_unit_id_ = source.schema_version >= 2 ? source.next_unit_id : 1;
    restored.next_unit_conversion_id_ = source.schema_version == 3 ? source.next_unit_conversion_id : 1;
    return restored;
}

ObjectTemplate& WorldDefinition::mutable_template(TemplateId id)
{
    return const_cast<ObjectTemplate&>(std::as_const(*this).object_template(id));
}

FunctionTypeDefinition& WorldDefinition::mutable_function_type(FunctionTypeId id)
{
    return const_cast<FunctionTypeDefinition&>(std::as_const(*this).function_type(id));
}

CalculationDefinition& WorldDefinition::mutable_calculation(CalculationId id)
{
    return const_cast<CalculationDefinition&>(std::as_const(*this).calculation(id));
}

bool WorldDefinition::parameter_belongs_to(const FunctionTypeDefinition& type, ParameterId parameter) const noexcept
{
    return std::ranges::any_of(type.genome_parameters,
                               [parameter](const GenomeParameterDefinition& item) { return item.id == parameter; }) ||
           std::ranges::any_of(type.derived_parameters,
                               [parameter](const DerivedParameterDefinition& item) { return item.id == parameter; });
}

void WorldDefinition::validate_unit_expression(const UnitExpression& expression) const
{
    for (std::size_t index = 0; index < expression.components.size(); ++index) {
        const UnitComponent& component = expression.components[index];
        (void)unit(component.unit);
        if (component.exponent == 0) {
            throw std::invalid_argument{"unit expression exponent must not be zero"};
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (expression.components[other].unit == component.unit) {
                throw std::invalid_argument{"unit expression must not contain a duplicate UnitId"};
            }
        }
    }
}

void WorldDefinition::validate_rule(const WorldRuleDefinition& rule, std::size_t ignored_index) const
{
    (void)value(rule.source);
    (void)value(rule.end_buffer);
    (void)value(rule.target);
    if (!std::isfinite(rule.target_per_source)) {
        throw std::invalid_argument{"world rule target_per_source must be finite"};
    }
    for (std::size_t index = 0; index < world_rules_.size(); ++index) {
        if (index != ignored_index && world_rules_[index].source == rule.source) {
            throw std::invalid_argument{"world value has more than one consuming rule"};
        }
    }
}

void WorldDefinition::validate_binding(const ObjectTemplate& object, const HostBinding& binding,
                                       std::size_t ignored_index) const
{
    require_name(binding.channel, "host channel");
    (void)value(binding.value);
    for (std::size_t index = 0; index < object.host_bindings.size(); ++index) {
        const HostBinding& existing = object.host_bindings[index];
        if (index != ignored_index && existing.direction == binding.direction && existing.channel == binding.channel) {
            throw std::invalid_argument{"host channel is bound more than once for its direction"};
        }
        if (index != ignored_index && binding.direction == HostChannelDirection::input &&
            existing.direction == HostChannelDirection::input && existing.value == binding.value) {
            throw std::invalid_argument{"world value has more than one host input binding"};
        }
    }
}

} // namespace clife::world
