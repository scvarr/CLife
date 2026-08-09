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
        CompiledPhenotype phenotype = compile_phenotype(definition, source.id);
        for (const HostBinding& binding : source.host_bindings) {
            (void)require_value_id(binding.value);
            const bool produced_by_process =
                std::ranges::any_of(source.genome, [&](const GenomeFunctionInstance& function) {
                    const FunctionTypeDefinition& type = definition.function_type(function.type);
                    return type.process && ((type.process->conversion.value == 0 && type.process->output == binding.value) ||
                                            std::ranges::any_of(type.process->outputs,
                                                                [&](const FunctionProcessOutputDefinition& output) {
                                                                    return output.output == binding.value;
                                                                }));
                });
            if (binding.direction == HostChannelDirection::input && produced_by_process) {
                throw std::invalid_argument{"host input cannot target a genome-produced value"};
            }
        }
        Program program;
        program.value_count = value_ids_.size();
        program.functions.reserve(phenotype.functions().size());
        std::vector<std::optional<std::size_t>> buffer_indices(phenotype.functions().size());
        for (std::size_t index = 0; index < phenotype.functions().size(); ++index) {
            const CompiledFunctionPhenotype& function = phenotype.function(index);
            const FunctionTypeDefinition& type = definition.function_type(function.type());
            if (type.process) {
                const CompiledProcessParameters& parameters = *function.process_parameters();
                program.functions.push_back({
                    .input = require_value_id(type.process->input),
                    .throughput = parameters.throughput,
                });
                Function& compiled = program.functions.back();
                for (const CompiledProcessOutput& output : parameters.outputs) {
                    compiled.outputs.push_back({
                        .value = require_value_id(output.output),
                        .result_per_input = output.result_per_input,
                    });
                }
            }
            if (type.buffer_process) {
                const CompiledBufferParameters& parameters = *function.buffer_parameters();
                buffer_indices[index] = program.buffers.size();
                program.buffers.push_back({
                    .value = require_value_id(type.buffer_process->value),
                    .capacity = parameters.capacity,
                    .throughput = parameters.throughput,
                    .leakage = parameters.leakage,
                });
            }
        }
        std::vector<Amount> initial_amounts(program.value_count, 0.0);
        std::vector<bool> has_initial(program.value_count, false);
        for (const InitialValueDefinition& initial : source.initial_values) {
            const ValueId id = require_value_id(initial.value);
            initial_amounts[id.index] += initial.amount;
            has_initial[id.index] = true;
        }
        for (const MaterialAmount& material : phenotype.material_amounts()) {
            const ValueId id = require_value_id(material.value);
            initial_amounts[id.index] += material.amount;
            has_initial[id.index] = true;
        }
        for (std::size_t index = 0; index < initial_amounts.size(); ++index) {
            if (has_initial[index]) {
                program.initial_values.push_back({
                    .value = ValueId{static_cast<ValueIndex>(index)},
                    .amount = initial_amounts[index],
                });
            }
        }
        program.end_buffer_transfers.reserve(definition.world_rules().size());
        program.end_rules.reserve(definition.world_rules().size());
        for (const WorldRuleDefinition& rule : definition.world_rules()) {
            program.end_buffer_transfers.push_back({
                .source = require_value_id(rule.source),
                .target = require_value_id(rule.end_buffer),
            });
            const EndRule compiled_rule{
                .source = require_value_id(rule.end_buffer),
                .target = require_value_id(rule.target),
                .target_per_source = rule.target_per_source,
            };
            if (std::ranges::find(program.end_rules, compiled_rule) == program.end_rules.end()) {
                program.end_rules.push_back(compiled_rule);
            }
        }

        // Construct once during compilation so invalid cross-definition combinations fail here.
        const Calculator validation{program};
        (void)validation;
        templates_.push_back({
            .source = source.id,
            .phenotype = std::move(phenotype),
            .program = std::move(program),
            .buffer_indices = std::move(buffer_indices),
            .bindings = source.host_bindings,
        });
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
        const auto duplicate =
            std::ranges::adjacent_find(external, {}, [](const ValueAmount& item) { return item.value.index; });
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

const CompiledPhenotype& RuntimeWorld::phenotype(ObjectId object_id) const
{
    return compiled_template(object(object_id).source).phenotype;
}

std::vector<RuntimeFunctionState> RuntimeWorld::function_states(ObjectId object_id) const
{
    const RuntimeObject& target = object(object_id);
    const CompiledTemplate& compiled = compiled_template(target.source);
    std::vector<RuntimeFunctionState> result;
    result.reserve(compiled.phenotype.functions().size());
    for (std::size_t index = 0; index < compiled.phenotype.functions().size(); ++index) {
        RuntimeFunctionState state{
            .function_index = index,
            .type = compiled.phenotype.function(index).type(),
        };
        if (compiled.buffer_indices[index]) {
            state.buffer = target.calculator.buffer_state(*compiled.buffer_indices[index]);
        }
        result.push_back(state);
    }
    return result;
}

Amount RuntimeWorld::last_end_value(ObjectId object_id, ValueKey value_key) const
{
    return object(object_id).calculator.end_value(require_value_id(value_key));
}

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
