#include <clife/world/definition.hpp>

#include <algorithm>
#include <cmath>
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
            std::ranges::any_of(object.host_bindings,
                                [&](const HostBinding& item) { return referenced(item.value); })) {
            throw std::invalid_argument{"cannot remove a referenced value"};
        }
    }
    if (std::ranges::any_of(world_rules_, [&](const WorldRuleDefinition& rule) {
            return referenced(rule.source) || referenced(rule.target);
        })) {
        throw std::invalid_argument{"cannot remove a referenced value"};
    }
    if (std::ranges::any_of(function_types_, [&](const FunctionTypeDefinition& type) {
            return type.process && (referenced(type.process->input) || referenced(type.process->output));
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
    std::vector<ParameterName> names;
    names.reserve(type.genome_parameters.size() + type.derived_parameters.size());
    for (const GenomeParameterDefinition& parameter : type.genome_parameters) {
        names.push_back({.parameter = parameter.id, .name = parameter.name});
    }
    for (const DerivedParameterDefinition& parameter : type.derived_parameters) {
        names.push_back({.parameter = parameter.id, .name = parameter.name});
    }
    Expression expression = compile_expression(expression_source, names);
    if (next_parameter_id_ == 0) {
        throw std::overflow_error{"ParameterId space exhausted"};
    }
    const ParameterId id{next_parameter_id_++};
    type.derived_parameters.push_back({.id = id, .name = std::move(name), .expression = std::move(expression)});
    return id;
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
    if (!parameter_belongs_to(type, process.throughput) || !parameter_belongs_to(type, process.result_per_input)) {
        throw std::invalid_argument{"process parameter does not belong to function type"};
    }
    type.process = process;
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
const std::vector<ObjectTemplate>& WorldDefinition::templates() const noexcept { return templates_; }
const std::vector<FunctionTypeDefinition>& WorldDefinition::function_types() const noexcept { return function_types_; }
const std::vector<WorldRuleDefinition>& WorldDefinition::world_rules() const noexcept { return world_rules_; }

const ValueDefinition& WorldDefinition::value(ValueKey key) const
{
    const auto found = std::ranges::find(values_, key, &ValueDefinition::key);
    if (found == values_.end()) {
        throw std::invalid_argument{"unknown ValueKey"};
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

ObjectTemplate& WorldDefinition::mutable_template(TemplateId id)
{
    return const_cast<ObjectTemplate&>(std::as_const(*this).object_template(id));
}

FunctionTypeDefinition& WorldDefinition::mutable_function_type(FunctionTypeId id)
{
    return const_cast<FunctionTypeDefinition&>(std::as_const(*this).function_type(id));
}

bool WorldDefinition::parameter_belongs_to(const FunctionTypeDefinition& type, ParameterId parameter) const noexcept
{
    return std::ranges::any_of(type.genome_parameters,
                               [parameter](const GenomeParameterDefinition& item) { return item.id == parameter; }) ||
           std::ranges::any_of(type.derived_parameters,
                               [parameter](const DerivedParameterDefinition& item) { return item.id == parameter; });
}

void WorldDefinition::validate_rule(const WorldRuleDefinition& rule, std::size_t ignored_index) const
{
    (void)value(rule.source);
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
