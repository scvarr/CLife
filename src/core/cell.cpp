#include <clife/core/cell.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace clife {
namespace {

template <typename Definition, typename Type>
[[nodiscard]] const Definition* find_definition(std::span<const Definition> definitions, Type type) noexcept
{
    const auto found = std::find_if(definitions.begin(), definitions.end(), [type](const Definition& definition) {
        return definition.type == type;
    });

    return found == definitions.end() ? nullptr : &*found;
}

template <typename Definition>
void validate_definitions(std::span<const Definition> definitions)
{
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const Definition& definition = definitions[i];
        if (!std::isfinite(definition.unit.measure_per_unit) || definition.unit.measure_per_unit <= 0.0) {
            throw std::invalid_argument{"type measure_per_unit must be finite and positive"};
        }

        for (std::size_t previous = 0; previous < i; ++previous) {
            if (definitions[previous].type == definition.type) {
                throw std::invalid_argument{"type has more than one definition"};
            }
        }

        for (std::size_t property_index = 0; property_index < definition.properties.size(); ++property_index) {
            const TypeProperty& property = definition.properties[property_index];
            if (!std::isfinite(property.value)) {
                throw std::invalid_argument{"type property must be finite"};
            }

            for (std::size_t previous = 0; previous < property_index; ++previous) {
                if (definition.properties[previous].property == property.property) {
                    throw std::invalid_argument{"type has more than one value for a property"};
                }
            }
        }
    }
}

[[nodiscard]] bool has_remainder(const CellPhenotype& phenotype, ResourceType resource) noexcept
{
    return std::find_if(phenotype.remainders.begin(), phenotype.remainders.end(),
                        [resource](const RemainderToState& remainder) {
                            return remainder.resource == resource;
                        }) != phenotype.remainders.end();
}

} // namespace

Amount CellInputs::field(FieldType type) const noexcept
{
    const auto index = static_cast<std::size_t>(type.index);
    if (index >= fields.size()) {
        return 0.0;
    }

    return fields[index];
}

Cell::Cell(CellPhenotype phenotype, TypeDefinitions definitions) : phenotype_(std::move(phenotype))
{
    validate_definitions(definitions.fields);
    validate_definitions(definitions.resources);
    validate_definitions(definitions.states);
    validate_definitions(definitions.matters);

    for (const MatterAmount& component : phenotype_.composition) {
        if (!std::isfinite(component.amount) || component.amount < 0.0) {
            throw std::invalid_argument{"matter amount must be finite and non-negative"};
        }

        if (find_definition(definitions.matters, component.type) == nullptr) {
            throw std::invalid_argument{"cell composition references undefined matter type"};
        }
    }

    remainders_.reserve(phenotype_.remainders.size());
    states_.reserve(phenotype_.remainders.size());
    for (std::size_t i = 0; i < phenotype_.remainders.size(); ++i) {
        const RemainderToState& remainder = phenotype_.remainders[i];

        for (std::size_t previous = 0; previous < i; ++previous) {
            if (phenotype_.remainders[previous].resource == remainder.resource) {
                throw std::invalid_argument{"resource has more than one remainder state"};
            }
        }

        const ResourceDefinition* resource_definition = find_definition(definitions.resources, remainder.resource);
        const StateDefinition* state_definition = find_definition(definitions.states, remainder.state);
        if (resource_definition == nullptr || state_definition == nullptr) {
            throw std::invalid_argument{"remainder references undefined resource or state type"};
        }
        if (resource_definition->unit.measure != state_definition->unit.measure) {
            throw std::invalid_argument{"remainder resource and state use different measures"};
        }

        const Amount state_per_resource =
            resource_definition->unit.measure_per_unit / state_definition->unit.measure_per_unit;
        if (!std::isfinite(state_per_resource)) {
            throw std::invalid_argument{"remainder unit conversion is not finite"};
        }

        remainders_.push_back({
            .resource = remainder.resource,
            .state = remainder.state,
            .state_per_resource = state_per_resource,
        });

        if (find_state(remainder.state) == nullptr) {
            states_.push_back({.state = remainder.state});
        }
    }

    transforms_.reserve(phenotype_.transforms.size());
    for (const FieldToResourceTransform& transform : phenotype_.transforms) {
        if (!std::isfinite(transform.throughput) || transform.throughput <= 0.0) {
            throw std::invalid_argument{"transform throughput must be finite and positive"};
        }
        if (!has_remainder(phenotype_, transform.output)) {
            throw std::invalid_argument{"transform output has no remainder state"};
        }

        const FieldDefinition* input_definition = find_definition(definitions.fields, transform.input);
        const ResourceDefinition* output_definition = find_definition(definitions.resources, transform.output);
        if (input_definition == nullptr || output_definition == nullptr) {
            throw std::invalid_argument{"transform references undefined field or resource type"};
        }
        if (input_definition->unit.measure != output_definition->unit.measure) {
            throw std::invalid_argument{"transform input and output use different measures"};
        }

        const Amount output_per_input =
            input_definition->unit.measure_per_unit / output_definition->unit.measure_per_unit;
        if (!std::isfinite(output_per_input)) {
            throw std::invalid_argument{"transform unit conversion is not finite"};
        }

        transforms_.push_back({
            .input = transform.input,
            .output = transform.output,
            .throughput = transform.throughput,
            .output_per_input = output_per_input,
        });
    }

    storage_.reserve(phenotype_.stores.size());
    for (const Store& store_definition : phenotype_.stores) {
        if (!std::isfinite(store_definition.capacity) || store_definition.capacity < 0.0) {
            throw std::invalid_argument{"store capacity must be finite and non-negative"};
        }
        if (find_definition(definitions.resources, store_definition.resource) == nullptr) {
            throw std::invalid_argument{"store references undefined resource type"};
        }

        storage_.push_back({
            .resource = store_definition.resource,
            .capacity = store_definition.capacity,
        });
    }

    // At most one temporary resource entry can be produced by each transform.
    tick_resources_.reserve(transforms_.size());
}

void Cell::step(CellInputs inputs) noexcept
{
    tick_resources_.clear();

    for (const RuntimeTransform& transform : transforms_) {
        const Amount available_input = inputs.field(transform.input);
        assert(std::isfinite(available_input));
        assert(available_input >= 0.0);

        const Amount demand = total_demand(transform.input);
        assert(std::isfinite(demand));
        assert(demand > 0.0);

        const Amount load_fraction = std::min(1.0, available_input / demand);
        const Amount processed_input = transform.throughput * load_fraction;
        const Amount produced = processed_input * transform.output_per_input;
        assert(std::isfinite(produced));
        produce(transform.output, produced);
    }

    for (ResourceStorage& storage : storage_) {
        const Amount free_capacity = storage.capacity - storage.amount;
        storage.amount += take(storage.resource, free_capacity);
    }

    for (TickResource& remaining : tick_resources_) {
        if (remaining.amount == 0.0) {
            continue;
        }

        const RuntimeRemainder* destination = find_remainder(remaining.resource);
        assert(destination != nullptr);
        const Amount state_amount = remaining.amount * destination->state_per_resource;
        assert(std::isfinite(state_amount));
        add_state(destination->state, state_amount);
        remaining.amount = 0.0;
    }

    tick_resources_.clear();
}

Amount Cell::stored(ResourceType resource) const noexcept
{
    Amount total{0.0};

    for (const ResourceStorage& storage : storage_) {
        if (storage.resource == resource) {
            total += storage.amount;
        }
    }

    return total;
}

Amount Cell::state(StateType state_type) const noexcept
{
    const StateValue* value = find_state(state_type);
    return value == nullptr ? 0.0 : value->amount;
}

Amount Cell::matter(MatterType type) const noexcept
{
    Amount total{0.0};

    for (const MatterAmount& component : phenotype_.composition) {
        if (component.type == type) {
            total += component.amount;
        }
    }

    return total;
}

Amount Cell::total_demand(FieldType field) const noexcept
{
    Amount demand{0.0};

    for (const RuntimeTransform& transform : transforms_) {
        if (transform.input == field) {
            demand += transform.throughput;
        }
    }

    return demand;
}

const Cell::RuntimeRemainder* Cell::find_remainder(ResourceType resource) const noexcept
{
    const auto found = std::find_if(remainders_.begin(), remainders_.end(), [resource](const RuntimeRemainder& remainder) {
        return remainder.resource == resource;
    });

    return found == remainders_.end() ? nullptr : &*found;
}

Cell::TickResource* Cell::find_tick_resource(ResourceType resource) noexcept
{
    const auto found = std::find_if(tick_resources_.begin(), tick_resources_.end(), [resource](const TickResource& entry) {
        return entry.resource == resource;
    });

    return found == tick_resources_.end() ? nullptr : &*found;
}

Cell::StateValue* Cell::find_state(StateType state_type) noexcept
{
    const auto found = std::find_if(states_.begin(), states_.end(), [state_type](const StateValue& value) {
        return value.state == state_type;
    });

    return found == states_.end() ? nullptr : &*found;
}

const Cell::StateValue* Cell::find_state(StateType state_type) const noexcept
{
    const auto found = std::find_if(states_.begin(), states_.end(), [state_type](const StateValue& value) {
        return value.state == state_type;
    });

    return found == states_.end() ? nullptr : &*found;
}

void Cell::produce(ResourceType resource, Amount amount) noexcept
{
    TickResource* existing = find_tick_resource(resource);
    if (existing == nullptr) {
        assert(tick_resources_.size() < tick_resources_.capacity());
        tick_resources_.push_back({.resource = resource, .amount = amount});
        return;
    }

    const Amount combined = existing->amount + amount;
    assert(std::isfinite(combined));
    existing->amount = combined;
}

Amount Cell::take(ResourceType resource, Amount requested) noexcept
{
    TickResource* available = find_tick_resource(resource);
    if (available == nullptr) {
        return 0.0;
    }

    const Amount taken = std::min(available->amount, requested);
    available->amount -= taken;
    return taken;
}

void Cell::add_state(StateType state_type, Amount amount) noexcept
{
    StateValue* value = find_state(state_type);
    assert(value != nullptr);

    const Amount combined = value->amount + amount;
    assert(std::isfinite(combined));
    value->amount = combined;
}

} // namespace clife
