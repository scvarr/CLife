#include <clife/core/simulation.hpp>

namespace clife {

void Simulation::step() noexcept
{
    ++tick_;
}

Tick Simulation::tick() const noexcept
{
    return tick_;
}

} // namespace clife
