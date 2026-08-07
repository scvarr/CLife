#include <clife/core/cell.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace clife {

Cell::Cell(CellPhenotype phenotype) noexcept : phenotype_(phenotype)
{
    assert(phenotype_.transform.input == Resource::Light);
    assert(phenotype_.transform.output == Resource::Energy);
    assert(phenotype_.transform.throughput == 1.0);
    assert(phenotype_.store.resource == Resource::Energy);
    assert(std::isfinite(phenotype_.store.capacity));
    assert(phenotype_.store.capacity >= 0.0);
}

void Cell::step(CellInputs inputs) noexcept
{
    assert(std::isfinite(inputs.light));
    assert(inputs.light >= 0.0);

    const ResourceAmount produced_energy = std::min(inputs.light, phenotype_.transform.throughput);
    const ResourceAmount free_capacity = phenotype_.store.capacity - stored_energy_;
    const ResourceAmount stored_now = std::min(produced_energy, free_capacity);

    stored_energy_ += stored_now;
    thermal_energy_ += produced_energy - stored_now;
}

ResourceAmount Cell::stored_energy() const noexcept
{
    return stored_energy_;
}

ResourceAmount Cell::thermal_energy() const noexcept
{
    return thermal_energy_;
}

} // namespace clife
