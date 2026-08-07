#include <clife/core/cell.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace clife {

Amount CellInputs::field(FieldType type) const noexcept
{
    const auto index = static_cast<std::size_t>(type.index);
    if (index >= fields.size()) {
        return 0.0;
    }

    return fields[index];
}

Cell::Cell(CellPhenotype phenotype) noexcept : phenotype_(phenotype)
{
    // C0 currently defines 100% efficiency only for the normalized throughput of 1.
    assert(phenotype_.transform.throughput == 1.0);
    assert(phenotype_.transform.output == phenotype_.store.resource);
    assert(std::isfinite(phenotype_.store.capacity));
    assert(phenotype_.store.capacity >= 0.0);
}

void Cell::step(CellInputs inputs) noexcept
{
    const Amount available_input = inputs.field(phenotype_.transform.input);
    assert(std::isfinite(available_input));
    assert(available_input >= 0.0);

    const Amount produced = std::min(available_input, phenotype_.transform.throughput);
    const Amount free_capacity = phenotype_.store.capacity - stored_amount_;
    const Amount stored_now = std::min(produced, free_capacity);

    stored_amount_ += stored_now;
}

Amount Cell::stored(ResourceType resource) const noexcept
{
    if (resource != phenotype_.store.resource) {
        return 0.0;
    }

    return stored_amount_;
}

} // namespace clife
