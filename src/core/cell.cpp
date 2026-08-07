#include <clife/core/cell.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>

namespace clife {

Amount CellInputs::field(FieldType type) const noexcept
{
    const auto index = static_cast<std::size_t>(type.index);
    if (index >= fields.size()) {
        return 0.0;
    }

    return fields[index];
}

Cell::Cell(CellPhenotype phenotype) : phenotype_(std::move(phenotype))
{
    for (const FieldToResourceTransform& transform : phenotype_.transforms) {
        // C0 currently defines 100% efficiency only for the normalized throughput of 1.
        assert(transform.throughput == 1.0);
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

    // Resources not persisted by Store exist only inside the current tick.
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

Cell::TickResource* Cell::find_tick_resource(ResourceType resource) noexcept
{
    const auto found = std::find_if(tick_resources_.begin(), tick_resources_.end(), [resource](const TickResource& entry) {
        return entry.resource == resource;
    });

    return found == tick_resources_.end() ? nullptr : &*found;
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

} // namespace clife
