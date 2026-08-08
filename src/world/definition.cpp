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
    if (std::ranges::any_of(values_, [key, &name](const ValueDefinition& entry) {
            return entry.key != key && entry.name == name;
        })) {
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
        if (std::ranges::any_of(object.initial_values, [&](const InitialValueDefinition& item) {
                return referenced(item.value);
            }) ||
            std::ranges::any_of(object.genome, [&](const GenomeFunctionDefinition& item) {
                return referenced(item.input) || referenced(item.output);
            }) ||
            std::ranges::any_of(object.host_bindings, [&](const HostBinding& item) {
                return referenced(item.value);
            })) {
            throw std::invalid_argument{"cannot remove a referenced value"};
        }
    }
    if (std::ranges::any_of(world_rules_, [&](const WorldRuleDefinition& rule) {
            return referenced(rule.source) || referenced(rule.target);
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
    if (std::ranges::any_of(templates_, [id, &name](const ObjectTemplate& entry) {
            return entry.id != id && entry.name == name;
        })) {
        throw std::invalid_argument{"template name must be unique"};
    }
    mutable_template(id).name = std::move(name);
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
    if (std::erase_if(object.initial_values, [key](const InitialValueDefinition& item) { return item.value == key; }) == 0) {
        throw std::invalid_argument{"initial value does not exist"};
    }
}

std::size_t WorldDefinition::add_genome_function(TemplateId id, GenomeFunctionDefinition function)
{
    validate_function(function);
    ObjectTemplate& object = mutable_template(id);
    object.genome.push_back(function);
    return object.genome.size() - 1;
}

void WorldDefinition::change_genome_function(TemplateId id, std::size_t index, GenomeFunctionDefinition function)
{
    validate_function(function);
    ObjectTemplate& object = mutable_template(id);
    if (index >= object.genome.size()) {
        throw std::out_of_range{"genome function index is out of range"};
    }
    object.genome[index] = function;
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

ObjectTemplate& WorldDefinition::mutable_template(TemplateId id)
{
    return const_cast<ObjectTemplate&>(std::as_const(*this).object_template(id));
}

void WorldDefinition::validate_function(const GenomeFunctionDefinition& function) const
{
    (void)value(function.input);
    (void)value(function.output);
    if (!std::isfinite(function.throughput) || function.throughput <= 0.0) {
        throw std::invalid_argument{"genome throughput must be finite and positive"};
    }
    if (!std::isfinite(function.result_per_input) || function.result_per_input < 0.0) {
        throw std::invalid_argument{"genome result_per_input must be finite and non-negative"};
    }
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
