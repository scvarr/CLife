#include <clife/world/runtime.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace clife::world {

RuntimeWorld::RuntimeWorld(const WorldDefinition& definition)
{
    std::vector<ValueKey> keys;
    keys.reserve(definition.values().size());
    for (const ValueDefinition& entry : definition.values()) {
        keys.push_back(entry.key);
    }
    std::ranges::sort(keys);
    if (keys.empty()) {
        throw std::invalid_argument{"runtime world must contain at least one value"};
    }
    if (keys.size() > static_cast<std::size_t>(std::numeric_limits<ValueIndex>::max())) {
        throw std::invalid_argument{"runtime world contains too many values"};
    }
    value_ids_.reserve(keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
        value_ids_.push_back({keys[index], ValueId{static_cast<ValueIndex>(index)}});
    }

    templates_.reserve(definition.templates().size());
    for (const ObjectTemplate& source : definition.templates()) {
        for (const HostBinding& binding : source.host_bindings) {
            (void)require_value_id(binding.value);
            if (binding.direction == HostChannelDirection::input &&
                std::ranges::any_of(source.genome, [binding](const GenomeFunctionDefinition& function) {
                    return function.output == binding.value;
                })) {
                throw std::invalid_argument{"host input cannot target a genome-produced value"};
            }
        }
        Program program;
        program.value_count = value_ids_.size();
        program.functions.reserve(source.genome.size());
        for (const GenomeFunctionDefinition& function : source.genome) {
            program.functions.push_back({
                .input = require_value_id(function.input),
                .output = require_value_id(function.output),
                .throughput = function.throughput,
                .result_per_input = function.result_per_input,
            });
        }
        program.initial_values.reserve(source.initial_values.size());
        for (const InitialValueDefinition& initial : source.initial_values) {
            program.initial_values.push_back({.value = require_value_id(initial.value), .amount = initial.amount});
        }
        program.end_rules.reserve(definition.world_rules().size());
        for (const WorldRuleDefinition& rule : definition.world_rules()) {
            program.end_rules.push_back({
                .source = require_value_id(rule.source),
                .target = require_value_id(rule.target),
                .target_per_source = rule.target_per_source,
            });
        }

        // Construct once during compilation so invalid cross-definition combinations fail here.
        const Calculator validation{program};
        (void)validation;
        templates_.push_back({.source = source.id, .program = std::move(program), .bindings = source.host_bindings});
    }
    std::ranges::sort(templates_, {}, &CompiledTemplate::source);
}

ObjectId RuntimeWorld::instantiate(TemplateId source_template_id)
{
    const CompiledTemplate& source = compiled_template(source_template_id);
    if (next_object_id_ == 0) {
        throw std::overflow_error{"ObjectId space exhausted"};
    }
    const ObjectId id{next_object_id_++};
    objects_.push_back({
        .id = id,
        .source = source.source,
        .calculator = Calculator{source.program},
        .bindings = source.bindings,
        .staged_inputs = {},
    });
    return id;
}

void RuntimeWorld::set_input(ObjectId object_id, ValueKey key, Amount amount)
{
    RuntimeObject& target = object(object_id);
    if (!std::isfinite(amount)) {
        throw std::invalid_argument{"host input must be finite"};
    }
    const bool is_input = std::ranges::any_of(target.bindings, [key](const HostBinding& binding) {
        return binding.direction == HostChannelDirection::input && binding.value == key;
    });
    if (!is_input) {
        throw std::invalid_argument{"ValueKey is not bound as a host input for this object"};
    }
    target.staged_inputs[key] = amount;
}

void RuntimeWorld::set_input(ObjectId object_id, std::string_view channel, Amount amount)
{
    RuntimeObject& target = object(object_id);
    const auto found = std::ranges::find_if(target.bindings, [channel](const HostBinding& binding) {
        return binding.direction == HostChannelDirection::input && binding.channel == channel;
    });
    if (found == target.bindings.end()) {
        throw std::invalid_argument{"unknown host input channel"};
    }
    set_input(object_id, found->value, amount);
}

void RuntimeWorld::step()
{
    for (RuntimeObject& target : objects_) {
        std::vector<ValueAmount> external;
        for (const HostBinding& binding : target.bindings) {
            if (binding.direction != HostChannelDirection::input) {
                continue;
            }
            const auto staged = target.staged_inputs.find(binding.value);
            external.push_back({
                .value = require_value_id(binding.value),
                .amount = staged == target.staged_inputs.end() ? 0.0 : staged->second,
            });
        }
        std::ranges::sort(external, {}, [](const ValueAmount& item) { return item.value.index; });
        const auto duplicate = std::ranges::adjacent_find(external, {}, [](const ValueAmount& item) {
            return item.value.index;
        });
        if (duplicate != external.end()) {
            throw std::invalid_argument{"a host input value is bound more than once"};
        }
        target.calculator.step(external);
        target.staged_inputs.clear();
    }
}

Amount RuntimeWorld::value(ObjectId object_id, ValueKey key) const
{
    return object(object_id).calculator.value(require_value_id(key));
}

Amount RuntimeWorld::output(ObjectId object_id, std::string_view channel) const
{
    const RuntimeObject& target = object(object_id);
    const auto found = std::ranges::find_if(target.bindings, [channel](const HostBinding& binding) {
        return binding.direction == HostChannelDirection::output && binding.channel == channel;
    });
    if (found == target.bindings.end()) {
        throw std::invalid_argument{"unknown host output channel"};
    }
    return target.calculator.value(require_value_id(found->value));
}

TemplateId RuntimeWorld::source_template(ObjectId object_id) const { return object(object_id).source; }

std::optional<ValueId> RuntimeWorld::runtime_value_id(ValueKey key) const noexcept
{
    const auto found = std::ranges::lower_bound(value_ids_, key, {}, &std::pair<ValueKey, ValueId>::first);
    return found != value_ids_.end() && found->first == key ? std::optional<ValueId>{found->second} : std::nullopt;
}

std::size_t RuntimeWorld::object_count() const noexcept { return objects_.size(); }

ValueId RuntimeWorld::require_value_id(ValueKey key) const
{
    const std::optional<ValueId> id = runtime_value_id(key);
    if (!id) {
        throw std::invalid_argument{"ValueKey is not present in the compiled runtime"};
    }
    return *id;
}

const RuntimeWorld::CompiledTemplate& RuntimeWorld::compiled_template(TemplateId id) const
{
    const auto found = std::ranges::lower_bound(templates_, id, {}, &CompiledTemplate::source);
    if (found == templates_.end() || found->source != id) {
        throw std::invalid_argument{"TemplateId is not present in the compiled runtime"};
    }
    return *found;
}

RuntimeWorld::RuntimeObject& RuntimeWorld::object(ObjectId id)
{
    const auto found = std::ranges::find(objects_, id, &RuntimeObject::id);
    if (found == objects_.end()) {
        throw std::invalid_argument{"unknown ObjectId"};
    }
    return *found;
}

const RuntimeWorld::RuntimeObject& RuntimeWorld::object(ObjectId id) const
{
    const auto found = std::ranges::find(objects_, id, &RuntimeObject::id);
    if (found == objects_.end()) {
        throw std::invalid_argument{"unknown ObjectId"};
    }
    return *found;
}

} // namespace clife::world
