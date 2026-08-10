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

void recompile_calculation_outputs(CalculationDefinition& calculation)
{
    std::vector<ParameterName> names;
    names.reserve(calculation.inputs.size() + calculation.outputs.size());
    for (const CalculationInputDefinition& input : calculation.inputs) {
        names.push_back({.parameter = ParameterId{input.id.value}, .name = input.name});
    }
    for (CalculationOutputDefinition& output : calculation.outputs) {
        output.expression = compile_expression(output.expression_source, names);
        names.push_back({.parameter = ParameterId{output.id.value}, .name = output.name});
    }
}

[[nodiscard]] bool references_calculation_output(const FunctionValueSource& source, CalculationId calculation,
                                                 CalculationPortId output)
{
    return source.kind == FunctionValueSourceKind::calculation_output && source.calculation == calculation &&
           source.calculation_output == output;
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

UnitId WorldDefinition::add_unit(std::string symbol, std::string description)
{
    require_name(symbol, "unit");
    if (std::ranges::any_of(units_, [&symbol](const UnitDefinition& entry) { return entry.symbol == symbol; })) {
        throw std::invalid_argument{"unit symbol must be unique"};
    }
    if (next_unit_id_ == 0) {
        throw std::overflow_error{"UnitId space exhausted"};
    }
    const UnitId id{next_unit_id_++};
    units_.push_back({.id = id, .symbol = std::move(symbol), .description = std::move(description)});
    return id;
}

void WorldDefinition::update_unit(UnitId id, std::string symbol, std::string description)
{
    require_name(symbol, "unit");
    if (std::ranges::any_of(units_, [id, &symbol](const auto& item) { return item.id != id && item.symbol == symbol; })) {
        throw std::invalid_argument{"unit symbol must be unique"};
    }
    auto& unit_definition = const_cast<UnitDefinition&>(unit(id));
    unit_definition.symbol = std::move(symbol);
    unit_definition.description = std::move(description);
}

void WorldDefinition::remove_unit(UnitId id)
{
    (void)unit(id);
    const auto used = [id](const UnitExpression& expression) {
        return std::ranges::any_of(expression.components, [id](const UnitComponent& component) { return component.unit == id; });
    };
    if (std::ranges::any_of(values_, [&](const ValueDefinition& value) { return value.unit && used(*value.unit); }) ||
        std::ranges::any_of(unit_conversions_, [&](const UnitConversionDefinition& conversion) {
            return used(conversion.source_unit) || used(conversion.target_unit);
        })) {
        throw std::invalid_argument{"cannot remove a referenced unit"};
    }
    std::erase_if(units_, [id](const UnitDefinition& item) { return item.id == id; });
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

ObjectCharacteristicId WorldDefinition::add_object_characteristic(std::string name)
{
    require_name(name, "object characteristic");
    if (std::ranges::any_of(object_characteristics_, [&name](const auto& item) { return item.name == name; })) {
        throw std::invalid_argument{"object characteristic name must be unique"};
    }
    if (next_object_characteristic_id_ == 0) {
        throw std::overflow_error{"ObjectCharacteristicId space exhausted"};
    }
    const ObjectCharacteristicId id{next_object_characteristic_id_++};
    object_characteristics_.push_back({.id = id, .name = std::move(name)});
    return id;
}

void WorldDefinition::rename_object_characteristic(ObjectCharacteristicId id, std::string name)
{
    require_name(name, "object characteristic");
    if (std::ranges::any_of(object_characteristics_, [id, &name](const auto& item) {
            return item.id != id && item.name == name;
        })) {
        throw std::invalid_argument{"object characteristic name must be unique"};
    }
    auto& item = const_cast<ObjectCharacteristicDefinition&>(object_characteristic(id));
    item.name = std::move(name);
}

void WorldDefinition::remove_object_characteristic(ObjectCharacteristicId id)
{
    (void)object_characteristic(id);
    const auto referenced = [id](ObjectCharacteristicId candidate) { return candidate == id; };
    if ((object_construction_ && (std::ranges::any_of(object_construction_->inputs, [&](const auto& item) {
             return referenced(item.source.characteristic);
         }) || std::ranges::any_of(object_construction_->outputs, [&](const auto& item) {
             return referenced(item.characteristic);
         }))) ||
        std::ranges::any_of(templates_, [&](const ObjectTemplate& object) {
            return std::ranges::any_of(object.base_characteristics, [&](const auto& item) {
                return referenced(item.characteristic);
            });
        }) || std::ranges::any_of(function_types_, [&](const FunctionTypeDefinition& type) {
            return std::ranges::any_of(type.characteristic_contributions, [&](const auto& item) {
                return referenced(item.characteristic);
            });
        }) || std::ranges::any_of(templates_, [&](const ObjectTemplate& object) {
            return std::ranges::any_of(object.host_bindings, [&](const HostBinding& item) {
                return item.source_kind == HostBinding::SourceKind::object_characteristic &&
                       referenced(item.characteristic);
            });
        })) {
        throw std::invalid_argument{"cannot remove a referenced object characteristic"};
    }
    std::erase_if(object_characteristics_, [id](const auto& item) { return item.id == id; });
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
            return (type.process && (referenced(type.process->input) ||
                                     std::ranges::any_of(type.process->outputs,
                                                         [&](const FunctionProcessOutputDefinition& output) {
                                                             return referenced(output.output);
                                                         }))) ||
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

void WorldDefinition::set_template_base_characteristic(TemplateId id, ObjectCharacteristicId characteristic,
                                                        Amount amount)
{
    (void)object_characteristic(characteristic);
    if (!std::isfinite(amount)) {
        throw std::invalid_argument{"base characteristic amount must be finite"};
    }
    ObjectTemplate& object = mutable_template(id);
    const auto found = std::ranges::find(object.base_characteristics, characteristic,
                                         &BaseObjectCharacteristicDefinition::characteristic);
    if (found == object.base_characteristics.end()) {
        object.base_characteristics.push_back({.characteristic = characteristic, .amount = amount});
    } else {
        found->amount = amount;
    }
}

void WorldDefinition::remove_template_base_characteristic(TemplateId id, ObjectCharacteristicId characteristic)
{
    ObjectTemplate& object = mutable_template(id);
    if (std::erase_if(object.base_characteristics, [characteristic](const auto& item) {
            return item.characteristic == characteristic;
        }) == 0) {
        throw std::invalid_argument{"template base characteristic does not exist"};
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

void WorldDefinition::remove_function_type(FunctionTypeId id)
{
    (void)function_type(id);
    if (std::ranges::any_of(templates_, [id](const ObjectTemplate& object) {
            return std::ranges::any_of(object.genome,
                                       [id](const GenomeFunctionInstance& function) { return function.type == id; });
        })) {
        throw std::invalid_argument{"cannot remove a function type referenced by a template genome"};
    }
    std::erase_if(function_types_, [id](const FunctionTypeDefinition& type) { return type.id == id; });
}

ParameterId WorldDefinition::add_genome_parameter(FunctionTypeId type_id, std::string name, Amount default_value)
{
    require_name(name, "parameter");
    if (!std::isfinite(default_value)) {
        throw std::invalid_argument{"genome parameter default must be finite"};
    }
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (std::ranges::any_of(type.genome_parameters,
                            [&name](const auto& parameter) { return parameter.name == name; })) {
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

void WorldDefinition::rename_parameter(FunctionTypeId type_id, ParameterId parameter_id, std::string name)
{
    require_name(name, "parameter");
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const auto duplicate_name = [parameter_id, &name](const auto& parameter) {
        return parameter.id != parameter_id && parameter.name == name;
    };
    if (std::ranges::any_of(type.genome_parameters, duplicate_name)) {
        throw std::invalid_argument{"parameter name must be unique within a function type"};
    }
    const auto genome = std::ranges::find(type.genome_parameters, parameter_id, &GenomeParameterDefinition::id);
    if (genome == type.genome_parameters.end()) {
        throw std::invalid_argument{"unknown ParameterId for function type"};
    }
    genome->name = std::move(name);
}

void WorldDefinition::set_function_calculation_binding(FunctionTypeId type_id, FunctionCalculationBinding binding)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    validate_function_calculation_binding(type, binding);
    const auto existing = std::ranges::find(type.calculations, binding.calculation,
                                            &FunctionCalculationBinding::calculation);
    if (existing == type.calculations.end()) {
        type.calculations.push_back(std::move(binding));
    } else {
        *existing = std::move(binding);
    }
}

void WorldDefinition::remove_function_calculation_binding(FunctionTypeId type_id, CalculationId calculation_id)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    const auto referenced = [calculation_id](const FunctionValueSource& source) {
        return source.kind == FunctionValueSourceKind::calculation_output && source.calculation == calculation_id;
    };
    if ((type.process && (referenced(type.process->throughput) ||
                          std::ranges::any_of(type.process->outputs, [&](const auto& output) {
                              return referenced(output.allocation);
                          }))) ||
        (type.buffer_process && (referenced(type.buffer_process->capacity) || referenced(type.buffer_process->throughput) ||
                                 referenced(type.buffer_process->leakage))) ||
        std::ranges::any_of(type.material_contributions,
                            [&](const auto& contribution) { return referenced(contribution.amount); })) {
        throw std::invalid_argument{"cannot remove a calculation binding referenced by the function type"};
    }
    if (std::erase_if(type.calculations, [calculation_id](const auto& binding) {
            return binding.calculation == calculation_id;
        }) == 0) {
        throw std::invalid_argument{"function calculation binding does not exist"};
    }
}

void WorldDefinition::set_function_process(FunctionTypeId type_id, FunctionProcessDefinition process)
{
    (void)value(process.input);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (type.buffer_process) {
        throw std::invalid_argument{"function type cannot have both conversion and buffer processes"};
    }
    validate_function_value_source(type, process.throughput);
    (void)unit_conversion(process.conversion);
    if (process.outputs.empty()) {
        throw std::invalid_argument{"function process must contain at least one output"};
    }
    for (std::size_t index = 0; index < process.outputs.size(); ++index) {
        const FunctionProcessOutputDefinition& output = process.outputs[index];
        (void)value(output.output);
        validate_function_value_source(type, output.allocation);
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (process.outputs[previous].output == output.output) {
                throw std::invalid_argument{"function process output ValueKey is duplicated"};
            }
        }
    }
    type.process = std::move(process);
}

void WorldDefinition::add_function_process_output(FunctionTypeId type_id, FunctionProcessOutputDefinition output)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (!type.process) {
        throw std::invalid_argument{"function type does not have a process"};
    }
    FunctionProcessDefinition process = *type.process;
    process.outputs.push_back(output);
    set_function_process(type_id, std::move(process));
}

void WorldDefinition::change_function_process_settings(FunctionTypeId type_id, ValueKey input,
                                                        FunctionValueSource throughput,
                                                        UnitConversionId conversion)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (!type.process) {
        throw std::invalid_argument{"function type does not have a process"};
    }
    FunctionProcessDefinition process = *type.process;
    process.input = input;
    process.throughput = throughput;
    process.conversion = conversion;
    set_function_process(type_id, std::move(process));
}

void WorldDefinition::change_function_process_output(FunctionTypeId type_id, ValueKey existing_output,
                                                      FunctionProcessOutputDefinition replacement)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (!type.process) {
        throw std::invalid_argument{"function type does not have a process"};
    }
    FunctionProcessDefinition process = *type.process;
    const auto found = std::ranges::find(process.outputs, existing_output, &FunctionProcessOutputDefinition::output);
    if (found == process.outputs.end()) {
        throw std::invalid_argument{"function process output does not exist"};
    }
    *found = replacement;
    set_function_process(type_id, std::move(process));
}

void WorldDefinition::remove_function_process_output(FunctionTypeId type_id, ValueKey output)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (!type.process) {
        throw std::invalid_argument{"function type does not have a process"};
    }
    FunctionProcessDefinition process = *type.process;
    const auto found = std::ranges::find(process.outputs, output, &FunctionProcessOutputDefinition::output);
    if (found == process.outputs.end()) {
        throw std::invalid_argument{"function process output does not exist"};
    }
    process.outputs.erase(found);
    set_function_process(type_id, std::move(process));
}

void WorldDefinition::remove_function_process(FunctionTypeId type_id)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (!type.process) {
        throw std::invalid_argument{"function type does not have a process"};
    }
    type.process.reset();
}

void WorldDefinition::set_buffer_process(FunctionTypeId type_id, BufferProcessDefinition process)
{
    (void)value(process.value);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (type.process) {
        throw std::invalid_argument{"function type cannot have both conversion and buffer processes"};
    }
    validate_function_value_source(type, process.capacity);
    validate_function_value_source(type, process.throughput);
    validate_function_value_source(type, process.leakage);
    type.buffer_process = process;
}

void WorldDefinition::remove_buffer_process(FunctionTypeId type_id)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (!type.buffer_process) {
        throw std::invalid_argument{"function type does not have a buffer process"};
    }
    type.buffer_process.reset();
}

void WorldDefinition::set_function_material_contribution(FunctionTypeId type_id, ValueKey key,
                                                          FunctionValueSource amount)
{
    (void)value(key);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    validate_function_value_source(type, amount);
    const auto contribution =
        std::ranges::find(type.material_contributions, key, &MaterialContributionDefinition::value);
    if (contribution == type.material_contributions.end()) {
        type.material_contributions.push_back({
            .value = key,
            .amount = amount,
        });
        return;
    }
    contribution->amount = amount;
}

void WorldDefinition::remove_function_material_contribution(FunctionTypeId type_id, ValueKey key)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (std::erase_if(type.material_contributions,
                      [key](const MaterialContributionDefinition& item) { return item.value == key; }) == 0) {
        throw std::invalid_argument{"function material contribution does not exist"};
    }
}

void WorldDefinition::set_function_characteristic_contribution(FunctionTypeId type_id,
                                                                ObjectCharacteristicId characteristic,
                                                                FunctionValueSource amount)
{
    (void)object_characteristic(characteristic);
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    validate_function_value_source(type, amount);
    const auto found = std::ranges::find(type.characteristic_contributions, characteristic,
                                         &FunctionCharacteristicContributionDefinition::characteristic);
    if (found == type.characteristic_contributions.end()) {
        type.characteristic_contributions.push_back({.characteristic = characteristic, .amount = amount});
    } else {
        found->amount = amount;
    }
}

void WorldDefinition::remove_function_characteristic_contribution(FunctionTypeId type_id,
                                                                   ObjectCharacteristicId characteristic)
{
    FunctionTypeDefinition& type = mutable_function_type(type_id);
    if (std::erase_if(type.characteristic_contributions, [characteristic](const auto& item) {
            return item.characteristic == characteristic;
        }) == 0) {
        throw std::invalid_argument{"function characteristic contribution does not exist"};
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

void WorldDefinition::remove_calculation(CalculationId id)
{
    const auto calculation = std::ranges::find(calculations_, id, &CalculationDefinition::id);
    if (calculation == calculations_.end()) {
        throw std::out_of_range{"CalculationId is out of range"};
    }
    if (std::ranges::any_of(function_types_, [id](const FunctionTypeDefinition& type) {
            return std::ranges::any_of(type.calculations, [id](const FunctionCalculationBinding& binding) {
                return binding.calculation == id;
            });
        })) {
        throw std::invalid_argument{"calculation is referenced by a function type"};
    }
    calculations_.erase(calculation);
}

void WorldDefinition::remove_calculation_input(CalculationId calculation_id, CalculationPortId input)
{
    if (std::ranges::any_of(function_types_, [calculation_id, input](const FunctionTypeDefinition& type) {
            return std::ranges::any_of(type.calculations, [calculation_id, input](const auto& binding) {
                return binding.calculation == calculation_id &&
                       std::ranges::any_of(binding.inputs, [input](const auto& entry) {
                           return entry.input == input;
                       });
            });
        })) {
        throw std::invalid_argument{"calculation input is referenced by a function type"};
    }
    CalculationDefinition candidate = calculation(calculation_id);
    const auto found = std::ranges::find(candidate.inputs, input, &CalculationInputDefinition::id);
    if (found == candidate.inputs.end()) {
        throw std::invalid_argument{"calculation input port does not exist"};
    }
    candidate.inputs.erase(found);
    recompile_calculation_outputs(candidate);
    mutable_calculation(calculation_id) = std::move(candidate);
}

void WorldDefinition::remove_calculation_output(CalculationId calculation_id, CalculationPortId output)
{
    for (const FunctionTypeDefinition& type : function_types_) {
        const auto references = [calculation_id, output](const FunctionValueSource& source) {
            return references_calculation_output(source, calculation_id, output);
        };
        if ((type.process && (references(type.process->throughput) ||
                              std::ranges::any_of(type.process->outputs, [&references](const auto& item) {
                                  return references(item.allocation);
                              }))) ||
            (type.buffer_process && (references(type.buffer_process->capacity) ||
                                     references(type.buffer_process->throughput) ||
                                     references(type.buffer_process->leakage))) ||
            std::ranges::any_of(type.material_contributions, [&references](const auto& item) {
                return references(item.amount);
            })) {
            throw std::invalid_argument{"calculation output is referenced by a function type"};
        }
    }

    CalculationDefinition candidate = calculation(calculation_id);
    const auto found = std::ranges::find(candidate.outputs, output, &CalculationOutputDefinition::id);
    if (found == candidate.outputs.end()) {
        throw std::invalid_argument{"calculation output port does not exist"};
    }
    candidate.outputs.erase(found);
    recompile_calculation_outputs(candidate);
    mutable_calculation(calculation_id) = std::move(candidate);
}

void WorldDefinition::set_calculation_output_expression(CalculationId calculation_id, CalculationPortId output,
                                                         std::string_view expression_source)
{
    CalculationDefinition candidate = calculation(calculation_id);
    const auto found = std::ranges::find(candidate.outputs, output, &CalculationOutputDefinition::id);
    if (found == candidate.outputs.end()) {
        throw std::invalid_argument{"calculation output port does not exist"};
    }
    found->expression_source = std::string{expression_source};
    recompile_calculation_outputs(candidate);
    mutable_calculation(calculation_id) = std::move(candidate);
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

void WorldDefinition::set_object_construction(ObjectConstructionDefinition construction)
{
    validate_object_construction(construction);
    object_construction_ = std::move(construction);
}

void WorldDefinition::remove_object_construction()
{
    if (!object_construction_) {
        throw std::invalid_argument{"object construction does not exist"};
    }
    object_construction_.reset();
}

const std::vector<ValueDefinition>& WorldDefinition::values() const noexcept { return values_; }
const std::vector<UnitDefinition>& WorldDefinition::units() const noexcept { return units_; }
const std::vector<UnitConversionDefinition>& WorldDefinition::unit_conversions() const noexcept
{
    return unit_conversions_;
}
const std::vector<ObjectCharacteristicDefinition>& WorldDefinition::object_characteristics() const noexcept
{
    return object_characteristics_;
}
const std::optional<ObjectConstructionDefinition>& WorldDefinition::object_construction() const noexcept
{
    return object_construction_;
}

const UnitConversionDefinition& WorldDefinition::unit_conversion(UnitConversionId id) const
{
    const auto found = std::ranges::find(unit_conversions_, id, &UnitConversionDefinition::id);
    if (found == unit_conversions_.end()) {
        throw std::invalid_argument{"unknown UnitConversionId"};
    }
    return *found;
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

const ObjectCharacteristicDefinition& WorldDefinition::object_characteristic(ObjectCharacteristicId id) const
{
    const auto found = std::ranges::find(object_characteristics_, id, &ObjectCharacteristicDefinition::id);
    if (found == object_characteristics_.end()) {
        throw std::invalid_argument{"unknown ObjectCharacteristicId"};
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
        .object_characteristics = object_characteristics_,
        .templates = templates_,
        .world_rules = world_rules_,
        .object_construction = object_construction_,
        .next_value_key = next_value_key_,
        .next_template_id = next_template_id_,
        .next_function_type_id = next_function_type_id_,
        .next_parameter_id = next_parameter_id_,
        .next_calculation_id = next_calculation_id_,
        .next_calculation_port_id = next_calculation_port_id_,
        .next_unit_id = next_unit_id_,
        .next_unit_conversion_id = next_unit_conversion_id_,
        .next_object_characteristic_id = next_object_characteristic_id_,
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
            .calculations = type.calculations,
            .process = type.process,
            .buffer_process = type.buffer_process,
            .material_contributions = type.material_contributions,
            .characteristic_contributions = type.characteristic_contributions,
        };
        result.function_types.push_back(std::move(stored));
    }
    return result;
}

WorldDefinition WorldDefinition::from_snapshot(const WorldDefinitionSnapshot& source)
{
    if (source.schema_version != 7) {
        throw std::invalid_argument{"unsupported WorldDefinition snapshot schema version"};
    }
    require_unique_snapshot_ids(source.values, &ValueDefinition::key, "ValueKey");
    require_unique_snapshot_names(source.values, "value");
    require_unique_snapshot_ids(source.units, &UnitDefinition::id, "UnitId");
    for (std::size_t index = 0; index < source.units.size(); ++index) {
        require_name(source.units[index].symbol, "unit");
        for (std::size_t other = 0; other < index; ++other) {
            if (source.units[other].symbol == source.units[index].symbol) {
                throw std::invalid_argument{"unit symbols must be unique"};
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
    restored.units_ = source.units;
    require_unique_snapshot_ids(source.object_characteristics, &ObjectCharacteristicDefinition::id,
                                "ObjectCharacteristicId");
    require_unique_snapshot_names(source.object_characteristics, "object characteristic");
    restored.object_characteristics_ = source.object_characteristics;
    for (const ValueDefinition& value : restored.values_) {
        if (value.unit) {
            restored.validate_unit_expression(*value.unit);
        }
    }
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
            const Expression expression = compile_expression(output.expression_source,
                                                             calculation_expression_names(calculation));
            port_ids.push_back(output.id);
            calculation.outputs.push_back({.id = output.id, .name = output.name,
                                           .expression_source = output.expression_source,
                                           .expression = expression});
        }
        restored.calculations_.push_back(std::move(calculation));
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
        for (const FunctionCalculationBinding& binding : stored.calculations) {
            if (std::ranges::any_of(type.calculations, [binding](const auto& existing) {
                    return existing.calculation == binding.calculation;
                })) {
                throw std::invalid_argument{"calculation may be bound only once per function type"};
            }
            restored.validate_function_calculation_binding(type, binding);
            type.calculations.push_back(binding);
        }
        if (stored.process && stored.buffer_process) {
            throw std::invalid_argument{"function type cannot have both conversion and buffer processes"};
        }
        if (stored.process) {
            (void)restored.value(stored.process->input);
            (void)restored.unit_conversion(stored.process->conversion);
            restored.validate_function_value_source(type, stored.process->throughput);
            if (stored.process->outputs.empty()) {
                throw std::invalid_argument{"function process must contain at least one output"};
            }
            for (std::size_t output_index = 0; output_index < stored.process->outputs.size(); ++output_index) {
                const FunctionProcessOutputDefinition& output = stored.process->outputs[output_index];
                (void)restored.value(output.output);
                restored.validate_function_value_source(type, output.allocation);
                for (std::size_t previous = 0; previous < output_index; ++previous) {
                    if (stored.process->outputs[previous].output == output.output) {
                        throw std::invalid_argument{"function process output ValueKey is duplicated"};
                    }
                }
            }
            type.process = stored.process;
        }
        if (stored.buffer_process) {
            (void)restored.value(stored.buffer_process->value);
            restored.validate_function_value_source(type, stored.buffer_process->capacity);
            restored.validate_function_value_source(type, stored.buffer_process->throughput);
            restored.validate_function_value_source(type, stored.buffer_process->leakage);
            type.buffer_process = stored.buffer_process;
        }
        for (const MaterialContributionDefinition& contribution : stored.material_contributions) {
            (void)restored.value(contribution.value);
            if (std::ranges::any_of(type.material_contributions, [&contribution](const auto& existing) {
                    return existing.value == contribution.value;
                })) {
                throw std::invalid_argument{"function type has more than one contribution for a material value"};
            }
            restored.validate_function_value_source(type, contribution.amount);
            type.material_contributions.push_back(contribution);
        }
        for (const FunctionCharacteristicContributionDefinition& contribution : stored.characteristic_contributions) {
            (void)restored.object_characteristic(contribution.characteristic);
            if (std::ranges::any_of(type.characteristic_contributions, [&contribution](const auto& existing) {
                    return existing.characteristic == contribution.characteristic;
                })) {
                throw std::invalid_argument{"function type has duplicate characteristic contribution"};
            }
            restored.validate_function_value_source(type, contribution.amount);
            type.characteristic_contributions.push_back(contribution);
        }
        restored.function_types_.push_back(std::move(type));
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
        for (const BaseObjectCharacteristicDefinition& base : stored.base_characteristics) {
            (void)restored.object_characteristic(base.characteristic);
            if (!std::isfinite(base.amount) || std::ranges::any_of(object.base_characteristics, [&base](const auto& item) {
                    return item.characteristic == base.characteristic;
                })) {
                throw std::invalid_argument{"template base characteristic must be finite and unique"};
            }
            object.base_characteristics.push_back(base);
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
    if (source.object_construction) {
        restored.validate_object_construction(*source.object_construction);
        restored.object_construction_ = source.object_construction;
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
    require_next_id_is_unused(source.next_unit_id, source.units, &UnitDefinition::id, "UnitId");
    require_next_id_is_unused(source.next_unit_conversion_id, source.unit_conversions,
                              &UnitConversionDefinition::id, "UnitConversionId");
    require_next_id_is_unused(source.next_object_characteristic_id, source.object_characteristics,
                              &ObjectCharacteristicDefinition::id, "ObjectCharacteristicId");
    restored.next_value_key_ = source.next_value_key;
    restored.next_template_id_ = source.next_template_id;
    restored.next_function_type_id_ = source.next_function_type_id;
    restored.next_parameter_id_ = source.next_parameter_id;
    restored.next_calculation_id_ = source.next_calculation_id;
    restored.next_calculation_port_id_ = source.next_calculation_port_id;
    restored.next_unit_id_ = source.next_unit_id;
    restored.next_unit_conversion_id_ = source.next_unit_conversion_id;
    restored.next_object_characteristic_id_ = source.next_object_characteristic_id;
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
                               [parameter](const GenomeParameterDefinition& item) { return item.id == parameter; });
}

void WorldDefinition::validate_function_value_source(const FunctionTypeDefinition& type,
                                                     const FunctionValueSource& source) const
{
    if (source.kind == FunctionValueSourceKind::genome_parameter) {
        if (!parameter_belongs_to(type, source.genome_parameter)) {
            throw std::invalid_argument{"function value source genome parameter does not belong to function type"};
        }
        return;
    }
    if (source.kind != FunctionValueSourceKind::calculation_output) {
        throw std::invalid_argument{"function value source kind is invalid"};
    }
    const auto binding = std::ranges::find(type.calculations, source.calculation,
                                           &FunctionCalculationBinding::calculation);
    if (binding == type.calculations.end()) {
        throw std::invalid_argument{"function value source calculation is not bound to function type"};
    }
    const CalculationDefinition& calculation_definition = calculation(source.calculation);
    if (std::ranges::none_of(calculation_definition.outputs, [source](const CalculationOutputDefinition& output) {
            return output.id == source.calculation_output;
        })) {
        throw std::invalid_argument{"function value source port is not an output of its calculation"};
    }
}

void WorldDefinition::validate_function_calculation_binding(const FunctionTypeDefinition& type,
                                                            const FunctionCalculationBinding& binding) const
{
    const CalculationDefinition& calculation_definition = calculation(binding.calculation);
    if (binding.inputs.size() != calculation_definition.inputs.size()) {
        throw std::invalid_argument{"calculation binding must bind every input exactly once"};
    }
    for (const CalculationInputDefinition& input : calculation_definition.inputs) {
        const auto count = std::ranges::count(binding.inputs, input.id, &FunctionCalculationInputBinding::input);
        if (count != 1) {
            throw std::invalid_argument{"calculation binding must bind every input exactly once"};
        }
    }
    for (const FunctionCalculationInputBinding& input : binding.inputs) {
        if (std::ranges::none_of(calculation_definition.inputs, [input](const CalculationInputDefinition& definition) {
                return definition.id == input.input;
            })) {
            throw std::invalid_argument{"calculation binding contains an unknown input port"};
        }
        if (!parameter_belongs_to(type, input.genome_parameter)) {
            throw std::invalid_argument{"calculation input genome parameter does not belong to function type"};
        }
    }
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
    if (binding.source_kind == HostBinding::SourceKind::value) {
        (void)value(binding.value);
    } else if (binding.source_kind == HostBinding::SourceKind::object_characteristic) {
        if (binding.direction != HostChannelDirection::output) {
            throw std::invalid_argument{"host input binding must target a runtime value"};
        }
        (void)object_characteristic(binding.characteristic);
    } else {
        throw std::invalid_argument{"host binding source kind is invalid"};
    }
    for (std::size_t index = 0; index < object.host_bindings.size(); ++index) {
        const HostBinding& existing = object.host_bindings[index];
        if (index != ignored_index && existing.direction == binding.direction && existing.channel == binding.channel) {
            throw std::invalid_argument{"host channel is bound more than once for its direction"};
        }
        if (index != ignored_index && binding.direction == HostChannelDirection::input &&
            existing.direction == HostChannelDirection::input && existing.source_kind == HostBinding::SourceKind::value &&
            binding.source_kind == HostBinding::SourceKind::value && existing.value == binding.value) {
            throw std::invalid_argument{"world value has more than one host input binding"};
        }
    }
}

void WorldDefinition::validate_object_construction(const ObjectConstructionDefinition& construction) const
{
    const CalculationDefinition& calculation_definition = calculation(construction.calculation);
    if (construction.outputs.empty() || construction.inputs.size() != calculation_definition.inputs.size()) {
        throw std::invalid_argument{"object construction must bind every input and at least one output"};
    }
    for (const CalculationInputDefinition& input : calculation_definition.inputs) {
        if (std::ranges::count(construction.inputs, input.id, &ObjectConstructionInputBinding::input) != 1) {
            throw std::invalid_argument{"object construction must bind every calculation input exactly once"};
        }
    }
    for (const ObjectConstructionInputBinding& binding : construction.inputs) {
        if (std::ranges::none_of(calculation_definition.inputs, [&](const auto& input) { return input.id == binding.input; })) {
            throw std::invalid_argument{"object construction contains an unknown calculation input"};
        }
        if (binding.source.kind != ObjectConstructionSourceKind::base_characteristic &&
            binding.source.kind != ObjectConstructionSourceKind::function_contribution_sum) {
            throw std::invalid_argument{"object construction source kind is invalid"};
        }
        (void)object_characteristic(binding.source.characteristic);
    }
    for (const ObjectConstructionOutputBinding& binding : construction.outputs) {
        if (std::ranges::none_of(calculation_definition.outputs, [&](const auto& output) { return output.id == binding.output; })) {
            throw std::invalid_argument{"object construction contains an unknown calculation output"};
        }
        (void)object_characteristic(binding.characteristic);
        if (std::ranges::count(construction.outputs, binding.characteristic,
                               &ObjectConstructionOutputBinding::characteristic) != 1) {
            throw std::invalid_argument{"object construction output characteristic is duplicated"};
        }
    }
}

} // namespace clife::world
