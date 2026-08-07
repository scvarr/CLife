#include <clife/core/cell.hpp>
#include <clife/core/simulation.hpp>

#include <array>
#include <cmath>
#include <iostream>

namespace {

constexpr clife::FieldType kSelectedField{1};
constexpr clife::ResourceType kStoredResource{7};
constexpr clife::ResourceType kOtherResource{8};

bool expect_equal(clife::Tick actual, clife::Tick expected, const char* message)
{
    if (actual == expected) {
        return true;
    }

    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    return false;
}

bool expect_near(clife::Amount actual, clife::Amount expected, const char* message)
{
    constexpr clife::Amount tolerance{1e-12};
    if (std::abs(actual - expected) <= tolerance) {
        return true;
    }

    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    return false;
}

bool test_simulation_lifecycle()
{
    clife::Simulation simulation;

    if (!expect_equal(simulation.tick(), 0, "new simulation tick")) {
        return false;
    }

    simulation.step();
    if (!expect_equal(simulation.tick(), 1, "tick after one step")) {
        return false;
    }

    constexpr clife::Tick additional_steps{99};
    for (clife::Tick i = 0; i < additional_steps; ++i) {
        simulation.step();
    }

    return expect_equal(simulation.tick(), 100, "tick after deterministic stepping");
}

bool test_cell_resource_flow()
{
    const clife::CellPhenotype phenotype{
        .transform =
            {
                .input = kSelectedField,
                .output = kStoredResource,
                .throughput = 1.0,
            },
        .store =
            {
                .resource = kStoredResource,
                .capacity = 1.5,
            },
    };

    clife::Cell cell{phenotype};

    std::array<clife::Amount, 2> fields{10.0, 0.25};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kStoredResource), 0.25, "stored resource after partial input") ||
        !expect_near(cell.stored(kOtherResource), 0.0, "unconfigured resource remains absent")) {
        return false;
    }

    fields = {10.0, 2.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kStoredResource), 1.25, "stored resource after throughput-limited input")) {
        return false;
    }

    fields = {10.0, 1.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kStoredResource), 1.5, "stored resource at capacity")) {
        return false;
    }

    fields = {10.0, 10.0};
    cell.step({.fields = fields});
    if (!expect_near(cell.stored(kStoredResource), 1.5, "stored resource remains capped")) {
        return false;
    }

    const std::array<clife::Amount, 1> missing_selected_field{10.0};
    cell.step({.fields = missing_selected_field});
    return expect_near(cell.stored(kStoredResource), 1.5, "missing field type provides zero input");
}

} // namespace

int main()
{
    if (!test_simulation_lifecycle()) {
        return 1;
    }

    if (!test_cell_resource_flow()) {
        return 1;
    }

    return 0;
}
