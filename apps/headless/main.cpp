#include <clife/core/simulation.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

constexpr clife::Tick kSmokeTestTicks{10};

int run_headless()
{
    clife::Simulation simulation;

    std::cout << "CLife headless: start at tick " << simulation.tick() << '\n';

    for (clife::Tick i = 0; i < kSmokeTestTicks; ++i) {
        simulation.step();
    }

    std::cout << "CLife headless: stop at tick " << simulation.tick() << '\n';
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    try {
        return run_headless();
    } catch (const std::exception& error) {
        std::cerr << "CLife headless fatal error: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "CLife headless fatal error: unknown exception\n";
    }

    return EXIT_FAILURE;
}
