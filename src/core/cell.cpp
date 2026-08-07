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

    for (const Store& store_definition : phenotype_.stores) {
        assert(std::isfinite(store_definition.capacity));
        assert(store_definition.capacity >= 0.0);

        ResourceStorage* existing = find_storage(store_definition.resource);
        if (existing == nullptr) {
            storage_.push_back({
                .resource = store_definition.resource,
                .capacity = store_definition.capacity,
            });
            continue;
        }

        const Amount combined_capacity = existing->capacity + store_definition.capacity;
        assert(std::isfinite(combined_capacity));
        existing->capacity = combined_capacity;
    }
}

void Cell::step(CellInputs inputs) noexcept
{
    for (const FieldToResourceTransform& transform : phenotype_.transforms) {
        const Amount available_input = inputs.field(transform.input);
        assert(std::isfinite(available_input));
        assert(available_input >= 0.0);

        const Amount demand = total_demand(transform.input);
        assert(demand > 0.0);

        const Amount load_fraction = std::min(1.0, available_input / demand);
        const Amount produced = transform.throughput * load_fraction;
        store(transform.output, produced);
    }
}

Amount Cell::stored(ResourceType resource) const noexcept
{
    const ResourceStorage* storage = find_storage(resource);
    return storage == nullptr ? 0.0 : storage->amount;
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

Cell::ResourceStorage* Cell::find_storage(ResourceType resource) noexcept
{
    const auto found = std::find_if(storage_.begin(), storage_.end(), [resource](const ResourceStorage& storage) {
        return storage.resource == resource;
    });

    return found == storage_.end() ? nullptr : &*found;
}

const Cell::ResourceStorage* Cell::find_storage(ResourceType resource) const noexcept
{
    const auto found = std::find_if(storage_.begin(), storage_.end(), [resource](const ResourceStorage& storage) {
        return storage.resource == resource;
    });

    return found == storage_.end() ? nullptr : &*found;
}

void Cell::store(ResourceType resource, Amount amount) noexcept
{
    ResourceStorage* storage = find_storage(resource);
    if (storage == nullptr) {
        return;
    }

    const Amount free_capacity = storage->capacity - storage->amount;
    storage->amount += std::min(amount, free_capacity);
}

} // namespace clife
