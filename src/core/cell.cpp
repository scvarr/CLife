#include <clife/core/cell.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace clife {
namespace {

[[nodiscard]] const MatterDefinition* find_matter_definition(std::span<const MatterDefinition> definitions,
                                                              MatterType type) noexcept
{
    const auto found = std::find_if(definitions.begin(), definitions.end(), [type](const MatterDefinition& definition) {
        return definition.type == type;
    });

    return found == definitions.end() ? nullptr : &*found;
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

Cell::Cell(CellPhenotype phenotype, std::span<const MatterDefinition> matter_definitions)
    : phenotype_(std::move(phenotype))
{
    for (std::size_t i = 0; i < matter_definitions.size(); ++i) {
        const MatterDefinition& definition = matter_definitions[i];
        if (!std::isfinite(definition.volume_per_unit) || definition.volume_per_unit < 0.0 ||
            !std::isfinite(definition.heat_capacity_per_unit) || definition.heat_capacity_per_unit < 0.0) {
            throw std::invalid_argument{"matter properties must be finite and non-negative"};
        }

        for (std::size_t previous = 0; previous < i; ++previous) {
            if (matter_definitions[previous].type == definition.type) {
                throw std::invalid_argument{"matter type has more than one definition"};
            }
        }
    }

    for (const MatterAmount& component : phenotype_.composition) {
        if (!std::isfinite(component.amount) || component.amount < 0.0) {
            throw std::invalid_argument{"matter amount must be finite and non-negative"};
        }

        const MatterDefinition* definition = find_matter_definition(matter_definitions, component.type);
        if (definition == nullptr) {
            throw std::invalid_argument{"cell composition references undefined matter type"};
        }

        const Amount volume_contribution = component.amount * definition->volume_per_unit;
        const Amount heat_capacity_contribution = component.amount * definition->heat_capacity_per_unit;
        if (!std::isfinite(volume_contribution) || !std::isfinite(heat_capacity_contribution)) {
            throw std::invalid_argument{"derived matter property is not finite"};
        }

        volume_ += volume_contribution;
        heat_capacity_ += heat_capacity_contribution;
        if (!std::isfinite(volume_) || !std::isfinite(heat_capacity_)) {
            throw std::invalid_argument{"derived cell property is not finite"};
        }
    }

    for (const FieldToResourceTransform& transform : phenotype_.transforms) {
        // C0 currently defines 100% efficiency only for the normalized throughput of 1.
        assert(transform.throughput == 1.0);

        if (find_remainder(transform.output) == nullptr) {
            throw std::invalid_argument{"transform output has no remainder state"};
        }
    }

    storage_.reserve(phenotype_.stores.size());
    for (const Store& store_definition : phenotype_.stores) {
        assert(std::isfinite(store_definition.capacity));
        assert(store_definition.capacity >= 0.0);

        storage_.push_back({
            .resource = store_definition.resource,
            .capacity = store_definition.capacity,
        });
    }

    states_.reserve(phenotype_.remainders.size());
    for (std::size_t i = 0; i < phenotype_.remainders.size(); ++i) {
        const RemainderToState& remainder = phenotype_.remainders[i];

        for (std::size_t previous = 0; previous < i; ++previous) {
            if (phenotype_.remainders[previous].resource == remainder.resource) {
                throw std::invalid_argument{"resource has more than one remainder state"};
            }
        }

        if (find_state(remainder.state) == nullptr) {
            states_.push_back({.state = remainder.state});
        }
    }

    // At most one temporary resource entry can be produced by each transform.
    tick_resources_.reserve(phenotype_.transforms.size());
}

void Cell::step(CellInputs inputs) noexcept
{
    tick_resources_.clear();

    for (const FieldToResourceTransform& transform : phenotype_.transforms) {
        const Amount available_input = inputs.field(transform.input);
        assert(std::isfinite(available_input));
        assert(available_input >= 0.0);

        const Amount demand = total_demand(transform.input);
        assert(demand > 0.0);

        const Amount load_fraction = std::min(1.0, available_input / demand);
        const Amount produced = transform.throughput * load_fraction;
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

        const RemainderToState* destination = find_remainder(remaining.resource);
        assert(destination != nullptr);
        add_state(destination->state, remaining.amount);
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

Amount Cell::volume() const noexcept
{
    return volume_;
}

Amount Cell::heat_capacity() const noexcept
{
    return heat_capacity_;
}

Amount Cell::total_demand(FieldType field) const noexcept
{
    Amount demand{0.0};

    for (const FieldToResourceTransform& transform : phenotype_.transforms) {
        if (transform.input == field) {
            demand += transform.throughput;
        }
    }

    return demand;
}

const RemainderToState* Cell::find_remainder(ResourceType resource) const noexcept
{
    const auto found = std::find_if(phenotype_.remainders.begin(), phenotype_.remainders.end(),
                                    [resource](const RemainderToState& remainder) {
                                        return remainder.resource == resource;
                                    });

    return found == phenotype_.remainders.end() ? nullptr : &*found;
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
