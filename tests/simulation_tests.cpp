#include <clife/core/simulation.hpp>

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

} // namespace

int main()
{
    clife::Simulation simulation;

    if (!expect_equal(simulation.tick(), 0, "new simulation tick")) {
        return 1;
    }

    simulation.step();
    if (!expect_equal(simulation.tick(), 1, "tick after one step")) {
        return 1;
    }

    constexpr clife::Tick additional_steps{99};
    for (clife::Tick i = 0; i < additional_steps; ++i) {
        simulation.step();
    }

    if (!expect_equal(simulation.tick(), 100, "tick after deterministic stepping")) {
        return 1;
    }

    return 0;
}
