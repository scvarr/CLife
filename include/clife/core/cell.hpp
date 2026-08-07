#pragma once

#include <cstdint>

namespace clife {

using ResourceAmount = double;

enum class Resource : std::uint8_t {
    Light,
    Energy,
};

struct Transform final {
    Resource input;
    Resource output;
    ResourceAmount throughput;
};

struct Store final {
    Resource resource;
    ResourceAmount capacity;
};

struct CellPhenotype final {
    Transform transform;
    Store store;
};

struct CellInputs final {
    ResourceAmount light{0.0};
};

class Cell final {
public:
    explicit Cell(CellPhenotype phenotype) noexcept;

    void step(CellInputs inputs) noexcept;

    [[nodiscard]] ResourceAmount stored_energy() const noexcept;
    [[nodiscard]] ResourceAmount thermal_energy() const noexcept;

private:
    CellPhenotype phenotype_;
    ResourceAmount stored_energy_{0.0};
    ResourceAmount thermal_energy_{0.0};
};

} // namespace clife
