#include <clife/core/cell.hpp>
#include <clife/core/simulation.hpp>

#include <cmath>
#include <iostream>

namespace {

bool expect_equal(clife::Tick actual, clife::Tick expected, const char* message)
{
    if (actual == expected) {
        return true;
    }

    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    return false;
}

bool expect_near(clife::ResourceAmount actual, clife::ResourceAmount expected, const char* message)
{
    constexpr clife::ResourceAmount tolerance{1e-12};
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
                .input = clife::Resource::Light,
                .output = clife::Resource::Energy,
                .throughput = 1.0,
            },
        .store =
            {
                .resource = clife::Resource::Energy,
                .capacity = 1.5,
            },
    };

    clife::Cell cell{phenotype};

    cell.step({.light = 0.25});
    if (!expect_near(cell.stored_energy(), 0.25, "stored energy after partial input") ||
        !expect_near(cell.thermal_energy(), 0.0, "thermal energy after partial input")) {
        return false;
    }

    cell.step({.light = 2.0});
    if (!expect_near(cell.stored_energy(), 1.25, "stored energy after throughput-limited input") ||
        !expect_near(cell.thermal_energy(), 0.0, "unprocessed light is not thermalized")) {
        return false;
    }

    cell.step({.light = 1.0});
    if (!expect_near(cell.stored_energy(), 1.5, "stored energy at capacity") ||
        !expect_near(cell.thermal_energy(), 0.75, "unstored produced energy becomes heat")) {
        return false;
    }

    cell.step({.light = 10.0});
    return expect_near(cell.stored_energy(), 1.5, "stored energy remains capped") &&
           expect_near(cell.thermal_energy(), 1.75, "full store thermalizes produced energy");
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
