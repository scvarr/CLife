#pragma once

#include <cstdint>

namespace clife {

using Tick = std::uint64_t;

class Simulation final {
public:
    Simulation() noexcept = default;

    void step() noexcept;

    [[nodiscard]] Tick tick() const noexcept;

private:
    Tick tick_{0};
};

} // namespace clife
